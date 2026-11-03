#include "queuesync.h"

#include "player.h"
#include "subsonic.h"

#include <QDateTime>
#include <QEventLoop>
#include <QGuiApplication>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

// Songs sent in a URL when the server takes no POST form: about 6 KB of ids, under the 8 KB a
// proxy such as nginx accepts for a request line by default.
constexpr int MaxSongsInUrl = 200;
// While a song plays, its position is saved this often.
constexpr int HeartbeatMs = 30'000;
// Waits this long after a change, so skipping through songs sends one save, not one per song.
constexpr int SettleMs = 2'000;
// The window coming to the front asks the server at most this often.
constexpr qint64 RecheckMs = 10'000;

} // namespace

QueueSync::QueueSync(SubsonicClient *client, Player *player, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_player(player)
{
    m_debounce.setSingleShot(true);
    connect(&m_debounce, &QTimer::timeout, this, &QueueSync::save);
    m_heartbeat.setInterval(HeartbeatMs);
    connect(&m_heartbeat, &QTimer::timeout, this, [this] {
        if (m_player->playing()) {
            m_dirty = true;
            save();
        }
    });
    m_heartbeat.start();

    // A song change also says queueChanged.
    connect(player, &Player::queueChanged, this, [this] { changed(SettleMs); });
    connect(player, &Player::seeked, this, [this] { changed(SettleMs); });
    // Pausing saves the exact place soon.
    connect(player, &Player::playingChanged, this, [this] { changed(m_player->playing() ? SettleMs : 500); });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            activated();
    });
}

void QueueSync::changed(int delayMs)
{
    if (m_applying || !m_client->isConfigured())
        return;
    m_dirty = true;
    m_untouched = false;
    m_debounce.start(delayMs);
}

void QueueSync::save()
{
    m_debounce.stop();
    if (m_saving || !m_dirty || !m_client->isConfigured())
        return;
    m_dirty = false;
    m_saving = true;

    const PlayQueue &queue = m_player->queue();
    const int position = queue.position();
    // Without a POST form a long queue keeps the current song and what follows it.
    int first = 0;
    int end = queue.size();
    if (!m_client->formPost() && end > MaxSongsInUrl) {
        first = std::max(0, position);
        end = std::min(end, first + MaxSongsInUrl);
    }
    QStringList ids;
    ids.reserve(end - first);
    for (int i = first; i < end; ++i)
        ids.append(queue.at(i).song.id);
    const QString current = queue.current() ? queue.current()->song.id : QString();

    m_client->savePlayQueue(ids, current, qRound64(m_player->position() * 1000),
                            [this, generation = m_generation](const QString &error) {
        m_saving = false;
        if (generation == m_generation) {
            if (!error.isEmpty()) {
                // Tried again with the next change or heartbeat, not at once.
                qWarning("legato: savePlayQueue: %s", qPrintable(error));
                m_dirty = true;
            } else if (m_dirty && !m_debounce.isActive()) {
                // Changed while this one was on the way.
                save();
            }
        }
        emit saveFinished();
    });
}

void QueueSync::flush()
{
    // A song playing has moved on since the last heartbeat.
    if (m_player->playing())
        m_dirty = true;
    save();
}

void QueueSync::flushAndWait(int ms)
{
    flush();
    if (!m_saving)
        return;
    QEventLoop loop;
    connect(this, &QueueSync::saveFinished, &loop, &QEventLoop::quit);
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

void QueueSync::serverChanged()
{
    ++m_generation;
    m_debounce.stop();
    // The changes belonged to the previous server.
    m_dirty = false;
    m_untouched = true;
    m_client->setFormPost(false);
    if (!m_client->isConfigured())
        return;
    m_client->extensions([this, generation = m_generation](const QStringList &names, const QString &) {
        if (generation == m_generation)
            m_client->setFormPost(names.contains("formPost"_L1));
    });
    m_lastCheck = QDateTime::currentMSecsSinceEpoch();
    restore();
}

void QueueSync::activated()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_client->isConfigured() || !m_untouched || now - m_lastCheck < RecheckMs)
        return;
    m_lastCheck = now;
    restore();
}

void QueueSync::restore()
{
    m_client->playQueue([this, generation = m_generation](const SavedQueue &saved, const QString &error) {
        // Something was played or changed here meanwhile, or nothing was saved (some servers answer
        // that with an error).
        if (generation != m_generation || !error.isEmpty() || saved.songs.isEmpty() || !m_untouched)
            return;
        const auto at = std::find_if(saved.songs.cbegin(), saved.songs.cend(),
                                     [&saved](const Song &song) { return song.id == saved.current; });
        const int index = at == saved.songs.cend() ? 0 : int(at - saved.songs.cbegin());
        // The same queue at the same place: the player keeps what it shows.
        const PlayQueue &queue = m_player->queue();
        bool same = queue.size() == saved.songs.size() && queue.position() == index
            && qAbs(m_player->position() - saved.positionMs / 1000.0) < 1;
        for (int i = 0; same && i < queue.size(); ++i)
            same = queue.at(i).song.id == saved.songs.at(i).id;
        if (same)
            return;
        QList<QUrl> urls;
        urls.reserve(saved.songs.size());
        for (const Song &song : saved.songs)
            urls.append(m_client->streamUrl(song.id));
        m_applying = true;
        m_player->restore(saved.songs, urls, index, saved.positionMs / 1000.0);
        m_applying = false;
    });
}

} // namespace legato

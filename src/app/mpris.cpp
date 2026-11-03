#include "mpris.h"

#include "coverstore.h"
#include "player.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QPointer>
#include <QUrl>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

constexpr auto ObjectPath = "/org/mpris/MediaPlayer2"_L1;
constexpr auto PlayerInterface = "org.mpris.MediaPlayer2.Player"_L1;
// The size Player loads for its palette, so the file is usually there already.
constexpr int ArtSize = 300;

qlonglong micros(double seconds)
{
    return qlonglong(seconds * 1e6);
}

} // namespace

Mpris::Mpris(Player *player, CoverStore *covers, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_covers(covers)
{
    new MprisRoot(this);
    m_adaptor = new MprisPlayer(this);

    for (auto signal : {&Player::playingChanged, &Player::durationChanged, &Player::volumeChanged,
                        &Player::shuffleChanged, &Player::repeatChanged})
        connect(player, signal, this, &Mpris::schedule);
    connect(player, &Player::songChanged, this, [this] {
        ++m_track;
        updateArt();
        schedule();
    });
    connect(player, &Player::seeked, this, [this] { emit m_adaptor->Seeked(micros(m_player->position())); });

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.registerObject(ObjectPath, this))
        return;
    // A second legato takes a name of its own, as the spec asks.
    const QString name = u"org.mpris.MediaPlayer2.legato"_s;
    const QString instance = name + ".instance"_L1 + QString::number(QCoreApplication::applicationPid());
    for (const QString &candidate : {name, instance}) {
        if (bus.registerService(candidate)) {
            m_service = candidate;
            break;
        }
    }
    // The starting values, so later flushes send only what changed.
    m_metadata = buildMetadata();
    flush();
}

Mpris::~Mpris()
{
    if (registered())
        QDBusConnection::sessionBus().unregisterService(m_service);
}

QDBusObjectPath Mpris::trackId() const
{
    if (!m_player->hasSong())
        return QDBusObjectPath(u"/org/mpris/MediaPlayer2/TrackList/NoTrack"_s);
    return QDBusObjectPath(u"/io/github/eeegoloauq/legato/track/%1"_s.arg(m_track));
}

QString Mpris::playbackStatus() const
{
    if (!m_player->hasSong())
        return u"Stopped"_s;
    return m_player->playing() ? u"Playing"_s : u"Paused"_s;
}

QVariantMap Mpris::buildMetadata() const
{
    QVariantMap out{{u"mpris:trackid"_s, QVariant::fromValue(trackId())}};
    if (!m_player->hasSong())
        return out;
    QStringList artists;
    for (const QVariant &artist : m_player->artists())
        artists.append(artist.toMap().value(u"name"_s).toString());
    if (artists.isEmpty() && !m_player->artist().isEmpty())
        artists.append(m_player->artist());
    out.insert(u"xesam:title"_s, m_player->title());
    out.insert(u"xesam:artist"_s, artists);
    out.insert(u"xesam:album"_s, m_player->album());
    if (m_player->duration() > 0)
        out.insert(u"mpris:length"_s, micros(m_player->duration()));
    if (!m_artUrl.isEmpty())
        out.insert(u"mpris:artUrl"_s, m_artUrl);
    return out;
}

void Mpris::updateArt()
{
    const QString cover = m_player->coverArt();
    if (cover == m_artFor)
        return;
    m_artFor = cover;
    m_artUrl.clear();
    if (cover.isEmpty() || !m_covers)
        return;
    const QString file = m_covers->cachedFile(cover, ArtSize);
    if (!file.isEmpty()) {
        m_artUrl = QUrl::fromLocalFile(file).toString();
        return;
    }
    // Not on disk yet: the song goes out without a picture, which follows when it arrives.
    QPointer<Mpris> self(this);
    m_covers->load(cover, ArtSize, nullptr, [self, cover](const QByteArray &, const QString &error) {
        if (!error.isEmpty())
            return;
        QMetaObject::invokeMethod(self, [self, cover] {
            if (!self || self->m_artFor != cover)
                return;
            const QString file = self->m_covers->cachedFile(cover, ArtSize);
            if (!file.isEmpty()) {
                self->m_artUrl = QUrl::fromLocalFile(file).toString();
                self->schedule();
            }
        }, Qt::QueuedConnection);
    });
}

void Mpris::schedule()
{
    if (m_flushQueued || !registered())
        return;
    m_flushQueued = true;
    QMetaObject::invokeMethod(this, &Mpris::flush, Qt::QueuedConnection);
}

void Mpris::flush()
{
    m_flushQueued = false;
    const bool hasSong = m_player->hasSong();
    const QVariantMap now{
        {u"PlaybackStatus"_s, playbackStatus()},
        {u"LoopStatus"_s, m_adaptor->loopStatus()},
        {u"Shuffle"_s, m_player->shuffle()},
        {u"Volume"_s, m_adaptor->volume()},
        {u"CanGoNext"_s, hasSong},
        {u"CanGoPrevious"_s, hasSong},
        {u"CanPlay"_s, hasSong},
        {u"CanPause"_s, hasSong},
        {u"CanSeek"_s, hasSong},
    };
    QVariantMap changed;
    for (auto it = now.cbegin(); it != now.cend(); ++it) {
        if (m_props.value(it.key()) != it.value())
            changed.insert(it.key(), it.value());
    }
    m_props = now;
    const QVariantMap metadata = buildMetadata();
    if (metadata != m_metadata) {
        m_metadata = metadata;
        changed.insert(u"Metadata"_s, metadata);
    }
    if (changed.isEmpty() || !registered())
        return;
    QDBusMessage signal = QDBusMessage::createSignal(ObjectPath, u"org.freedesktop.DBus.Properties"_s,
                                                     u"PropertiesChanged"_s);
    signal << QString(PlayerInterface) << changed << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

MprisRoot::MprisRoot(Mpris *mpris)
    : QDBusAbstractAdaptor(mpris)
    , m_mpris(mpris)
{
}

QString MprisRoot::identity() const
{
    return QGuiApplication::applicationDisplayName();
}

QString MprisRoot::desktopEntry() const
{
    return QGuiApplication::desktopFileName();
}

void MprisRoot::Raise()
{
    emit m_mpris->raiseRequested();
}

void MprisRoot::Quit()
{
    QCoreApplication::quit();
}

MprisPlayer::MprisPlayer(Mpris *mpris)
    : QDBusAbstractAdaptor(mpris)
    , m_mpris(mpris)
{
}

QString MprisPlayer::loopStatus() const
{
    switch (m_mpris->player()->repeat()) {
    case 1:
        return u"Playlist"_s;
    case 2:
        return u"Track"_s;
    default:
        return u"None"_s;
    }
}

void MprisPlayer::setLoopStatus(const QString &status)
{
    if (status == "None"_L1)
        m_mpris->player()->setRepeat(0);
    else if (status == "Playlist"_L1)
        m_mpris->player()->setRepeat(1);
    else if (status == "Track"_L1)
        m_mpris->player()->setRepeat(2);
}

bool MprisPlayer::shuffle() const
{
    return m_mpris->player()->shuffle();
}

void MprisPlayer::setShuffle(bool on)
{
    m_mpris->player()->setShuffle(on);
}

double MprisPlayer::volume() const
{
    return m_mpris->player()->volume() / 100.0;
}

void MprisPlayer::setVolume(double volume)
{
    m_mpris->player()->setVolume(qRound(std::clamp(volume, 0.0, 1.0) * 100));
}

qlonglong MprisPlayer::position() const
{
    return micros(m_mpris->player()->position());
}

bool MprisPlayer::hasSong() const
{
    return m_mpris->player()->hasSong();
}

void MprisPlayer::Next()
{
    m_mpris->player()->next();
}

void MprisPlayer::Previous()
{
    m_mpris->player()->previous();
}

void MprisPlayer::Pause()
{
    if (m_mpris->player()->playing())
        m_mpris->player()->togglePause();
}

void MprisPlayer::PlayPause()
{
    m_mpris->player()->togglePause();
}

// legato has no stopped state with a song: Stop pauses and goes back to the start.
void MprisPlayer::Stop()
{
    Pause();
    m_mpris->player()->seek(0);
}

void MprisPlayer::Play()
{
    if (!m_mpris->player()->playing())
        m_mpris->player()->togglePause();
}

void MprisPlayer::Seek(qlonglong offset)
{
    Player *player = m_mpris->player();
    const double target = player->position() + offset / 1e6;
    // Past the end is the next song, as the spec says.
    if (player->duration() > 0 && target >= player->duration())
        player->next();
    else
        player->seek(std::max(0.0, target));
}

void MprisPlayer::SetPosition(const QDBusObjectPath &trackId, qlonglong position)
{
    Player *player = m_mpris->player();
    const double seconds = position / 1e6;
    // An old track's id or a position outside the song is ignored, as the spec says.
    if (trackId != m_mpris->trackId() || seconds < 0 || seconds > player->duration())
        return;
    player->seek(seconds);
}

} // namespace legato

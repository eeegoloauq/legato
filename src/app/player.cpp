#include "player.h"

#include "automix.h"
#include "coverprovider.h"
#include "coverstore.h"
#include "mixanalyzer.h"
#include "models.h"
#include "mpvoptions.h"
#include "palette.h"

#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>
#include <cmath>
#include <memory>

#include <mpv/client.h>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

// Reply ids of the observed properties; events are matched by name.
enum Observed : uint64_t { Pause = 1, TimePos, Duration, Path, IdleActive };
// Reply id of the bass swap's filter commands, whose failures are logged.
constexpr uint64_t BassReply = 100;

// Volume steps during a fade: smooth to the ear, and few enough commands for mpv.
constexpr int FadeStepMs = 20;
// A change overlapping this long shows "Mixing".
constexpr double MixingSeconds = 2;
// How long before a fade the next song starts loading.
constexpr double PreloadSeconds = 10;
// How long before the end of a song Automatic crossfade measures the change: time for a slow server,
// and late enough that a song skipped early was not fetched for nothing.
constexpr double AnalyzeSeconds = 60;

// Each measured change, for tuning by ear: QT_LOGGING_RULES="legato.automix=false" hides them.
Q_LOGGING_CATEGORY(lcAutomix, "legato.automix", QtInfoMsg)

const Song &emptySong()
{
    static const Song song;
    return song;
}

} // namespace

Player::Player(QObject *parent)
    : QObject(parent)
{
    connect(this, &Player::queueChanged, this, &Player::syncUpNext);
    connect(this, &Player::positionChanged, this, &Player::updateLyricsLine);
    m_fadeTimer.setInterval(FadeStepMs);
    m_fadeTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_fadeTimer, &QTimer::timeout, this, &Player::fadeStep);
    if (!createDeck(m_decks[0], 0) || !createDeck(m_decks[1], 1)) {
        for (Deck &deck : m_decks) {
            if (deck.mpv) {
                mpv_set_wakeup_callback(deck.mpv, nullptr, nullptr);
                mpv_terminate_destroy(deck.mpv);
                deck.mpv = nullptr;
            }
        }
        setError(u"libmpv could not be started"_s);
    }
}

bool Player::createDeck(Deck &deck, int index)
{
    deck.mpv = mpv_create();
    if (!deck.mpv)
        return false;
    mpv_handle *mpv = deck.mpv;
    const auto option = [mpv](const char *name, const char *value) { mpv_set_option_string(mpv, name, value); };
    mpvoptions::setCommon(mpv);
    // The name the sound settings show for legato's stream.
    option("audio-client-name", "Legato");
    // The bass swap's low shelf (crossfade::bassCuts), at 0 dB but while a change swaps the bass. A
    // filter added only for the change would restart the audio chain, a gap in the music. In floating
    // point: newer FFmpeg runs it on the decoder's integer samples, where a loud master clips (and
    // logs each clip) even at 0 dB, and a cut that raises a peak past full scale would clip for real.
    // mpv's volume, after the filter, still brings a fading song back under full scale.
    const QByteArray shelf = "@swap:lavfi=[lowshelf@swap=f=" + QByteArray::number(crossfade::BassHz)
        + ":t=q:w=0.707:g=0:precision=f64]";
    option("af", shelf.constData());
    // Gapless playback over the network needs the next song fetched before the current one ends.
    option("gapless-audio", "weak");
    option("prefetch-playlist", "yes");
    option("network-timeout", "30");
    // mpv's buffer is sized for video from the internet: up to 150 MiB read ahead and 50 MiB kept
    // behind, which for a song is the whole file, then the next one once prefetch starts. Half a
    // minute ahead, so a gapless next song is still fetched half a minute before it plays; a ceiling
    // for hi-res files; a little behind for a short seek back.
    option("cache-secs", "30");
    option("demuxer-max-bytes", "32MiB");
    option("demuxer-max-back-bytes", "2MiB");
    mpv_request_log_messages(mpv, "warn");

    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        deck.mpv = nullptr;
        return false;
    }
    // The volume is not observed: legato sets it, and during a fade it is not the user's volume.
    mpv_observe_property(mpv, Pause, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, TimePos, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, Duration, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, Path, "path", MPV_FORMAT_STRING);
    mpv_observe_property(mpv, IdleActive, "idle-active", MPV_FORMAT_FLAG);
    m_wakeSlots[index] = {this, index};
    mpv_set_wakeup_callback(mpv, [](void *context) {
        const auto *slot = static_cast<const WakeSlot *>(context);
        QMetaObject::invokeMethod(slot->player, [player = slot->player, i = slot->index] { player->drainEvents(i); },
                                  Qt::QueuedConnection);
    }, &m_wakeSlots[index]);
    return true;
}

Player::~Player()
{
    for (Deck &deck : m_decks) {
        if (deck.mpv) {
            mpv_set_wakeup_callback(deck.mpv, nullptr, nullptr);
            mpv_terminate_destroy(deck.mpv);
        }
    }
}

const Song &Player::song() const
{
    const QueueEntry *entry = m_queue.current();
    return entry ? entry->song : emptySong();
}

void Player::command(Deck &deck, std::initializer_list<const char *> args)
{
    if (!deck.mpv)
        return;
    std::vector<const char *> argv(args);
    argv.push_back(nullptr);
    // Async, so a slow server never blocks the interface. mpv runs them in the order given.
    mpv_command_async(deck.mpv, 0, argv.data());
}

void Player::applyVolume(Deck &deck)
{
    const QByteArray volume = QByteArray::number(crossfade::mpvVolume(m_volume, deck.gain), 'f', 3);
    command(deck, {"set", "volume", volume.constData()});
}

void Player::applyBass(Deck &deck)
{
    if (!deck.mpv || std::abs(deck.bassCut - deck.sentBassCut) < 0.01)
        return;
    deck.sentBassCut = deck.bassCut;
    const QByteArray gain = QByteArray::number(-crossfade::BassCutDb * deck.bassCut, 'f', 2);
    const char *argv[] = {"af-command", "swap", "g", gain.constData(), "lowshelf@swap", nullptr};
    mpv_command_async(deck.mpv, BassReply, argv);
}

void Player::play(const QList<Song> &songs, const QList<QUrl> &urls, int start, const QString &from)
{
    if (!available() || songs.isEmpty() || songs.size() != urls.size())
        return;
    QList<QueueEntry> entries;
    entries.reserve(songs.size());
    for (qsizetype i = 0; i < songs.size(); ++i)
        entries.append({songs.at(i), urls.at(i)});
    m_queue.replace(std::move(entries), start, m_shuffle);
    m_from = from;
    setError({});
    startAt(m_queue.position());
}

void Player::restore(const QList<Song> &songs, const QList<QUrl> &urls, int index, double seconds)
{
    if (!available() || songs.isEmpty() || songs.size() != urls.size())
        return;
    QList<QueueEntry> entries;
    entries.reserve(songs.size());
    for (qsizetype i = 0; i < songs.size(); ++i)
        entries.append({songs.at(i), urls.at(i)});
    // Already in play order; shuffling it again would move the songs the server holds.
    if (m_shuffle) {
        m_shuffle = false;
        emit shuffleChanged();
    }
    m_queue.replace(std::move(entries), std::clamp(index, 0, int(songs.size()) - 1), false);
    m_from.clear();
    dropOther();
    m_ended = true;
    m_paused = true;
    active().currentUrl.clear();
    active().nextUrl.clear();
    m_nextUrl.clear();
    m_fadeNext = false;
    m_duration = song().duration;
    m_position = std::clamp(seconds, 0.0, std::max(0.0, m_duration));
    setError({});
    announceSong();
}

void Player::continueWith(const QList<Song> &songs, const QList<QUrl> &urls, const QString &from)
{
    if (!available() || songs.isEmpty() || songs.size() != urls.size())
        return;
    if (!hasSong()) {
        play(songs, urls, 0, from);
        return;
    }
    QList<QueueEntry> entries{*m_queue.current()};
    entries.reserve(songs.size() + 1);
    for (qsizetype i = 0; i < songs.size(); ++i)
        entries.append({songs.at(i), urls.at(i)});
    m_queue.replace(std::move(entries), 0, m_shuffle);
    m_from = from;
    emit queueChanged();
    if (!m_ended)
        updateNext(false);
}

void Player::insert(const QList<Song> &songs, const QList<QUrl> &urls, bool next)
{
    if (!available() || songs.isEmpty() || songs.size() != urls.size())
        return;
    if (!hasSong()) {
        play(songs, urls, 0, {});
        return;
    }
    QList<QueueEntry> entries;
    entries.reserve(songs.size());
    for (qsizetype i = 0; i < songs.size(); ++i)
        entries.append({songs.at(i), urls.at(i)});
    m_queue.insert(std::move(entries), next);
    emit queueChanged();
    if (!m_ended)
        updateNext(false);
}

QVariantList Player::artists() const
{
    return artistList(song().artists);
}

void Player::startAt(int index, double from)
{
    m_queue.setPosition(index);
    const QueueEntry *entry = m_queue.current();
    if (!entry)
        return;
    // A change the listener asked for cuts at once, as in other players; only songs that end by
    // themselves fade.
    dropOther();
    Deck &deck = active();
    if (deck.gain != 1) {
        deck.gain = 1;
        applyVolume(deck);
    }
    m_ended = false;
    deck.currentUrl = entry->url.toEncoded();
    deck.switching = true;
    deck.failed = false;
    deck.duration = 0;
    deck.pendingSeek = from;
    m_position = from;
    m_duration = entry->song.duration;
    command(deck, {"loadfile", deck.currentUrl.constData(), "replace"});
    command(deck, {"set", "pause", "no"});
    m_paused = false;
    updateNext(true);
    announceSong();
}

void Player::updateNext(bool force)
{
    if (!available())
        return;
    Deck &deck = active();
    const int following = m_queue.following(m_repeat == 1);
    // One song on repeat all follows itself; mpv loops the file instead, as for repeat one.
    const bool loopFile = m_repeat == 2 || (m_repeat == 1 && following == m_queue.position());
    mpv_set_property_string(deck.mpv, "loop-file", loopFile ? "inf" : "no");
    const QByteArray url = following >= 0 && !loopFile ? m_queue.at(following).url.toEncoded() : QByteArray();
    const bool fade = !url.isEmpty() && m_crossfade > 0
        && !(m_keepAlbumsGapless && crossfade::albumContinues(song(), m_queue.at(following).song));
    if (!force && url == m_nextUrl && fade == m_fadeNext)
        return;
    m_nextUrl = url;
    m_fadeNext = fade;
    const bool due = m_fadeDue;
    // Removes everything but the playing entry.
    command(deck, {"playlist-clear"});
    deck.nextUrl = fade ? QByteArray() : url;
    if (!deck.nextUrl.isEmpty())
        command(deck, {"loadfile", url.constData(), "append"});
    // The other deck holds a song that is no longer next. During a fade it holds the old song.
    if (!m_fading && !other().currentUrl.isEmpty() && (!fade || other().currentUrl != url))
        dropOther();
    // The current song had ended and was waiting for the old next one to load.
    if (due && !m_fadeDue) {
        if (following >= 0)
            startAt(following);
        else
            endQueue();
        return;
    }
    checkFade();
}

double Player::fadeLength() const
{
    const int following = m_queue.following(m_repeat == 1);
    if (following < 0)
        return 0;
    return crossfade::length(m_crossfade, m_duration, m_queue.at(following).song.duration);
}

QByteArray Player::planKey() const
{
    return m_decks[m_active].currentUrl + '\n' + m_nextUrl;
}

crossfade::Plan Player::plan() const
{
    if (m_automatic && m_autoPlan && m_planFor == planKey())
        return *m_autoPlan;
    return crossfade::fixedPlan(fadeLength());
}

void Player::analyzeNext()
{
    if (!m_automatic || !m_analyzer || m_position < m_duration - AnalyzeSeconds)
        return;
    const QByteArray key = planKey();
    const int following = m_queue.following(m_repeat == 1);
    if (key == m_planFor || following < 0)
        return;
    m_planFor = key;
    m_autoPlan.reset();
    // Parts of a change that is no longer next.
    m_analyzer->dropWaiting();
    struct Parts {
        automix::Part tail;
        automix::Part head;
        int left = 2;
    };
    auto parts = std::make_shared<Parts>();
    const QueueEntry &in = m_queue.at(following);
    const QString names = song().title + u" → "_s + in.song.title;
    const double inDuration = in.song.duration;
    QPointer<Player> self(this);
    const auto measured = [self, key, parts, names, inDuration] {
        if (--parts->left > 0 || !self || self->m_planFor != key || self->planKey() != key)
            return;
        automix::Judged judged;
        if (!parts->tail.level.isEmpty() && !parts->head.level.isEmpty()) {
            const double limit = crossfade::length(automix::MaxOverlap, self->m_duration, inDuration);
            self->m_autoPlan = automix::plan(parts->tail, parts->head, limit, &judged);
        }
        if (const auto &p = self->m_autoPlan) {
            qCInfo(lcAutomix,
                   "%s: in %.2f s before the end, out at %.2f, from %.2f, fade out after %.2f, in over %.2f, "
                   "bass swap at %.2f; beat %.2f → %.2f%s",
                   qUtf8Printable(names), p->start, p->stop, p->inFrom, p->outFadeFrom, p->inRamp, p->bassSwap,
                   judged.oldBeat, judged.newBeat, judged.beatsClash ? ", both: short change" : "");
        } else {
            qCInfo(lcAutomix, "%s: not measured, fixed fade", qUtf8Printable(names));
        }
        self->cueNext();
        self->checkFade();
    };
    m_analyzer->analyze(song().id, active().currentUrl, MixAnalyzer::Part::Tail,
                        [parts, measured](const automix::Part &part) { parts->tail = part; measured(); });
    m_analyzer->analyze(in.song.id, in.url.toEncoded(), MixAnalyzer::Part::Head,
                        [parts, measured](const automix::Part &part) { parts->head = part; measured(); });
}

void Player::cueNext()
{
    Deck &next = other();
    if (m_fading || next.currentUrl != m_nextUrl || next.switching || next.failed)
        return;
    const double from = plan().inFrom;
    if (std::abs(from - next.cue) < 0.01)
        return;
    next.cue = from;
    command(next, {"seek", QByteArray::number(from).constData(), "absolute"});
}

void Player::checkFade()
{
    if (!m_fadeNext || m_fading || m_ended || active().switching || m_duration <= 0)
        return;
    analyzeNext();
    const crossfade::Plan p = plan();
    Deck &next = other();
    const double start = m_duration - p.start;
    // Early enough for a slow server, late enough that a paused stream is not left open for long.
    if (next.currentUrl != m_nextUrl && m_position >= start - PreloadSeconds) {
        next.currentUrl = m_nextUrl;
        next.nextUrl.clear();
        next.switching = true;
        next.failed = false;
        next.duration = 0;
        next.cue = 0;
        next.gain = 0;
        applyVolume(next);
        command(next, {"set", "pause", "yes"});
        command(next, {"set", "loop-file", "no"});
        command(next, {"loadfile", next.currentUrl.constData(), "replace"});
    }
    if (m_position >= start && next.currentUrl == m_nextUrl && !next.switching && !next.failed)
        beginFade(m_duration - p.stop - m_position);
}

void Player::beginFade(double remaining)
{
    // Taken before the queue moves on: the plan belongs to this change.
    m_fadePlan = plan();
    m_fadeLength = std::min(m_fadePlan.overlap(), std::max(0.0, remaining));
    m_fadeSkip = m_fadePlan.overlap() - m_fadeLength;
    Deck &out = active();
    m_active = 1 - m_active;
    Deck &in = active();
    m_fadeDue = false;
    m_queue.setPosition(m_queue.following(m_repeat == 1));
    m_position = in.cue;
    m_duration = in.duration > 0 ? in.duration : song().duration;
    // Too little of the old song is left to hear a fade: the new one starts as after a gap.
    if (m_fadeLength < 0.3) {
        command(out, {"stop"});
        out.currentUrl.clear();
        out.nextUrl.clear();
        in.gain = 1;
        in.bassCut = 0;
    } else {
        m_fading = true;
        m_fadeDone = 0;
        m_fadeClock.start();
        std::tie(out.gain, in.gain) = crossfade::gains(m_fadePlan, m_fadeSkip);
        std::tie(out.bassCut, in.bassCut) = crossfade::bassCuts(m_fadePlan, m_fadeSkip);
        applyVolume(out);
        applyBass(out);
        if (!m_paused)
            m_fadeTimer.start();
    }
    // Its filter may hold what an earlier change left there.
    in.sentBassCut = std::nan("");
    applyBass(in);
    applyVolume(in);
    if (!m_paused)
        command(in, {"set", "pause", "no"});
    updateNext(true);
    // Before the song changes on screen, so the cover takes as long as the change to turn.
    setMixing(m_fading && m_fadeLength >= MixingSeconds);
    announceSong();
}

void Player::setMixing(bool mixing)
{
    if (mixing == m_mixing)
        return;
    m_mixing = mixing;
    emit mixingChanged();
}

void Player::fadeStep()
{
    if (!m_fading)
        return;
    const double elapsed = m_fadeDone + (m_paused ? 0 : m_fadeClock.elapsed() / 1000.0);
    if (elapsed >= m_fadeLength) {
        finishFade();
        return;
    }
    const auto [outGain, inGain] = crossfade::gains(m_fadePlan, m_fadeSkip + elapsed);
    other().gain = outGain;
    active().gain = inGain;
    applyVolume(other());
    applyVolume(active());
    std::tie(other().bassCut, active().bassCut) = crossfade::bassCuts(m_fadePlan, m_fadeSkip + elapsed);
    applyBass(other());
    applyBass(active());
}

void Player::finishFade()
{
    if (!m_fading)
        return;
    m_fading = false;
    m_fadeTimer.stop();
    Deck &out = other();
    out.bassCut = 0;
    applyBass(out);
    command(out, {"stop"});
    out.currentUrl.clear();
    out.nextUrl.clear();
    out.switching = false;
    active().gain = 1;
    applyVolume(active());
    active().bassCut = 0;
    applyBass(active());
    setMixing(false);
    // The song after the new one may be short enough to be loaded now.
    checkFade();
}

void Player::dropOther()
{
    m_fading = false;
    m_fadeTimer.stop();
    m_fadeDue = false;
    setMixing(false);
    // A change cut short leaves the playing song whole.
    active().bassCut = 0;
    applyBass(active());
    Deck &deck = other();
    deck.bassCut = 0;
    applyBass(deck);
    if (deck.currentUrl.isEmpty() && !deck.switching)
        return;
    command(deck, {"stop"});
    deck.currentUrl.clear();
    deck.nextUrl.clear();
    deck.switching = false;
    deck.failed = false;
}

void Player::announceSong()
{
    emit songChanged();
    emit starredChanged();
    emit queueChanged();
    emit positionChanged();
    emit durationChanged();
    m_lyrics.clear();
    m_lyricsSynced = false;
    emit lyricsChanged();
    updateLyricsLine();
    updatePlaying();
    loadPalette();
}

void Player::drainEvents(int index)
{
    Deck &deck = m_decks[index];
    while (deck.mpv) {
        mpv_event *event = mpv_wait_event(deck.mpv, 0);
        const bool isActive = index == m_active;
        switch (event->event_id) {
        case MPV_EVENT_NONE:
            return;
        case MPV_EVENT_PROPERTY_CHANGE:
            onProperty(index, *static_cast<mpv_event_property *>(event->data));
            break;
        case MPV_EVENT_FILE_LOADED:
            deck.switching = false;
            if (!isActive) {
                // The next song is ready: it starts now when its time has come or already passed.
                cueNext();
                if (m_fadeDue)
                    beginFade(0);
                else
                    checkFade();
                break;
            }
            // A song that failed is reported until the next one loads (mpv moves on by itself).
            deck.failed = false;
            setError({});
            if (deck.pendingSeek > 0)
                command(deck, {"seek", QByteArray::number(deck.pendingSeek).constData(), "absolute"});
            deck.pendingSeek = 0;
            break;
        case MPV_EVENT_END_FILE: {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end->reason != MPV_END_FILE_REASON_ERROR)
                break;
            if (isActive) {
                deck.failed = true;
                setError(u"The song could not be played: %1"_s.arg(QString::fromUtf8(mpv_error_string(end->error))));
            } else if (!m_fading) {
                // The next song would not load. The active one plays to its end; then the next is
                // tried once more as an ordinary change, which shows the error if it fails again.
                deck.switching = false;
                deck.failed = true;
                if (m_fadeDue) {
                    m_fadeDue = false;
                    startAt(m_queue.following(m_repeat == 1));
                }
            }
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {
            const auto *msg = static_cast<mpv_event_log_message *>(event->data);
            qWarning("mpv: %s: %s", msg->prefix, QByteArray(msg->text).trimmed().constData());
            break;
        }
        case MPV_EVENT_COMMAND_REPLY:
            // Sent again with the next step.
            if (event->reply_userdata == BassReply && event->error < 0) {
                qCWarning(lcAutomix, "bass swap: %s", mpv_error_string(event->error));
                deck.sentBassCut = std::nan("");
            }
            break;
        default:
            break;
        }
    }
}

void Player::onProperty(int index, const mpv_event_property &property)
{
    Deck &deck = m_decks[index];
    // Kept for the other deck too: it reports the length of the next song before that one is shown.
    if (qstrcmp(property.name, "duration") == 0 && property.format == MPV_FORMAT_DOUBLE)
        deck.duration = *static_cast<double *>(property.data);
    // The other deck only loads the next song or fades the old one out; the interface follows the
    // active one.
    if (index != m_active)
        return;
    const auto flag = [&] { return property.format == MPV_FORMAT_FLAG && *static_cast<int *>(property.data); };
    const auto number = [&] { return property.format == MPV_FORMAT_DOUBLE ? *static_cast<double *>(property.data) : 0.0; };
    const QByteArray name(property.name);
    if (name == "pause") {
        m_paused = flag();
        updatePlaying();
    } else if (name == "time-pos") {
        // Between songs mpv has no position, and the song being replaced may still report one.
        if (property.format != MPV_FORMAT_DOUBLE || deck.switching || deck.pendingSeek > 0)
            return;
        const double previous = m_position;
        m_position = number();
        emit positionChanged();
        // mpv went back to the start of the song by itself: repeat.
        if (m_position < previous - 1.5)
            emit seeked();
        checkFade();
    } else if (name == "duration") {
        if (property.format != MPV_FORMAT_DOUBLE || deck.switching)
            return;
        m_duration = number();
        emit durationChanged();
    } else if (name == "path") {
        if (property.format != MPV_FORMAT_STRING)
            return;
        const QByteArray path(*static_cast<char **>(property.data));
        // mpv moved on to the song queued after the current one. Songs legato started itself were
        // announced when it started them.
        if (deck.nextUrl.isEmpty() || path != deck.nextUrl)
            return;
        m_queue.setPosition(m_queue.following(m_repeat == 1));
        deck.currentUrl = deck.nextUrl;
        deck.failed = false;
        m_position = 0;
        m_duration = song().duration;
        updateNext(true);
        announceSong();
    } else if (name == "idle-active" && flag() && hasSong() && !m_ended) {
        // After a song that played (one that failed stops the queue, as without crossfade).
        if (m_fadeNext && !m_fading && !deck.failed) {
            const Deck &next = other();
            // The fade never began: the next song was still loading, or it failed.
            if (next.currentUrl == m_nextUrl && !next.failed) {
                if (next.switching)
                    m_fadeDue = true;
                else
                    beginFade(0);
            } else {
                startAt(m_queue.following(m_repeat == 1));
            }
            return;
        }
        endQueue();
    }
}

void Player::endQueue()
{
    // The queue ran out. The last song stays, stopped at its end; play starts it again.
    m_ended = true;
    m_nextUrl.clear();
    m_fadeNext = false;
    m_position = m_duration;
    emit positionChanged();
    updatePlaying();
}

void Player::updatePlaying()
{
    const bool playing = !m_paused && hasSong() && !m_ended;
    if (playing == m_playing)
        return;
    m_playing = playing;
    emit playingChanged();
}

void Player::syncUpNext()
{
    QList<Song> songs;
    songs.reserve(std::max(0, m_queue.size() - m_queue.position() - 1));
    for (int i = m_queue.position() + 1; i < m_queue.size(); ++i)
        songs.append(m_queue.at(i).song);
    m_upNext.sync(std::move(songs));
}

void Player::setLyrics(const QString &songId, const Lyrics &lyrics)
{
    if (songId != this->songId())
        return;
    m_lyricsFor = songId;
    m_lyrics.set(lyrics);
    m_lyricsSynced = lyrics.synced;
    emit lyricsChanged();
    updateLyricsLine();
}

void Player::updateLyricsLine()
{
    // A line lights up a little before its time, with the voice rather than after it.
    const int line = m_lyrics.lineAt(m_position + 0.15);
    if (line == m_lyricsLine)
        return;
    m_lyricsLine = line;
    emit lyricsLineChanged();
}

void Player::setStarred(const QString &songId, bool starred)
{
    if (m_queue.setStarred(songId, starred) && songId == this->songId())
        emit starredChanged();
}

void Player::loadPalette()
{
    const QString cover = coverArt();
    // Songs of one album share a cover and keep its colors.
    if (cover == m_paletteCover)
        return;
    m_paletteCover = cover;
    if (!m_covers || cover.isEmpty()) {
        m_palette.clear();
        m_accentLight = m_accentDark = QColor();
        emit paletteChanged();
        return;
    }
    QPointer<Player> self(this);
    m_covers->load(cover, 300, nullptr, [self, cover](const QByteArray &bytes, const QString &error) {
        if (!error.isEmpty())
            return;
        const QList<QColor> colors = extractPalette(CoverProvider::decode(bytes, QSize(64, 64)));
        if (colors.isEmpty())
            return;
        QMetaObject::invokeMethod(self, [self, cover, colors] {
            if (!self || self->coverArt() != cover)
                return;
            self->m_palette.clear();
            for (const QColor &c : colors)
                self->m_palette.append(c);
            self->m_accentLight = accentFrom(colors.first(), false);
            self->m_accentDark = accentFrom(colors.first(), true);
            emit self->paletteChanged();
        }, Qt::QueuedConnection);
    });
}

void Player::setSilentFallback(bool on)
{
    for (Deck &deck : m_decks) {
        if (deck.mpv)
            mpv_set_property_string(deck.mpv, "audio-fallback-to-null", on ? "yes" : "no");
    }
}

void Player::setPaused(bool paused)
{
    // Set here as well as by mpv's answer, so the button turns at once.
    m_paused = paused;
    const char *value = paused ? "yes" : "no";
    command(active(), {"set", "pause", value});
    if (m_fading) {
        command(other(), {"set", "pause", value});
        if (paused) {
            m_fadeDone += m_fadeClock.elapsed() / 1000.0;
            m_fadeTimer.stop();
        } else {
            m_fadeClock.start();
            m_fadeTimer.start();
        }
    }
    updatePlaying();
}

void Player::togglePause()
{
    if (!hasSong())
        return;
    if (m_ended) {
        startAt(m_queue.position(), m_position < m_duration - 1 ? m_position : 0);
        return;
    }
    setPaused(!m_paused);
}

void Player::next()
{
    if (!hasSong())
        return;
    const int following = m_queue.following(true);
    startAt(following);
}

void Player::previous()
{
    if (!hasSong())
        return;
    if (m_position > 3 || (m_queue.position() == 0 && m_repeat != 1)) {
        seek(0);
        return;
    }
    startAt(m_queue.position() > 0 ? m_queue.position() - 1 : m_queue.size() - 1);
}

void Player::seek(double seconds)
{
    if (!hasSong())
        return;
    seconds = std::clamp(seconds, 0.0, std::max(0.0, m_duration));
    m_position = seconds;
    emit positionChanged();
    emit seeked();
    // Nothing loaded: play starts there.
    if (m_ended)
        return;
    // A seek during a fade is in the new song: the old one goes at once.
    finishFade();
    // A song loaded for a fade that is now minutes away would sit on an open, paused stream.
    if (!other().currentUrl.isEmpty() && seconds < m_duration - plan().start - 2 * PreloadSeconds)
        dropOther();
    command(active(), {"seek", QByteArray::number(seconds).constData(), "absolute"});
}

void Player::setVolume(int volume)
{
    volume = std::clamp(volume, 0, 100);
    if (volume == m_volume)
        return;
    m_volume = volume;
    emit volumeChanged();
    applyVolume(active());
    if (m_fading)
        applyVolume(other());
}

void Player::setCrossfade(int seconds)
{
    seconds = std::clamp(seconds, 0, 12);
    if (seconds == m_crossfade)
        return;
    m_crossfade = seconds;
    emit crossfadeChanged();
    if (hasSong() && !m_ended)
        updateNext(false);
}

void Player::setAutomaticCrossfade(bool on)
{
    if (on == m_automatic)
        return;
    m_automatic = on;
    if (on && !m_analyzer) {
        m_analyzer = new MixAnalyzer(this);
        m_analyzer->setDirectory(m_analysisDir);
    }
    m_planFor.clear();
    m_autoPlan.reset();
    emit crossfadeChanged();
    if (hasSong() && !m_ended) {
        cueNext();
        checkFade();
    }
}

void Player::setAnalysisDirectory(const QString &dir)
{
    m_analysisDir = dir;
    if (m_analyzer)
        m_analyzer->setDirectory(dir);
}

void Player::setKeepAlbumsGapless(bool on)
{
    if (on == m_keepAlbumsGapless)
        return;
    m_keepAlbumsGapless = on;
    emit crossfadeChanged();
    if (hasSong() && !m_ended)
        updateNext(false);
}

void Player::toggleShuffle()
{
    setShuffle(!m_shuffle);
}

void Player::setShuffle(bool on)
{
    if (m_shuffle == on)
        return;
    m_shuffle = on;
    m_queue.setShuffle(m_shuffle);
    emit shuffleChanged();
    emit queueChanged();
    if (hasSong() && !m_ended)
        updateNext(false);
}

void Player::cycleRepeat()
{
    setRepeat((m_repeat + 1) % 3);
}

void Player::setRepeat(int repeat)
{
    if (repeat < 0 || repeat > 2 || repeat == m_repeat)
        return;
    m_repeat = repeat;
    emit repeatChanged();
    if (hasSong() && !m_ended)
        updateNext(false);
}

void Player::playAt(int index)
{
    if (index >= 0 && index < m_queue.size())
        startAt(index);
}

void Player::removeAt(int index)
{
    if (index == m_queue.position())
        return;
    m_queue.remove(index);
    emit queueChanged();
    if (!m_ended)
        updateNext(false);
}

void Player::stop()
{
    if (!hasSong())
        return;
    m_queue.replace({}, -1, false);
    dropOther();
    m_ended = true;
    command(active(), {"stop"});
    m_from.clear();
    active().currentUrl.clear();
    active().nextUrl.clear();
    m_nextUrl.clear();
    m_fadeNext = false;
    m_position = 0;
    m_duration = 0;
    announceSong();
}

void Player::clearUpNext()
{
    m_queue.clearUpNext();
    emit queueChanged();
    if (hasSong() && !m_ended)
        updateNext(false);
}

void Player::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

} // namespace legato

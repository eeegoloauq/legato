// Playback through libmpv (docs/architecture.md, "Playback engine").
//
// Audio only: mpv runs without video output or its render API. Its events arrive on mpv's thread;
// the wakeup callback only queues a call to drain them on the GUI thread.
//
// legato owns the queue (playqueue.h). mpv's playlist holds the current song and the one after it,
// so mpv can fetch the next song early and play the two without a gap.
//
// Crossfade needs two songs playing at once, which one mpv cannot do: there are two mpv instances
// ("decks"). The active deck plays the song the interface shows. Before a change that fades, the
// other deck loads the next song paused; the fade starts it, swaps the roles and ramps the volumes.
// With Automatic crossfade, a third mpv (mixanalyzer.h) measures the two songs around the change
// first, and the change follows their outro and intro (automix.h) instead of a fixed length.
#pragma once

#include "crossfade.h"
#include "models.h"
#include "playqueue.h"
#include "subsonic.h"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQmlIntegration>

#include <optional>

struct mpv_handle;
struct mpv_event_property;

namespace legato {

class CoverStore;
class MixAnalyzer;

class Player : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool hasSong READ hasSong NOTIFY songChanged)
    Q_PROPERTY(QString songId READ songId NOTIFY songChanged)
    Q_PROPERTY(QString title READ title NOTIFY songChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY songChanged)
    Q_PROPERTY(QString album READ album NOTIFY songChanged)
    Q_PROPERTY(QString albumId READ albumId NOTIFY songChanged)
    // Each artist of the song as {id, name}, for one link each.
    Q_PROPERTY(QVariantList artists READ artists NOTIFY songChanged)
    Q_PROPERTY(QString coverArt READ coverArt NOTIFY songChanged)
    Q_PROPERTY(QString quality READ quality NOTIFY songChanged)
    Q_PROPERTY(bool starred READ starred NOTIFY starredChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY shuffleChanged)
    // 0 off, 1 all, 2 this song
    Q_PROPERTY(int repeat READ repeat NOTIFY repeatChanged)
    // Seconds songs overlap when one changes into the next by itself; 0 plays them gapless.
    Q_PROPERTY(int crossfade READ crossfade WRITE setCrossfade NOTIFY crossfadeChanged)
    // Fits each change to the songs' outro and intro; `crossfade` is the fade used when they could
    // not be measured. Has no effect while `crossfade` is 0.
    Q_PROPERTY(bool automaticCrossfade READ automaticCrossfade WRITE setAutomaticCrossfade NOTIFY crossfadeChanged)
    // Never fades into the next track of the same album (live albums, continuous mixes).
    Q_PROPERTY(bool keepAlbumsGapless READ keepAlbumsGapless WRITE setKeepAlbumsGapless NOTIFY crossfadeChanged)
    // The old song is still heard under the current one, in a change long enough to show ("Mixing"),
    // and how long that change overlaps in all.
    Q_PROPERTY(bool mixing READ mixing NOTIFY mixingChanged)
    Q_PROPERTY(double mixSeconds READ mixSeconds NOTIFY mixingChanged)
    // The songs after the current one, in play order. A model rather than a list: a song change
    // removes a row instead of rebuilding the Up Next panel.
    Q_PROPERTY(legato::SongModel *upNext READ upNext CONSTANT)
    // The current song's place in the play order; Up Next's row r is at queueIndex + 1 + r.
    Q_PROPERTY(int queueIndex READ queueIndex NOTIFY queueChanged)
    // The album or playlist the queue came from.
    Q_PROPERTY(QString playingFrom READ playingFrom NOTIFY queueChanged)
    // The current song's lyrics as maps of time (seconds, -1 when unsynced) and text.
    Q_PROPERTY(QVariantList lyrics READ lyrics NOTIFY lyricsChanged)
    Q_PROPERTY(bool lyricsSynced READ lyricsSynced NOTIFY lyricsChanged)
    // True until the server has answered for the current song.
    Q_PROPERTY(bool lyricsLoading READ lyricsLoading NOTIFY lyricsChanged)
    // Three colors from the cover, most prominent first; empty without a song.
    Q_PROPERTY(QVariantList palette READ palette NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentLight READ accentLight NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentDark READ accentDark NOTIFY paletteChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    void setCovers(CoverStore *covers) { m_covers = covers; }
    // Where Automatic crossfade keeps what it measured; one directory per server, empty for none.
    void setAnalysisDirectory(const QString &dir);
    // Development: play to a null output when there is no sound device.
    void setSilentFallback(bool on);
    // Replaces the queue and plays it. `urls` are the stream URLs of `songs`, in the same order.
    // A negative start plays from the top: the first song, or a random one with shuffle on.
    void play(const QList<Song> &songs, const QList<QUrl> &urls, int start, const QString &from);
    // Puts back a saved queue (in play order) without playing it: the song at `index` shows paused at
    // `seconds`, and mpv loads it when play is pressed.
    void restore(const QList<Song> &songs, const QList<QUrl> &urls, int index, double seconds);
    // Stops and empties the queue, for a server switch: its songs belong to the old server.
    void stop();
    // Keeps the current song where it is (playing or not) and puts `songs` after it in place of
    // everything else, from `from`. With nothing playing, plays them.
    void continueWith(const QList<Song> &songs, const QList<QUrl> &urls, const QString &from);
    // Play Next (`next`) or Add to Queue; with nothing playing, plays them.
    void insert(const QList<Song> &songs, const QList<QUrl> &urls, bool next);
    void setLyrics(const QString &songId, const Lyrics &lyrics);
    void setStarred(const QString &songId, bool starred);
    void setShuffle(bool on);
    // Shows a failure that is not the player's own (a song that would not load from the server)
    // where its own errors appear.
    void showError(const QString &error) { setError(error); }

    bool available() const { return m_decks[0].mpv != nullptr; }
    bool hasSong() const { return m_queue.current() != nullptr; }
    // mpv has the current song: false for a restored queue not played yet and after the queue ended.
    bool songLoaded() const { return hasSong() && !m_ended; }
    QString songId() const { return song().id; }
    QString title() const { return song().title; }
    QString artist() const { return song().artist; }
    QString album() const { return song().album; }
    QString albumId() const { return song().albumId; }
    QVariantList artists() const;
    QString coverArt() const { return song().coverArt; }
    QString quality() const { return subsonic::qualityLabel(song()); }
    bool starred() const { return song().starred; }
    bool playing() const { return m_playing; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    int volume() const { return m_volume; }
    void setVolume(int volume);
    bool shuffle() const { return m_shuffle; }
    int repeat() const { return m_repeat; }
    int crossfade() const { return m_crossfade; }
    void setCrossfade(int seconds);
    bool automaticCrossfade() const { return m_automatic; }
    void setAutomaticCrossfade(bool on);
    bool keepAlbumsGapless() const { return m_keepAlbumsGapless; }
    void setKeepAlbumsGapless(bool on);
    bool mixing() const { return m_mixing; }
    double mixSeconds() const { return m_fadeLength; }
    SongModel *upNext() { return &m_upNext; }
    // In play order, for saving.
    const PlayQueue &queue() const { return m_queue; }
    // The current song; an empty one when nothing plays.
    const Song &song() const;
    int queueIndex() const { return m_queue.position(); }
    QString playingFrom() const { return m_from; }
    QVariantList lyrics() const { return m_lyrics; }
    bool lyricsSynced() const { return m_lyricsSynced; }
    bool lyricsLoading() const { return hasSong() && m_lyricsFor != songId(); }
    QVariantList palette() const { return m_palette; }
    QColor accentLight() const { return m_accentLight; }
    QColor accentDark() const { return m_accentDark; }
    QString error() const { return m_error; }

    Q_INVOKABLE void togglePause();
    // At the end of the queue, starts over from the first song.
    Q_INVOKABLE void next();
    // Restarts the song when more than 3 s in, as the keyboard map says (docs/design.md).
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void cycleRepeat();
    // 0 off, 1 all, 2 this song.
    void setRepeat(int repeat);
    // Indexes are places in the play order.
    Q_INVOKABLE void playAt(int index);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void clearUpNext();

signals:
    void songChanged();
    void starredChanged();
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void shuffleChanged();
    void repeatChanged();
    void crossfadeChanged();
    void mixingChanged();
    void queueChanged();
    void lyricsChanged();
    void paletteChanged();
    void errorChanged();
    // The position jumped: a seek, or the song starting over on repeat. Not sent for a new song.
    void seeked();

private:
    struct Deck {
        mpv_handle *mpv = nullptr;
        // The song it plays or loads, and the one queued after it for a gapless change.
        QByteArray currentUrl;
        QByteArray nextUrl;
        // A song was given to it and has not loaded yet; what it reports still belongs to the one before.
        bool switching = false;
        double pendingSeek = 0;
        // Where a song loaded for a fade waits to start (an Automatic crossfade skips its silence).
        double cue = 0;
        // Its song would not load.
        bool failed = false;
        // The fade's share of the user's volume.
        double gain = 1;
        // How much of its low end the bass swap cuts (0..1), and what its filter was last given; NaN
        // sends the next value whatever it is.
        double bassCut = 0;
        double sentBassCut = 0;
        // What mpv reports for its song; 0 until known.
        double duration = 0;
    };

    bool createDeck(Deck &deck, int index);
    Deck &active() { return m_decks[m_active]; }
    Deck &other() { return m_decks[1 - m_active]; }
    void drainEvents(int deck);
    void onProperty(int deck, const mpv_event_property &property);
    void command(Deck &deck, std::initializer_list<const char *> args);
    void applyVolume(Deck &deck);
    void applyBass(Deck &deck);
    // Loads the song at `index` and plays it from `from` seconds, cutting any fade short.
    void startAt(int index, double from = 0);
    // Gives mpv the song that follows the current one, or plans a fade into it. Without `force`,
    // only when that changed.
    void updateNext(bool force);
    // Called as the active song plays: loads the next song on the other deck shortly before the fade,
    // and starts the fade when its time has come and the song is loaded.
    void checkFade();
    // The fixed fade's length into the next song.
    double fadeLength() const;
    // How the change into the next song goes: measured with Automatic crossfade once both songs are
    // analysed, else the fixed fade.
    crossfade::Plan plan() const;
    // The current song and the next, as the analysis knows them.
    QByteArray planKey() const;
    // Starts measuring the change into the next song once it is near.
    void analyzeNext();
    // Moves the loaded next song to where the plan starts it.
    void cueNext();
    // Swaps the decks and plays the loaded song, overlapping the old one by at most `remaining` s:
    // the time left until the plan stops the old song.
    void beginFade(double remaining);
    void fadeStep();
    // The old song is gone and the new one plays at full volume.
    void finishFade();
    void setMixing(bool mixing);
    // Stops the other deck: an unused preload, or the song fading out.
    void dropOther();
    void setPaused(bool paused);
    void endQueue();
    // Everything the interface shows about the song, after the current one changed.
    void announceSong();
    // Playing means a current song that is not paused and has not ended; mpv reports these apart.
    void updatePlaying();
    void loadPalette();
    void syncUpNext();
    void setError(const QString &error);

    // The context of a deck's wakeup callback.
    struct WakeSlot {
        Player *player = nullptr;
        int index = 0;
    };

    Deck m_decks[2];
    WakeSlot m_wakeSlots[2];
    int m_active = 0;
    CoverStore *m_covers = nullptr;
    PlayQueue m_queue;
    SongModel m_upNext;
    QString m_from;
    // The song after the current one, and whether the change into it fades (on the other deck)
    // rather than staying gapless (on the active one).
    QByteArray m_nextUrl;
    bool m_fadeNext = false;
    // The active song ended before the next one had loaded on the other deck: it plays once loaded.
    bool m_fadeDue = false;
    bool m_fading = false;
    bool m_mixing = false;
    // The plan of the running fade, and how much of its overlap a late start skipped.
    crossfade::Plan m_fadePlan;
    double m_fadeSkip = 0;
    double m_fadeLength = 0;
    // Fade time before the last pause, and the clock since the fade last started or resumed.
    double m_fadeDone = 0;
    QElapsedTimer m_fadeClock;
    QTimer m_fadeTimer;
    // mpv holds nothing: the queue played to its end (the last song stays shown), or it was restored
    // and has not played yet. Play starts the current song at m_position, or from the top at its end.
    bool m_ended = false;
    bool m_paused = true;
    bool m_playing = false;
    double m_position = 0;
    double m_duration = 0;
    int m_volume = 100;
    bool m_shuffle = false;
    int m_repeat = 0;
    int m_crossfade = 0;
    bool m_automatic = false;
    MixAnalyzer *m_analyzer = nullptr;
    QString m_analysisDir;
    // The change the analysis was asked for (planKey), and its plan once both songs are measured;
    // nothing while measuring or when they could not be.
    QByteArray m_planFor;
    std::optional<crossfade::Plan> m_autoPlan;
    bool m_keepAlbumsGapless = true;
    QVariantList m_lyrics;
    bool m_lyricsSynced = false;
    QString m_lyricsFor;
    QString m_paletteCover;
    QVariantList m_palette;
    QColor m_accentLight;
    QColor m_accentDark;
    QString m_error;
};

} // namespace legato

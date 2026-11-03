// The active server's library as QML sees it. Session gives it the server. Pages load their own
// data (pages.h) through its client; Library plays, queues and stars what they show, and keeps
// the playlists the sidebar lists.
#pragma once

#include "models.h"
#include "scrobbletracker.h"

#include <QObject>
#include <QtQmlIntegration>

#include <functional>
#include <optional>

class QNetworkAccessManager;

namespace legato {

class CoverStore;
class LyricsFinder;
class Player;
class QueueSync;
class SubsonicClient;
struct Credentials;

class Library : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool configured READ configured NOTIFY serverChanged)
    Q_PROPERTY(legato::PlaylistModel *playlists READ playlists CONSTANT)
    // Names the server and account, for things kept per server on this computer (pins).
    Q_PROPERTY(QString serverKey READ serverKey NOTIFY serverChanged)
    // Off: the server hears nothing of what plays, neither "now playing" nor counted listens.
    Q_PROPERTY(bool scrobbling READ scrobbling WRITE setScrobbling NOTIFY scrobblingChanged)
    // On: songs the server has no lyrics for are looked up on LRCLIB (their title, artist, album and
    // length leave this computer). Off by default.
    Q_PROPERTY(bool lrclib READ lrclib WRITE setLrclib NOTIFY lrclibChanged)

public:
    explicit Library(QObject *parent = nullptr);
    ~Library() override;

    bool configured() const;
    QString serverKey() const;
    bool scrobbling() const { return m_scrobbling; }
    void setScrobbling(bool on);
    bool lrclib() const { return m_lrclib; }
    void setLrclib(bool on);
    PlaylistModel *playlists() { return &m_playlists; }
    SubsonicClient *client() const { return m_client; }
    CoverStore *covers() const { return m_covers; }
    // Also fetches the lyrics of each song the player starts, and keeps its queue on the server.
    void setPlayer(Player *player);
    // Switches to another server (or none, with empty credentials). Pages reload on serverChanged;
    // answers still on the way from the previous server are dropped.
    void setServer(const Credentials &credentials);

    // Replaces the queue with `songs` and plays the one at `start`, or from the top when it is
    // negative. `shuffle` turns shuffle on first, so the top is a random song.
    void playSongs(const QList<Song> &songs, int start, const QString &from, bool shuffle = false);
    // Play Next (`next`) or Add to Queue.
    void queueSongs(const QList<Song> &songs, bool next);

    // The songs of a list a page shows, from `row` (the top when negative).
    Q_INVOKABLE void playFrom(legato::SongModel *songs, int row, const QString &from, bool shuffle = false);
    // One song of a list, or all of them when `row` is negative.
    Q_INVOKABLE void queueFrom(legato::SongModel *songs, int row, bool next);
    Q_INVOKABLE void playAlbum(const QString &albumId, bool shuffle = false);
    // The album that `song` is on, from that song; the song alone when the album will not load.
    void playAlbumFrom(const QString &albumId, const Song &song);
    Q_INVOKABLE void queueAlbum(const QString &albumId, bool next);
    Q_INVOKABLE void playPlaylist(const QString &playlistId, bool shuffle = false);
    Q_INVOKABLE void loadPlaylists();
    // Playlists (docs/design.md, "Playlists"). `kind` and `id` name what is added: a "song" or an
    // "album" (all its songs), or nothing for an empty new playlist. Each says how it went with
    // notice() or the player bar's error, and lists that changed reload (playlistChanged).
    // Songs already in the playlist are left out, as in other players.
    Q_INVOKABLE void addToPlaylist(const QString &playlistId, const QString &kind, const QString &id);
    Q_INVOKABLE void newPlaylist(const QString &name, const QString &kind = {}, const QString &id = {});
    // The song at `index` of the playlist as the server lists it.
    Q_INVOKABLE void removeFromPlaylist(const QString &playlistId, int index);
    Q_INVOKABLE void renamePlaylist(const QString &playlistId, const QString &name);
    Q_INVOKABLE void deletePlaylist(const QString &playlistId);
    // Whether the user may change the playlist: their own, unless the server says otherwise.
    bool canEdit(const Playlist &playlist) const;
    // Plays 500 random songs from the whole library (the server picks them).
    Q_INVOKABLE void shuffleLibrary();
    // Instant Mix (docs/design.md, "Instant Mix"): the artist and similar artists, mixed. Always
    // built from an artist: the server's mix for a single song was all over the place on a real
    // library. From a song, that song plays first.
    Q_INVOKABLE void instantMix(const QString &artistId, const QString &artistName);
    Q_INVOKABLE void instantMixFrom(legato::SongModel *songs, int row);
    Q_INVOKABLE void instantMixFromPlaying();
    // Stars or unstars the playing song on the server.
    Q_INVOKABLE void toggleFavorite();
    // Shown everywhere at once (starChanged), taken back if the server refuses.
    Q_INVOKABLE void setSongStarred(const QString &id, bool starred);
    Q_INVOKABLE void setAlbumStarred(const QString &id, bool starred);
    Q_INVOKABLE void setArtistStarred(const QString &id, bool starred);

signals:
    void serverChanged();
    void scrobblingChanged();
    void lrclibChanged();
    // A song, album or artist was starred or unstarred; every list showing it follows.
    void starChanged(const QString &id, bool starred);
    // A playlist's songs or name changed.
    void playlistChanged(const QString &id);
    void playlistDeleted(const QString &id);
    // A short confirmation over the player bar ("Added to Road Trip").
    void notice(const QString &text);

private:
    // The ids of the songs `kind` and `id` name (see addToPlaylist); `then` runs only on success.
    void songIdsOf(const QString &kind, const QString &id, std::function<void(QStringList)> then);
    void startMix(const QString &artistId, const QString &artistName, const std::optional<Song> &seed);
    void setStarred(int item, const QString &id, bool starred);
    void reportError(const QString &error);
    // The playing song's lyrics: the server's, else LRCLIB's when that is on.
    void loadLyrics();
    void findLyricsOnline(const Song &song);
    // Reports the playing position to the tracker, and the server what it says.
    void trackListening();
    // Sends the listens not yet delivered, the new one last.
    void submitListens();

    QNetworkAccessManager *m_network;
    SubsonicClient *m_client;
    CoverStore *m_covers;
    LyricsFinder *m_lyrics;
    Player *m_player = nullptr;
    QueueSync *m_queueSync = nullptr;
    PlaylistModel m_playlists;
    ScrobbleTracker m_scrobbles;
    // Listens the server did not get (it was not reachable), resent with the next one.
    QList<Listen> m_unsent;
    bool m_submitting = false;
    bool m_scrobbling = true;
    bool m_lrclib = false;
    // Counts server switches, so a late answer from the previous server is recognized.
    int m_generation = 0;
};

} // namespace legato

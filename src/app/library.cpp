#include "library.h"

#include "coverstore.h"
#include "instantmix.h"
#include "lyricsfinder.h"
#include "player.h"
#include "queuesync.h"
#include "subsonic.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QRandomGenerator>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

// Listens kept for a server that does not answer; the oldest go first.
constexpr int MaxUnsent = 200;

QString coverRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/covers/"_L1;
}

// Automatic crossfade's measurements of songs.
QString mixRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/mix/"_L1;
}

// Answers from LRCLIB.
QString lyricsRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/lyrics"_L1;
}

} // namespace

Library::Library(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_client(new SubsonicClient({}, m_network, this))
    // Without a server nothing is downloaded, so the directory stays empty until setServer.
    , m_covers(new CoverStore(m_client, coverRoot(), this))
    , m_lyrics(new LyricsFinder(m_network, lyricsRoot(), this))
{
}

Library::~Library() = default;

bool Library::configured() const
{
    return m_client->isConfigured();
}

QString Library::serverKey() const
{
    return configured() ? m_client->cacheKey() : QString();
}

void Library::setScrobbling(bool on)
{
    if (m_scrobbling == on)
        return;
    m_scrobbling = on;
    // Listens counted before it went off are not sent after.
    if (!on)
        m_unsent.clear();
    // Counting starts again from now; nothing counts while it is off.
    m_scrobbles.start(on && m_player ? m_player->songId() : QString(), QDateTime::currentMSecsSinceEpoch());
    emit scrobblingChanged();
}

void Library::setServer(const Credentials &credentials)
{
    // The previous server gets its last change before the client points elsewhere.
    if (m_queueSync)
        m_queueSync->flush();
    ++m_generation;
    m_client->setCredentials(credentials);
    if (configured())
        m_covers->setDirectory(coverRoot() + m_client->cacheKey());
    if (m_player)
        m_player->setAnalysisDirectory(configured() ? mixRoot() + m_client->cacheKey() : QString());
    m_playlists.set({});
    // They belong to the previous server's songs.
    m_unsent.clear();
    m_scrobbles.start({}, 0);
    if (m_queueSync)
        m_queueSync->serverChanged();
    emit serverChanged();
    loadPlaylists();
}

void Library::loadPlaylists()
{
    if (!configured())
        return;
    m_client->playlists([this, generation = m_generation](QList<Playlist> playlists, const QString &error) {
        // A failure keeps the list there was; the playlists page says what went wrong.
        if (generation == m_generation && error.isEmpty())
            m_playlists.set(std::move(playlists));
    });
}

void Library::setPlayer(Player *player)
{
    m_player = player;
    if (configured())
        player->setAnalysisDirectory(mixRoot() + m_client->cacheKey());
    m_queueSync = new QueueSync(m_client, player, this);
    // Where the music stopped, for the next start or another client.
    connect(qApp, &QCoreApplication::aboutToQuit, m_queueSync, [this] { m_queueSync->flushAndWait(1500); });
    connect(player, &Player::songChanged, this, &Library::loadLyrics);
    connect(this, &Library::starChanged, player, &Player::setStarred);
    connect(player, &Player::songChanged, this, [this] {
        m_scrobbles.start(m_player->songId(), QDateTime::currentMSecsSinceEpoch());
        // A restored queue shows its song without playing it.
        if (configured() && m_scrobbling && m_player->songLoaded())
            m_client->scrobble({{m_player->songId(), 0}}, false, [](const QString &, bool) {});
    });
    connect(player, &Player::positionChanged, this, &Library::trackListening);
}

void Library::setLrclib(bool on)
{
    if (m_lrclib == on)
        return;
    m_lrclib = on;
    emit lrclibChanged();
    // The song playing now gets its lyrics without waiting for the next one.
    if (on && m_player && m_player->hasSong() && !m_player->lyricsLoading() && m_player->lyrics().isEmpty())
        findLyricsOnline(m_player->song());
}

void Library::loadLyrics()
{
    const QString id = m_player->songId();
    if (id.isEmpty() || !configured())
        return;
    const Song song = m_player->song();
    m_client->lyrics(id, [this, song](const Lyrics &lyrics, const QString &) {
        // A server without the songLyrics extension answers with an error: no lyrics, then.
        if (lyrics.lines.isEmpty() && m_lrclib && song.id == m_player->songId())
            findLyricsOnline(song);
        else
            m_player->setLyrics(song.id, lyrics);
    });
}

void Library::findLyricsOnline(const Song &song)
{
    m_lyrics->find(song, [this, id = song.id](const Lyrics &lyrics) { m_player->setLyrics(id, lyrics); });
}

void Library::trackListening()
{
    if (!configured() || !m_scrobbling)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    switch (m_scrobbles.advance(m_player->position(), m_player->duration(), now)) {
    case ScrobbleTracker::Step::None:
        break;
    case ScrobbleTracker::Step::Restarted:
        m_client->scrobble({{m_scrobbles.songId(), 0}}, false, [](const QString &, bool) {});
        break;
    case ScrobbleTracker::Step::Counted:
        m_unsent.append({m_scrobbles.songId(), m_scrobbles.startedAt()});
        if (m_unsent.size() > MaxUnsent)
            m_unsent.removeFirst();
        submitListens();
        break;
    }
}

void Library::submitListens()
{
    // One request at a time, so a listen is never sent twice.
    if (m_submitting || m_unsent.isEmpty())
        return;
    m_submitting = true;
    const QList<Listen> sending = m_unsent;
    m_client->scrobble(sending, true, [this, sending, generation = m_generation](const QString &error, bool answered) {
        m_submitting = false;
        if (generation != m_generation) {
            submitListens();
            return;
        }
        // Kept only while the server is out of reach. One it refuses would refuse them again.
        if (!error.isEmpty())
            qWarning("legato: scrobble: %s", qPrintable(error));
        if (error.isEmpty() || answered) {
            m_unsent.erase(m_unsent.begin(), m_unsent.begin() + std::min(sending.size(), m_unsent.size()));
            // A listen counted while these were on the way.
            submitListens();
        }
    });
}

void Library::playSongs(const QList<Song> &songs, int start, const QString &from, bool shuffle)
{
    if (!m_player || songs.isEmpty())
        return;
    QList<QUrl> urls;
    urls.reserve(songs.size());
    for (const Song &song : songs)
        urls.append(m_client->streamUrl(song.id));
    if (shuffle) {
        m_player->setShuffle(true);
        start = -1;
    }
    m_player->play(songs, urls, start, from);
}

void Library::queueSongs(const QList<Song> &songs, bool next)
{
    if (!m_player || songs.isEmpty())
        return;
    QList<QUrl> urls;
    urls.reserve(songs.size());
    for (const Song &song : songs)
        urls.append(m_client->streamUrl(song.id));
    m_player->insert(songs, urls, next);
}

void Library::playFrom(SongModel *songs, int row, const QString &from, bool shuffle)
{
    if (songs)
        playSongs(songs->items(), songs->has(row) ? row : -1, from, shuffle);
}

void Library::queueFrom(SongModel *songs, int row, bool next)
{
    if (!songs)
        return;
    queueSongs(songs->has(row) ? QList<Song>{songs->at(row)} : songs->items(), next);
}

void Library::playAlbum(const QString &albumId, bool shuffle)
{
    if (!configured())
        return;
    m_client->album(albumId, [this, shuffle, generation = m_generation](const AlbumWithSongs &album, const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        playSongs(album.songs, -1, album.album.name, shuffle);
    });
}

void Library::playAlbumFrom(const QString &albumId, const Song &song)
{
    if (!configured())
        return;
    if (albumId.isEmpty())
        return playSongs({song}, 0, {});
    m_client->album(albumId, [this, song, generation = m_generation](const AlbumWithSongs &album, const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return playSongs({song}, 0, {});
        const auto at = std::find_if(album.songs.cbegin(), album.songs.cend(),
                                     [&song](const Song &s) { return s.id == song.id; });
        if (at == album.songs.cend())
            return playSongs({song}, 0, {});
        playSongs(album.songs, int(at - album.songs.cbegin()), album.album.name);
    });
}

void Library::queueAlbum(const QString &albumId, bool next)
{
    if (!configured())
        return;
    m_client->album(albumId, [this, next, generation = m_generation](const AlbumWithSongs &album, const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        queueSongs(album.songs, next);
    });
}

void Library::playPlaylist(const QString &playlistId, bool shuffle)
{
    if (!configured())
        return;
    m_client->playlist(playlistId, [this, shuffle, generation = m_generation](const PlaylistWithSongs &playlist, const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        playSongs(playlist.songs, -1, playlist.playlist.name, shuffle);
    });
}

void Library::shuffleLibrary()
{
    if (!configured())
        return;
    m_client->randomSongs(500, [this, generation = m_generation](const QList<Song> &songs, const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        // Already in random order: it starts with the first, shuffle on or off.
        playSongs(songs, 0, tr("Your library"));
    });
}

void Library::instantMix(const QString &artistId, const QString &artistName)
{
    startMix(artistId, artistName, std::nullopt);
}

void Library::instantMixFrom(SongModel *songs, int row)
{
    if (songs && songs->has(row))
        startMix({}, {}, songs->at(row));
}

void Library::instantMixFromPlaying()
{
    if (m_player && m_player->hasSong())
        startMix({}, {}, m_player->song());
}

void Library::startMix(const QString &artistId, const QString &artistName, const std::optional<Song> &seed)
{
    QString id = artistId;
    QString name = artistName;
    // A song's mix is its first artist's.
    if (seed) {
        id = seed->artistId.isEmpty() && !seed->artists.isEmpty() ? seed->artists.first().id : seed->artistId;
        const auto own = std::find_if(seed->artists.cbegin(), seed->artists.cend(),
                                      [&id](const ArtistRef &artist) { return artist.id == id; });
        name = own != seed->artists.cend() ? own->name : seed->artist;
    }
    if (!configured() || id.isEmpty())
        return;
    // 100: enough for a few hours, and what the server finds for one artist in a library of a few
    // hundred albums.
    m_client->similarSongs(id, 100, [this, name, seed, generation = m_generation](const QList<Song> &songs,
                                                                                  const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        const QList<Song> mix = arrangeMix(songs, seed, *QRandomGenerator::global());
        if (mix.size() < 2)
            return reportError(tr("No similar music found for %1").arg(name));
        if (!m_player)
            return;
        // Already mixed; shuffle would only undo the spacing between artists.
        m_player->setShuffle(false);
        const QString from = tr("Instant Mix · %1").arg(name);
        // From the song that plays: it plays on, and the mix follows it.
        if (seed && m_player->hasSong() && seed->id == m_player->songId()) {
            const QList<Song> rest = mix.mid(1);
            QList<QUrl> urls;
            urls.reserve(rest.size());
            for (const Song &song : rest)
                urls.append(m_client->streamUrl(song.id));
            m_player->continueWith(rest, urls, from);
            return;
        }
        playSongs(mix, 0, from);
    });
}

void Library::toggleFavorite()
{
    if (m_player && m_player->hasSong())
        setSongStarred(m_player->songId(), !m_player->starred());
}

void Library::setSongStarred(const QString &id, bool starred)
{
    setStarred(int(SubsonicClient::Item::Song), id, starred);
}

void Library::setAlbumStarred(const QString &id, bool starred)
{
    setStarred(int(SubsonicClient::Item::Album), id, starred);
}

void Library::setArtistStarred(const QString &id, bool starred)
{
    setStarred(int(SubsonicClient::Item::Artist), id, starred);
}

void Library::setStarred(int item, const QString &id, bool starred)
{
    if (!configured() || id.isEmpty())
        return;
    emit starChanged(id, starred);
    m_client->setStarred(SubsonicClient::Item(item), id, starred,
                         [this, id, starred, generation = m_generation](const QString &error) {
        if (error.isEmpty() || generation != m_generation)
            return;
        emit starChanged(id, !starred);
        reportError(error);
    });
}

void Library::reportError(const QString &error)
{
    if (m_player)
        m_player->showError(error);
}

} // namespace legato

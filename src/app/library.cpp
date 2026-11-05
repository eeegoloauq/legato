#include "library.h"

#include "coverstore.h"
#include "diskcache.h"
#include "instantmix.h"
#include "lyricsfinder.h"
#include "player.h"
#include "queuesync.h"
#include "subsonic.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QRandomGenerator>
#include <QSet>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

Q_LOGGING_CATEGORY(lcCache, "legato.cache", QtInfoMsg)

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

// Only the caches' own folders: the cache directory also holds Qt's shader caches, and on Windows
// it is the settings' directory too.
void trimCaches()
{
    QThreadPool::globalInstance()->start([roots = QStringList{coverRoot(), mixRoot(), lyricsRoot()}] {
        if (const qint64 removed = diskcache::trim(roots))
            qCInfo(lcCache, "removed %lld MB of least recently used files", removed >> 20);
    });
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
    // A minute after the start, out of the way of the first pages' covers, then every few hours for
    // a player left open for days.
    QTimer::singleShot(std::chrono::minutes(1), this, trimCaches);
    auto *trimming = new QTimer(this);
    trimming->setInterval(std::chrono::hours(6));
    connect(trimming, &QTimer::timeout, this, trimCaches);
    trimming->start();
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
        if (generation != m_generation || !error.isEmpty())
            return;
        for (Playlist &playlist : playlists)
            playlist.readonly = !canEdit(playlist);
        // Unchanged, the views keep their cards: a reset would build them all again.
        if (playlists == m_playlists.items())
            return;
        m_playlists.set(std::move(playlists));
    });
}

bool Library::canEdit(const Playlist &playlist) const
{
    // Navidrome lists other users' public playlists too, and marks them read only. Other servers
    // only name the owner.
    return !playlist.readonly && (playlist.owner.isEmpty() || playlist.owner == m_client->credentials().username);
}

void Library::songIdsOf(const QString &kind, const QString &id, std::function<void(QStringList)> then)
{
    if (kind == "song"_L1)
        return then({id});
    if (kind != "album"_L1)
        return then({});
    m_client->album(id, [this, then = std::move(then), generation = m_generation](const AlbumWithSongs &album, const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        QStringList ids;
        for (const Song &song : album.songs)
            ids.append(song.id);
        then(ids);
    });
}

void Library::addToPlaylist(const QString &playlistId, const QString &kind, const QString &id)
{
    if (!configured() || playlistId.isEmpty())
        return;
    songIdsOf(kind, id, [this, playlistId, generation = m_generation](const QStringList &ids) {
        // What the playlist holds now, so a song is not added twice.
        m_client->playlist(playlistId, [this, playlistId, ids, generation](const PlaylistWithSongs &playlist, const QString &error) {
            if (generation != m_generation)
                return;
            if (!error.isEmpty())
                return reportError(error);
            QSet<QString> present;
            for (const Song &song : playlist.songs)
                present.insert(song.id);
            QStringList added;
            for (const QString &song : ids) {
                if (!present.contains(song) && !added.contains(song))
                    added.append(song);
            }
            const QString name = playlist.playlist.name;
            if (added.isEmpty())
                return emit notice(ids.size() == 1 ? tr("Already in %1").arg(name) : tr("All of these are already in %1").arg(name));
            m_client->updatePlaylist(playlistId, {}, added, {}, [this, playlistId, name, generation](const QString &error) {
                if (generation != m_generation)
                    return;
                if (!error.isEmpty())
                    return reportError(error);
                emit notice(tr("Added to %1").arg(name));
                emit playlistChanged(playlistId);
                loadPlaylists();
            });
        });
    });
}

void Library::newPlaylist(const QString &name, const QString &kind, const QString &id)
{
    if (!configured() || name.trimmed().isEmpty())
        return;
    songIdsOf(kind, id, [this, name = name.trimmed(), generation = m_generation](const QStringList &ids) {
        m_client->createPlaylist(name, ids, [this, name, generation](const Playlist &, const QString &error) {
            if (generation != m_generation)
                return;
            if (!error.isEmpty())
                return reportError(error);
            emit notice(tr("Created %1").arg(name));
            loadPlaylists();
        });
    });
}

void Library::removeFromPlaylist(const QString &playlistId, int index)
{
    if (!configured() || playlistId.isEmpty() || index < 0)
        return;
    m_client->updatePlaylist(playlistId, {}, {}, {index}, [this, playlistId, generation = m_generation](const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        emit playlistChanged(playlistId);
        loadPlaylists();
    });
}

void Library::renamePlaylist(const QString &playlistId, const QString &name)
{
    if (!configured() || playlistId.isEmpty() || name.trimmed().isEmpty())
        return;
    m_client->updatePlaylist(playlistId, name.trimmed(), {}, {}, [this, playlistId, generation = m_generation](const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        emit playlistChanged(playlistId);
        loadPlaylists();
    });
}

void Library::deletePlaylist(const QString &playlistId)
{
    if (!configured() || playlistId.isEmpty())
        return;
    m_client->deletePlaylist(playlistId, [this, playlistId, generation = m_generation](const QString &error) {
        if (generation != m_generation)
            return;
        if (!error.isEmpty())
            return reportError(error);
        emit playlistDeleted(playlistId);
        loadPlaylists();
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
    if (on && m_player && m_player->hasSong() && !m_player->lyricsLoading() && m_player->lyrics()->count() == 0)
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
        if (error.isEmpty())
            emit listensSent();
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

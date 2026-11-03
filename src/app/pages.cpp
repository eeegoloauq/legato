#include "pages.h"

#include "iteminfo.h"
#include "library.h"
#include "subsonic.h"

#include <QLocale>
#include <QQmlEngine>
#include <QSet>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace legato {

bool isSingleOrEp(const Album &album)
{
    const QStringList &types = album.releaseTypes;
    // Servers without OpenSubsonic's release types: a release of up to three songs is a single.
    if (types.isEmpty())
        return album.songCount > 0 && album.songCount <= 3;
    return !types.contains("album"_L1) && (types.contains("single"_L1) || types.contains("ep"_L1));
}

namespace {

// getAlbumList2's largest page.
constexpr int AlbumPage = 500;

// Newest first: by year, then by when the server added it.
bool newerThan(const Album &a, const Album &b)
{
    if (a.year != b.year)
        return a.year > b.year;
    return a.created > b.created;
}

} // namespace

QString songSummary(const QList<Song> &songs, bool format)
{
    if (songs.isEmpty())
        return {};
    qint64 seconds = 0;
    qint64 bytes = 0;
    for (const Song &song : songs) {
        seconds += song.duration;
        bytes += song.size;
    }
    const qsizetype count = songs.size();
    QString text = count == 1 ? QObject::tr("1 song") : QObject::tr("%1 songs").arg(count);
    text += u", "_s + durationText(seconds);
    if (bytes > 0)
        text += u", "_s + QLocale().formattedDataSize(bytes, 0, QLocale::DataSizeSIFormat);
    if (format) {
        const QString first = subsonic::formatLabel(songs.constFirst());
        const bool shared = std::all_of(songs.begin(), songs.end(),
                                        [&](const Song &song) { return subsonic::formatLabel(song) == first; });
        if (shared && !first.isEmpty())
            text += u" · "_s + first;
    }
    return text;
}

// LibraryQuery

void LibraryQuery::componentComplete()
{
    m_library = qmlEngine(this)->singletonInstance<Library *>("Legato", "Library");
    connect(m_library, &Library::serverChanged, this, [this] {
        clear();
        reload();
    });
    connect(m_library, &Library::starChanged, this, &LibraryQuery::onStarChanged);
    connect(m_library, &Library::playlistChanged, this, &LibraryQuery::onPlaylistChanged);
    reload();
}

SubsonicClient *LibraryQuery::client() const
{
    return m_library->client();
}

void LibraryQuery::reload()
{
    // Answers to the previous load are dropped from here on.
    ++m_request;
    if (!m_library || !m_library->configured() || !canFetch()) {
        finish();
        return;
    }
    m_error.clear();
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }
    fetch();
}

void LibraryQuery::finish(const QString &error)
{
    if (!m_loading && m_error == error)
        return;
    m_loading = false;
    m_error = error;
    emit loadingChanged();
}

// AlbumList

void AlbumList::setType(const QString &type)
{
    if (type == m_type)
        return;
    m_type = type;
    emit typeChanged();
    // Another order of the same albums: the old one would only flash before it.
    clear();
    if (library())
        reload();
}

void AlbumList::setLimit(int limit)
{
    if (limit == m_limit)
        return;
    m_limit = limit;
    emit limitChanged();
    if (library())
        reload();
}

void AlbumList::fetch()
{
    fetchPage(0);
}

void AlbumList::fetchPage(int offset)
{
    const int size = m_limit > 0 ? std::min(AlbumPage, m_limit - offset) : AlbumPage;
    client()->albumList(m_type, size, offset, guard([this, offset, size](const QList<Album> &page, const QString &error) {
        if (!error.isEmpty())
            return finish(error);
        // The first page replaces the old list at once, so Random's refresh never shows it empty. A
        // row of Home updates in place: Recently played, asked again after a listen, keeps its
        // covers and its scroll position where they did not change.
        if (offset == 0 && m_limit > 0)
            m_model.sync(page);
        else if (offset == 0)
            m_model.set(page);
        else
            m_model.append(page);
        const int next = offset + int(page.size());
        if (page.size() == size && (m_limit == 0 || next < m_limit))
            return fetchPage(next);
        finish();
    }));
}

// AlbumDetails

AlbumDetails::AlbumDetails(QObject *parent)
    : LibraryQuery(parent)
{
    m_songs.setDiscs(true);
}

void AlbumDetails::setAlbumId(const QString &id)
{
    if (id == m_id)
        return;
    m_id = id;
    emit albumIdChanged();
    clear();
    if (library())
        reload();
}

QString AlbumDetails::quality() const
{
    // The best of the album's songs, as Apple shows Hi-Res when any song is.
    QString best;
    for (const Song &song : m_songs.items()) {
        const QString label = subsonic::qualityLabel(song);
        if (label == "Hi-Res Lossless"_L1)
            return label;
        if (best.isEmpty() || label == "Lossless"_L1)
            best = label;
    }
    return best;
}

QString AlbumDetails::summary() const
{
    return songSummary(m_songs.items(), true);
}

void AlbumDetails::fetch()
{
    client()->album(m_id, guard([this](AlbumWithSongs details, const QString &error) {
        if (!error.isEmpty())
            return finish(error);
        m_album = std::move(details.album);
        // The album's comment is the songs' when they share one worth showing.
        const QList<Song> &songs = details.songs;
        const bool shared = !songs.isEmpty() && std::all_of(songs.begin(), songs.end(), [&](const Song &song) {
            return song.comment == songs.constFirst().comment;
        });
        m_comment = shared && subsonic::isUsefulComment(songs.constFirst().comment) ? songs.constFirst().comment
                                                                                    : QString();
        m_songs.set(std::move(details.songs));
        emit changed();
        finish();
    }));
}

void AlbumDetails::clear()
{
    m_album = {};
    m_comment.clear();
    m_songs.set({});
    emit changed();
}

void AlbumDetails::onStarChanged(const QString &id, bool starred)
{
    m_songs.setStarred(id, starred);
    if (id == m_album.id && m_album.starred != starred) {
        m_album.starred = starred;
        emit changed();
    }
}

// ArtistList

void ArtistList::fetch()
{
    client()->artists(guard([this](QList<Artist> artists, const QString &error) {
        if (error.isEmpty())
            m_model.set(std::move(artists));
        finish(error);
    }));
}

// ArtistDetails

ArtistDetails::ArtistDetails(QObject *parent)
    : LibraryQuery(parent)
{
}

void ArtistDetails::setArtistId(const QString &id)
{
    if (id == m_id)
        return;
    m_id = id;
    emit artistIdChanged();
    clear();
    if (library())
        reload();
}

void ArtistDetails::fetch()
{
    client()->artist(m_id, guard([this](ArtistWithAlbums details, const QString &error) {
        if (!error.isEmpty())
            return finish(error);
        m_artist = std::move(details.artist);
        QList<Album> releases = std::move(details.albums);
        std::stable_sort(releases.begin(), releases.end(), newerThan);
        QList<Album> albums;
        QList<Album> singles;
        for (const Album &album : releases)
            (isSingleOrEp(album) ? singles : albums).append(album);
        m_albums.set(albums);
        m_singles.set(singles);
        m_latest.clear();
        if (!releases.isEmpty()) {
            const Album &latest = releases.constFirst();
            m_latest = {{u"albumId"_s, latest.id},       {u"name"_s, latest.name},
                        {u"coverArt"_s, latest.coverArt}, {u"year"_s, latest.year},
                        {u"songCount"_s, latest.songCount}, {u"hue"_s, hueOf(latest.id)}};
        }
        emit changed();
        finish();
        findAppearances(releases);
        loadSongs(std::move(releases));
    }));
}

void ArtistDetails::findAppearances(const QList<Album> &own)
{
    QSet<QString> owned;
    for (const Album &album : own)
        owned.insert(album.id);
    client()->searchSongs(m_artist.name, AppearancesSearched, 0, guard([this, owned](const QList<Song> &songs, const QString &) {
        // A failed search leaves the row out, as an empty one.
        QList<Album> albums;
        QSet<QString> seen = owned;
        for (const Song &song : songs) {
            const bool sings = song.artistId == m_id
                || std::any_of(song.artists.cbegin(), song.artists.cend(), [this](const ArtistRef &a) { return a.id == m_id; });
            if (!sings || song.albumId.isEmpty() || seen.contains(song.albumId))
                continue;
            seen.insert(song.albumId);
            Album album;
            album.id = song.albumId;
            album.name = song.album;
            album.coverArt = song.coverArt;
            album.year = song.year;
            album.artist = song.albumArtist;
            album.artists = song.albumArtists;
            if (!song.albumArtists.isEmpty())
                album.artistId = song.albumArtists.constFirst().id;
            albums.append(album);
        }
        std::stable_sort(albums.begin(), albums.end(), newerThan);
        m_appearsOn.set(albums);
    }));
}

void ArtistDetails::loadSongs(QList<Album> releases)
{
    m_songs = QList<QList<Song>>(releases.size());
    if (releases.isEmpty())
        return chooseTopSongs();
    auto waiting = std::make_shared<int>(int(releases.size()));
    for (qsizetype i = 0; i < releases.size(); ++i) {
        client()->album(releases.at(i).id, guard([this, i, waiting](AlbumWithSongs album, const QString &) {
            // An album that fails to load leaves its songs out; the rest still play.
            m_songs[i] = std::move(album.songs);
            if (--*waiting == 0)
                chooseTopSongs();
        }));
    }
}

void ArtistDetails::chooseTopSongs()
{
    QList<Song> played;
    for (const QList<Song> &album : std::as_const(m_songs)) {
        for (const Song &song : album) {
            if (song.playCount > 0)
                played.append(song);
        }
    }
    std::stable_sort(played.begin(), played.end(), [](const Song &a, const Song &b) { return a.playCount > b.playCount; });
    m_songsReady = true;
    if (!played.isEmpty()) {
        m_top.set(played.mid(0, TopCount));
        emit changed();
        return;
    }
    emit changed();
    // Nothing played yet: what Last.fm says is popular, among the songs the server has.
    client()->topSongs(m_artist.name, TopCount, guard([this](QList<Song> songs, const QString &) {
        m_top.set(std::move(songs));
    }));
}

void ArtistDetails::play(bool shuffle)
{
    QList<Song> songs;
    for (const QList<Song> &album : std::as_const(m_songs))
        songs.append(album);
    library()->playSongs(songs, -1, m_artist.name, shuffle);
}

void ArtistDetails::clear()
{
    m_artist = {};
    m_latest.clear();
    m_albums.set({});
    m_singles.set({});
    m_appearsOn.set({});
    m_top.set({});
    m_songs.clear();
    m_songsReady = false;
    emit changed();
}

void ArtistDetails::onStarChanged(const QString &id, bool starred)
{
    m_albums.setStarred(id, starred);
    m_singles.setStarred(id, starred);
    m_top.setStarred(id, starred);
    for (QList<Song> &album : m_songs) {
        for (Song &song : album) {
            if (song.id == id)
                song.starred = starred;
        }
    }
    if (id == m_artist.id && m_artist.starred != starred) {
        m_artist.starred = starred;
        emit changed();
    }
}

// AllSongs

void AllSongs::fetch()
{
    m_fetching = false;
    client()->songCount(guard([this](int count, const QString &) {
        if (count != m_total) {
            m_total = count;
            emit totalChanged();
        }
    }));
    fetchMore();
}

void AllSongs::fetchMore()
{
    if (m_fetching || m_complete || !library() || !library()->configured())
        return;
    m_fetching = true;
    const int offset = m_model.count();
    client()->searchSongs(QString(), PageSize, offset, guard([this](const QList<Song> &page, const QString &error) {
        m_fetching = false;
        if (!error.isEmpty())
            return finish(error);
        m_model.append(page);
        setComplete(page.size() < PageSize);
        finish();
    }));
}

void AllSongs::clear()
{
    m_model.set({});
    m_total = -1;
    emit totalChanged();
    setComplete(false);
}

void AllSongs::setComplete(bool complete)
{
    if (complete == m_complete)
        return;
    m_complete = complete;
    emit completeChanged();
}

// Favorites

void Favorites::fetch()
{
    client()->starred(guard([this](Starred starred, const QString &error) {
        if (error.isEmpty()) {
            m_songs.set(std::move(starred.songs));
            m_albums.set(std::move(starred.albums));
            m_artists.set(std::move(starred.artists));
        }
        finish(error);
    }));
}

void Favorites::clear()
{
    m_songs.set({});
    m_albums.set({});
    m_artists.set({});
}

void Favorites::onStarChanged(const QString &id, bool starred)
{
    m_songs.setStarred(id, starred);
    m_albums.setStarred(id, starred);
    m_artists.setStarred(id, starred);
}

// PlaylistDetails

void PlaylistDetails::setPlaylistId(const QString &id)
{
    if (id == m_id)
        return;
    m_id = id;
    emit playlistIdChanged();
    clear();
    if (library())
        reload();
}

QString PlaylistDetails::updated() const
{
    if (!m_playlist.changed.isValid())
        return {};
    return tr("Updated %1").arg(QLocale().toString(m_playlist.changed.toLocalTime().date(), QLocale::LongFormat));
}

QString PlaylistDetails::summary() const
{
    return songSummary(m_songs.items(), false);
}

bool PlaylistDetails::editable() const
{
    return !m_playlist.id.isEmpty() && library() && library()->canEdit(m_playlist);
}

void PlaylistDetails::onPlaylistChanged(const QString &id)
{
    if (id == m_id)
        reload();
}

void PlaylistDetails::fetch()
{
    client()->playlist(m_id, guard([this](PlaylistWithSongs details, const QString &error) {
        if (!error.isEmpty())
            return finish(error);
        m_playlist = std::move(details.playlist);
        m_songs.set(std::move(details.songs));
        emit changed();
        finish();
    }));
}

void PlaylistDetails::clear()
{
    m_playlist = {};
    m_songs.set({});
    emit changed();
}

// SearchResults

SearchResults::SearchResults(QObject *parent)
    : LibraryQuery(parent)
{
    m_typing.setSingleShot(true);
    m_typing.setInterval(TypingMs);
    connect(&m_typing, &QTimer::timeout, this, &SearchResults::reload);
}

void SearchResults::setQuery(const QString &query)
{
    if (query == m_query)
        return;
    m_query = query;
    emit queryChanged();
    if (!canFetch()) {
        m_typing.stop();
        // Drops answers still on the way.
        reload();
        clear();
        setSearching(false);
        return;
    }
    // Playlists are here already: they follow every key.
    rebuild();
    setSearching(library() && library()->configured());
    m_typing.start();
}

void SearchResults::setSearching(bool searching)
{
    if (searching == m_searching)
        return;
    m_searching = searching;
    emit searchingChanged();
}

void SearchResults::fetch()
{
    client()->search(m_query.trimmed(), Artists, Albums, Songs, guard([this](SearchResult found, const QString &error) {
        if (error.isEmpty()) {
            m_found = std::move(found);
            rebuild();
        }
        finish(error);
        // Only the latest query's answer gets here, and only once typing has paused.
        if (!m_typing.isActive())
            setSearching(false);
    }));
}

void SearchResults::clear()
{
    m_found = {};
    rebuild();
}

void SearchResults::rebuild()
{
    QVariantList items;
    const auto add = [&items](const QString &kind, const QString &group, const QString &id, const QString &title,
                              const QStringList &detail, const QString &coverArt) {
        QStringList parts;
        for (const QString &part : detail) {
            if (!part.isEmpty())
                parts.append(part);
        }
        items.append(QVariantMap{{u"kind"_s, kind},
                                 {u"group"_s, group},
                                 {u"id"_s, id},
                                 {u"title"_s, title},
                                 {u"detail"_s, parts.join(u" · "_s)},
                                 {u"coverArt"_s, coverArt},
                                 {u"hue"_s, hueOf(id)}});
    };
    const QString query = m_query.trimmed();
    if (!query.isEmpty()) {
        // The group's title says what each row is; the detail line does not repeat it.
        for (const Artist &artist : std::as_const(m_found.artists))
            add(u"artist"_s, tr("Artists"), artist.id, artist.name, {}, artist.coverArt);
        for (const Album &album : std::as_const(m_found.albums))
            add(u"album"_s, tr("Albums"), album.id, album.name,
                {album.artist, album.year > 0 ? QString::number(album.year) : QString()}, album.coverArt);
        for (const Song &song : std::as_const(m_found.songs))
            add(u"song"_s, tr("Songs"), song.id, song.title, {song.artist, song.album}, song.coverArt);
        if (library()) {
            int playlists = 0;
            for (const Playlist &playlist : library()->playlists()->items()) {
                if (playlists == Playlists)
                    break;
                if (playlist.name.contains(query, Qt::CaseInsensitive)) {
                    add(u"playlist"_s, tr("Playlists"), playlist.id, playlist.name, {}, playlist.coverArt);
                    ++playlists;
                }
            }
        }
    }
    if (items == m_items)
        return;
    m_items = std::move(items);
    emit itemsChanged();
}

void SearchResults::playSong(const QString &songId)
{
    for (const Song &song : std::as_const(m_found.songs)) {
        if (song.id == songId) {
            library()->playAlbumFrom(song.albumId, song);
            return;
        }
    }
}

// ItemInfo

void ItemInfo::load(const QString &kind, const QString &id)
{
    clear();
    m_kind = kind;
    m_id = id;
    emit changed();
    if (library())
        reload();
}

void ItemInfo::fetch()
{
    if (m_kind == "album"_L1) {
        client()->album(m_id, guard([this](const AlbumWithSongs &details, const QString &error) {
            if (!error.isEmpty())
                return finish(error);
            show(details.album.name, details.album.artist, details.album.coverArt,
                 albumInfo(details.album, details.songs));
            finish();
        }));
        return;
    }
    client()->song(m_id, guard([this](const Song &song, const QString &error) {
        if (!error.isEmpty())
            return finish(error);
        if (song.id.isEmpty())
            return finish(tr("The server did not send the song."));
        show(song.title, song.artist, song.coverArt, songInfo(song));
        finish();
    }));
}

void ItemInfo::show(const QString &title, const QString &artist, const QString &coverArt,
                    const QList<InfoRow> &rows)
{
    m_title = title;
    m_artist = artist;
    m_coverArt = coverArt;
    m_rows.clear();
    for (const InfoRow &row : rows)
        m_rows.append(QVariantMap{{u"label"_s, row.label}, {u"value"_s, row.value}});
    emit changed();
}

void ItemInfo::clear()
{
    m_title.clear();
    m_artist.clear();
    m_coverArt.clear();
    m_rows.clear();
    emit changed();
}

} // namespace legato

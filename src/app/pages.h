// What the library pages show, loaded by the pages themselves: each page declares one of these
// with the id from its route, as lumeo's pages do (`AlbumDetails { albumId: page.route.id }`).
// They load once complete, again when the id changes or Library switches servers, and follow
// stars set anywhere in the app.
#pragma once

#include "iteminfo.h"
#include "models.h"

#include <QPointer>
#include <QQmlParserStatus>
#include <QTimer>
#include <QtQmlIntegration>

namespace legato {

class Library;
class SubsonicClient;

class LibraryQuery : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_INTERFACES(QQmlParserStatus)
    // True until the first answer; a page shows what it has meanwhile.
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    // Why the last load failed, in the server's or the network's words; empty otherwise.
    Q_PROPERTY(QString error READ error NOTIFY loadingChanged)

public:
    using QObject::QObject;

    void classBegin() override {}
    void componentComplete() override;

    bool loading() const { return m_loading; }
    QString error() const { return m_error; }

    Q_INVOKABLE void reload();

signals:
    void loadingChanged();

protected:
    // Sends the requests; their answers end with finish().
    virtual void fetch() = 0;
    // False while a required id is missing.
    virtual bool canFetch() const { return true; }
    // Forgets what the previous server sent.
    virtual void clear() {}
    virtual void onStarChanged(const QString &id, bool starred) = 0;
    // A playlist's songs or name changed.
    virtual void onPlaylistChanged(const QString &) {}

    Library *library() const { return m_library; }
    SubsonicClient *client() const;
    void finish(const QString &error = {});

    // An answer's handler that runs only while this object exists and has not reloaded since.
    template <typename Handler>
    auto guard(Handler handler)
    {
        return [self = QPointer<LibraryQuery>(this), request = m_request,
                handler = std::move(handler)](auto &&...args) mutable {
            if (self && self->m_request == request)
                handler(std::forward<decltype(args)>(args)...);
        };
    }

private:
    Library *m_library = nullptr;
    int m_request = 0;
    bool m_loading = true;
    QString m_error;
};

// Albums in one of the server's orders: getAlbumList2's `type`. Every album, a page at a time, or
// the first `limit`.
class AlbumList : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY limitChanged)
    Q_PROPERTY(legato::AlbumModel *model READ model CONSTANT)

public:
    using LibraryQuery::LibraryQuery;

    QString type() const { return m_type; }
    void setType(const QString &type);
    int limit() const { return m_limit; }
    void setLimit(int limit);
    AlbumModel *model() { return &m_model; }

signals:
    void typeChanged();
    void limitChanged();

protected:
    void fetch() override;
    void clear() override { m_model.set({}); }
    void onStarChanged(const QString &id, bool starred) override { m_model.setStarred(id, starred); }

private:
    void fetchPage(int offset);

    QString m_type = QStringLiteral("alphabeticalByName");
    int m_limit = 0;
    AlbumModel m_model;
};

// One album's page.
class AlbumDetails : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString albumId READ albumId WRITE setAlbumId NOTIFY albumIdChanged)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString artist READ artist NOTIFY changed)
    Q_PROPERTY(QVariantList artists READ artists NOTIFY changed)
    Q_PROPERTY(QString coverArt READ coverArt NOTIFY changed)
    Q_PROPERTY(int year READ year NOTIFY changed)
    Q_PROPERTY(QString genre READ genre NOTIFY changed)
    Q_PROPERTY(QString quality READ quality NOTIFY changed)
    Q_PROPERTY(QString comment READ comment NOTIFY changed)
    // "9 songs, 41 minutes, 312 MB · FLAC · 24-bit / 96 kHz"
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool starred READ starred NOTIFY changed)
    Q_PROPERTY(legato::SongModel *songs READ songs CONSTANT)

public:
    explicit AlbumDetails(QObject *parent = nullptr);

    QString albumId() const { return m_id; }
    void setAlbumId(const QString &id);
    QString name() const { return m_album.name; }
    QString artist() const { return m_album.artist; }
    QVariantList artists() const { return artistList(m_album.artists); }
    QString coverArt() const { return m_album.coverArt; }
    int year() const { return m_album.year; }
    // The main genre: the header has room for one, Get info lists them all.
    QString genre() const { return m_album.genres.value(0); }
    QString quality() const;
    QString comment() const { return m_comment; }
    QString summary() const;
    bool starred() const { return m_album.starred; }
    SongModel *songs() { return &m_songs; }

signals:
    void albumIdChanged();
    void changed();

protected:
    void fetch() override;
    bool canFetch() const override { return !m_id.isEmpty(); }
    void clear() override;
    void onStarChanged(const QString &id, bool starred) override;

private:
    QString m_id;
    Album m_album;
    QString m_comment;
    SongModel m_songs;
};

class ArtistList : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(legato::ArtistModel *model READ model CONSTANT)

public:
    using LibraryQuery::LibraryQuery;

    ArtistModel *model() { return &m_model; }

protected:
    void fetch() override;
    void clear() override { m_model.set({}); }
    void onStarChanged(const QString &id, bool starred) override { m_model.setStarred(id, starred); }

private:
    ArtistModel m_model;
};

// One artist's page: their albums, singles and EPs apart, the latest release and the top songs.
class ArtistDetails : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString artistId READ artistId WRITE setArtistId NOTIFY artistIdChanged)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString coverArt READ coverArt NOTIFY changed)
    Q_PROPERTY(bool starred READ starred NOTIFY changed)
    // The newest release as {albumId, name, coverArt, year, songCount}; empty without one.
    Q_PROPERTY(QVariantMap latest READ latest NOTIFY changed)
    // Newest first.
    Q_PROPERTY(legato::AlbumModel *albums READ albums CONSTANT)
    Q_PROPERTY(legato::AlbumModel *singles READ singles CONSTANT)
    // Other artists' albums with songs of this artist (features, guest spots), newest first.
    Q_PROPERTY(legato::AlbumModel *appearsOn READ appearsOn CONSTANT)
    // The user's most played songs of the artist, else the most popular ones Last.fm knows.
    Q_PROPERTY(legato::SongModel *topSongs READ topSongs CONSTANT)
    // Play and Shuffle work once every album's songs are here.
    Q_PROPERTY(bool songsReady READ songsReady NOTIFY changed)

public:
    // How many top songs the page shows: a 3 by 3 grid.
    static constexpr int TopCount = 9;
    // The most songs search3 gives at once; an artist on more than this many songs may miss some.
    static constexpr int AppearancesSearched = 500;

    explicit ArtistDetails(QObject *parent = nullptr);

    QString artistId() const { return m_id; }
    void setArtistId(const QString &id);
    QString name() const { return m_artist.name; }
    QString coverArt() const { return m_artist.coverArt; }
    bool starred() const { return m_artist.starred; }
    QVariantMap latest() const { return m_latest; }
    AlbumModel *albums() { return &m_albums; }
    AlbumModel *singles() { return &m_singles; }
    AlbumModel *appearsOn() { return &m_appearsOn; }
    SongModel *topSongs() { return &m_top; }
    bool songsReady() const { return m_songsReady; }

    // Every song of the artist's albums, newest album first.
    Q_INVOKABLE void play(bool shuffle = false);

signals:
    void artistIdChanged();
    void changed();

protected:
    void fetch() override;
    bool canFetch() const override { return !m_id.isEmpty(); }
    void clear() override;
    void onStarChanged(const QString &id, bool starred) override;

private:
    void loadSongs(QList<Album> releases);
    void chooseTopSongs();
    // Subsonic has no call for these: search3 finds the songs that name the artist, and the albums
    // of those that are not the artist's own make the list.
    void findAppearances(const QList<Album> &own);

    QString m_id;
    Artist m_artist;
    QVariantMap m_latest;
    AlbumModel m_albums;
    AlbumModel m_singles;
    AlbumModel m_appearsOn;
    SongModel m_top;
    // Album by album, in the order of `releases` given to loadSongs.
    QList<QList<Song>> m_songs;
    bool m_songsReady = false;
};

// Every song of the library, a page at a time as the list scrolls (fetchMore).
class AllSongs : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(legato::SongModel *model READ model CONSTANT)
    // How many songs the library has, -1 when the server does not say.
    Q_PROPERTY(int total READ total NOTIFY totalChanged)
    Q_PROPERTY(bool complete READ complete NOTIFY completeChanged)

public:
    static constexpr int PageSize = 500;

    using LibraryQuery::LibraryQuery;

    SongModel *model() { return &m_model; }
    int total() const { return m_total; }
    bool complete() const { return m_complete; }

    // Loads the next page unless one is on its way or there is none.
    Q_INVOKABLE void fetchMore();

signals:
    void totalChanged();
    void completeChanged();

protected:
    void fetch() override;
    void clear() override;
    void onStarChanged(const QString &id, bool starred) override { m_model.setStarred(id, starred); }

private:
    void setComplete(bool complete);

    SongModel m_model;
    int m_total = -1;
    bool m_complete = false;
    bool m_fetching = false;
};

class Favorites : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(legato::SongModel *songs READ songs CONSTANT)
    Q_PROPERTY(legato::AlbumModel *albums READ albums CONSTANT)
    Q_PROPERTY(legato::ArtistModel *artists READ artists CONSTANT)

public:
    using LibraryQuery::LibraryQuery;

    SongModel *songs() { return &m_songs; }
    AlbumModel *albums() { return &m_albums; }
    ArtistModel *artists() { return &m_artists; }

protected:
    void fetch() override;
    void clear() override;
    // A row unstarred here stays until the page loads again, so a slip is undone in place.
    void onStarChanged(const QString &id, bool starred) override;

private:
    SongModel m_songs;
    AlbumModel m_albums;
    ArtistModel m_artists;
};

class PlaylistDetails : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString playlistId READ playlistId WRITE setPlaylistId NOTIFY playlistIdChanged)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString comment READ comment NOTIFY changed)
    Q_PROPERTY(QString coverArt READ coverArt NOTIFY changed)
    Q_PROPERTY(int hue READ hue NOTIFY changed)
    // "Updated 3 March 2026", or empty when the server does not say.
    Q_PROPERTY(QString updated READ updated NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    // The user's own: songs can be removed, and it can be renamed and deleted.
    Q_PROPERTY(bool editable READ editable NOTIFY changed)
    Q_PROPERTY(legato::SongModel *songs READ songs CONSTANT)

public:
    using LibraryQuery::LibraryQuery;

    QString playlistId() const { return m_id; }
    bool editable() const;
    void setPlaylistId(const QString &id);
    QString name() const { return m_playlist.name; }
    QString comment() const { return m_playlist.comment; }
    QString coverArt() const { return m_playlist.coverArt; }
    int hue() const { return hueOf(m_id); }
    QString updated() const;
    QString summary() const;
    SongModel *songs() { return &m_songs; }

signals:
    void playlistIdChanged();
    void changed();

protected:
    void fetch() override;
    bool canFetch() const override { return !m_id.isEmpty(); }
    void clear() override;
    void onStarChanged(const QString &id, bool starred) override { m_songs.setStarred(id, starred); }
    void onPlaylistChanged(const QString &id) override;

private:
    QString m_id;
    Playlist m_playlist;
    SongModel m_songs;
};

// What Get info shows (docs/design.md, "Get info"): a song from getSong or an album from getAlbum,
// asked for each time the dialog opens, so plays and last played are fresh.
class ItemInfo : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    // "song" or "album".
    Q_PROPERTY(QString kind READ kind NOTIFY changed)
    Q_PROPERTY(QString itemId READ itemId NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString artist READ artist NOTIFY changed)
    Q_PROPERTY(QString coverArt READ coverArt NOTIFY changed)
    // Maps of label and value, in the dialog's order.
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)

public:
    using LibraryQuery::LibraryQuery;

    QString kind() const { return m_kind; }
    QString itemId() const { return m_id; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString coverArt() const { return m_coverArt; }
    QVariantList rows() const { return m_rows; }

    // Forgets the item shown before and asks the server for this one.
    Q_INVOKABLE void load(const QString &kind, const QString &id);

signals:
    void changed();

protected:
    void fetch() override;
    bool canFetch() const override { return !m_id.isEmpty(); }
    void clear() override;
    void onStarChanged(const QString &, bool) override {}

private:
    void show(const QString &title, const QString &artist, const QString &coverArt, const QList<InfoRow> &rows);

    QString m_kind;
    QString m_id;
    QString m_title;
    QString m_artist;
    QString m_coverArt;
    QVariantList m_rows;
};

// Whether the artist page lists the release under Singles & EPs rather than Albums.
bool isSingleOrEp(const Album &album);

// "9 songs, 41 minutes, 312 MB", with " · FLAC · 24-bit / 96 kHz" when `format` and every song
// shares it.
QString songSummary(const QList<Song> &songs, bool format);

// The search palette's matches (docs/design.md, "Search"): artists, albums and songs from the
// server, playlists from the sidebar's list. The server is asked once typing pauses; answers to
// older queries are dropped, and the previous matches stay until the new ones arrive.
class SearchResults : public LibraryQuery
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    // Maps of kind (artist, album, song, playlist), id, title, detail, coverArt, hue, round and
    // group, in the palette's order: Artists, Albums, Songs, Playlists.
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    // From a key press until the server has answered for the query: no "nothing matches" meanwhile.
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)

public:
    static constexpr int TypingMs = 150;
    // A few of each, so the dropdown stays short: what the user wants is near the top, and a
    // longer query narrows it further.
    static constexpr int Artists = 3;
    static constexpr int Albums = 4;
    static constexpr int Songs = 5;
    static constexpr int Playlists = 3;

    explicit SearchResults(QObject *parent = nullptr);

    QString query() const { return m_query; }
    void setQuery(const QString &query);
    QVariantList items() const { return m_items; }
    bool searching() const { return m_searching; }

    // Plays the song's album from the song.
    Q_INVOKABLE void playSong(const QString &songId);

signals:
    void queryChanged();
    void itemsChanged();
    void searchingChanged();

protected:
    void fetch() override;
    bool canFetch() const override { return !m_query.trimmed().isEmpty(); }
    void clear() override;
    void onStarChanged(const QString &, bool) override {}

private:
    void rebuild();
    void setSearching(bool searching);

    QString m_query;
    bool m_searching = false;
    QTimer m_typing;
    SearchResult m_found;
    QVariantList m_items;
};

} // namespace legato

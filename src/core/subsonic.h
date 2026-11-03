// A small client for the (Open)Subsonic REST API.
//
// Only the calls legato needs are implemented. Every request uses token authentication:
// t = md5(password + salt) with a fresh salt, so the password never goes over the wire.
#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace legato {

struct Credentials {
    // Server root, for example https://music.example.org. /rest/... is appended.
    QString baseUrl;
    QString username;
    QString password;

    bool isComplete() const;
    // LEGATO_TEST_SERVER_URL, LEGATO_TEST_USERNAME and LEGATO_TEST_PASSWORD: a server for tests and
    // development that overrides the saved ones.
    static Credentials fromEnvironment();
};

// An artist as a song or an album names it: enough for a link.
struct ArtistRef {
    QString id;
    QString name;

    bool operator==(const ArtistRef &) const = default;
};

struct Album {
    QString id;
    QString name;
    QString artist;
    QString coverArt;
    int year = 0;
    QString artistId;
    // OpenSubsonic: each album artist, for one link each. At least the one of `artist` when it
    // has an id.
    QList<ArtistRef> artists;
    // Every genre, the main one first (OpenSubsonic genres, else the classic genre).
    QStringList genres;
    int songCount = 0;
    int duration = 0; // seconds
    int playCount = 0;
    bool starred = false;
    QDateTime created;
    // When the user last played a song of it; invalid when never (OpenSubsonic).
    QDateTime played;
    // OpenSubsonic: the release date when the tags give the day; invalid otherwise.
    QDate releaseDate;
    QStringList recordLabels;
    // OpenSubsonic, lower case as the tags have them: "album", "single", "ep", "compilation", ...
    QStringList releaseTypes;
};

struct Song {
    QString id;
    QString title;
    QString artist;
    QString album;
    QString coverArt;
    QString suffix;
    int duration = 0;     // seconds
    int bitRate = 0;      // kbit/s
    int bitDepth = 0;     // OpenSubsonic
    int samplingRate = 0; // Hz, OpenSubsonic
    bool starred = false;
    QString albumId;
    QString artistId;
    // OpenSubsonic: each artist of the song. At least the one of `artist` when it has an id.
    QList<ArtistRef> artists;
    // OpenSubsonic: the album's artists, and their names as one line; empty on other servers.
    QList<ArtistRef> albumArtists;
    QString albumArtist;
    int track = 0;
    int disc = 0;
    int year = 0;
    qint64 size = 0; // bytes
    int playCount = 0;
    QString comment;
    // Every genre, the main one first (OpenSubsonic genres, else the classic genre).
    QStringList genres;
    // When the server first saw the file.
    QDateTime created;
    // When the user last played it; invalid when never (OpenSubsonic).
    QDateTime played;
};

struct Artist {
    QString id;
    QString name;
    QString coverArt;
    int albumCount = 0;
    bool starred = false;
};

struct Playlist {
    QString id;
    QString name;
    QString comment;
    QString owner;
    QString coverArt;
    int songCount = 0;
    int duration = 0; // seconds
    QDateTime changed;
    // Navidrome: the user may not change it (another user's public playlist).
    bool readonly = false;

    bool operator==(const Playlist &) const = default;
};

struct AlbumWithSongs {
    Album album;
    QList<Song> songs;
};

struct ArtistWithAlbums {
    Artist artist;
    QList<Album> albums;
};

struct PlaylistWithSongs {
    Playlist playlist;
    QList<Song> songs;
};

// getStarred2: everything the user marked as a favorite.
struct Starred {
    QList<Song> songs;
    QList<Album> albums;
    QList<Artist> artists;
};

// search3: what matches a query, best first.
struct SearchResult {
    QList<Artist> artists;
    QList<Album> albums;
    QList<Song> songs;
};

struct LyricLine {
    // Milliseconds from the start of the song; -1 when the lyrics are not synced.
    qint64 start = -1;
    // Empty for an instrumental gap.
    QString text;
};

struct Lyrics {
    QList<LyricLine> lines;
    bool synced = false;
};

// What a ping says about a server and the credentials sent to it.
// A finished play, for scrobble: the song and when it started (milliseconds since the epoch).
struct Listen {
    QString songId;
    qint64 time = 0;
};

// getPlayQueue: the queue a client saved for this user, in play order.
struct SavedQueue {
    QList<Song> songs;
    QString current;         // song id
    qint64 positionMs = 0;   // in the current song
    QString changedBy;       // the client that saved it
    QDateTime changed;
};

struct PingResult {
    enum Kind {
        Ok,
        // Subsonic errors 40, 41 and 44: the server refuses these credentials or this way of signing in.
        Refused,
        // It answered, but not as a Subsonic server: a web page, a 404, another error.
        NotSubsonic,
        // No answer: no route, connection refused, DNS failure, timeout, TLS failure.
        Unreachable,
    };
    Kind kind = Unreachable;
    // Subsonic's error code for Refused, 0 otherwise.
    int code = 0;
    // The server's or the network's own words, for the log and a detail line.
    QString detail;
};

// The body of a "subsonic-response" envelope, or the reason it could not be used.
struct ApiResult {
    QJsonObject body;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

namespace subsonic {

QString token(const QString &password, const QString &salt);
// What a person types as the server address, made into the server root: "music.example.org" gets
// https://, trailing slashes and a trailing /rest are dropped. Empty when it cannot be an http(s)
// address.
QString normalizeAddress(const QString &input);
// The answer to ping, from its body. A body that is not a Subsonic envelope is NotSubsonic.
PingResult parsePing(const QByteArray &json);
// 16 random bytes as hex. A new salt per request keeps tokens from being replayed.
QString newSalt();
ApiResult parseEnvelope(const QByteArray &json);
QList<Album> parseAlbumList(const QJsonObject &body);
// getAlbum: the album and its songs, whose missing covers fall back to the album's.
AlbumWithSongs parseAlbum(const QJsonObject &body);
QList<Song> parseRandomSongs(const QJsonObject &body);
// getSong: one song, with an empty id when the answer has none.
Song parseSong(const QJsonObject &body);
// getArtists: every artist of every index letter, in the server's order.
QList<Artist> parseArtists(const QJsonObject &body);
ArtistWithAlbums parseArtist(const QJsonObject &body);
QList<Song> parseTopSongs(const QJsonObject &body);
QList<Song> parseSimilarSongs(const QJsonObject &body);
Starred parseStarred(const QJsonObject &body);
QList<Playlist> parsePlaylists(const QJsonObject &body);
PlaylistWithSongs parsePlaylist(const QJsonObject &body);
// search3: the songs only.
QList<Song> parseSearchSongs(const QJsonObject &body);
SearchResult parseSearch(const QJsonObject &body);
// getScanStatus: how many songs the library has, -1 when the server does not say.
int parseSongCount(const QJsonObject &body);
// getLyricsBySongId (OpenSubsonic songLyrics): the first synced entry, else the first one, with its
// offset applied.
Lyrics parseLyrics(const QJsonObject &body);
SavedQueue parsePlayQueue(const QJsonObject &body);
// getOpenSubsonicExtensions: the names, empty for a server without OpenSubsonic.
QStringList parseExtensions(const QJsonObject &body);
// The badge text of the player bar: "Hi-Res Lossless", "Lossless", "MP3 320", ...
QString qualityLabel(const Song &song);
// The format at the end of an album's footer: "FLAC · 24-bit / 96 kHz", "MP3 · 320 kbps".
QString formatLabel(const Song &song);
// A comment worth showing: not a URL or a ripper's or encoder's signature (EAC, "Ripped by", ...).
bool isUsefulComment(const QString &comment);

} // namespace subsonic

class SubsonicClient : public QObject
{
    Q_OBJECT
public:
    // Covers larger than this are rejected rather than read into memory.
    static constexpr qint64 MaxCoverBytes = 20 * 1024 * 1024;

    SubsonicClient(Credentials credentials, QNetworkAccessManager *network, QObject *parent = nullptr);

    bool isConfigured() const { return m_credentials.isComplete(); }
    const Credentials &credentials() const { return m_credentials; }
    // Requests already sent finish against the old server; callers drop their answers.
    void setCredentials(Credentials credentials) { m_credentials = std::move(credentials); }
    // Readable and safe as a directory name; cover ids are only unique per server and user.
    QString cacheKey() const;

    // A full request URL including authentication. Also used for streaming through mpv.
    QUrl url(const QString &endpoint, const QUrlQuery &params = {}) const;
    QUrl streamUrl(const QString &songId) const;

    // Checks the server and the credentials, with a shorter timeout than other calls.
    void ping(std::function<void(PingResult)> done);
    template <typename T>
    using Done = std::function<void(T, QString error)>;

    // One page of getAlbumList2. The server caps size at 500. `type` is the API's: newest,
    // alphabeticalByName, alphabeticalByArtist, byYear (newest first), frequent, random, starred.
    void albumList(const QString &type, int size, int offset, Done<QList<Album>> done);
    void album(const QString &id, Done<AlbumWithSongs> done);
    void song(const QString &id, Done<Song> done);
    // getRandomSongs. The server caps size at 500.
    void randomSongs(int size, Done<QList<Song>> done);
    void artists(Done<QList<Artist>> done);
    void artist(const QString &id, Done<ArtistWithAlbums> done);
    // getTopSongs: the artist's most popular songs as Last.fm knows them, by the artist's name.
    void topSongs(const QString &artistName, int count, Done<QList<Song>> done);
    // getSimilarSongs2: songs by the artist and by similar artists, as the server's metadata agent
    // (Last.fm and the like) knows them. Empty when the server has none.
    void similarSongs(const QString &artistId, int count, Done<QList<Song>> done);
    void starred(Done<Starred> done);
    void playlists(Done<QList<Playlist>> done);
    void playlist(const QString &id, Done<PlaylistWithSongs> done);
    // createPlaylist: a new playlist of the user's with `songIds`. The answer is the new playlist
    // (Subsonic 1.14 and later); an older server sends none, and its id is empty.
    void createPlaylist(const QString &name, const QStringList &songIds, Done<Playlist> done);
    // updatePlaylist: renames it when `name` is not empty, removes the songs at `removeIndexes`
    // (their places before the change) and appends `addIds`. Sent as a POST form when the server
    // takes one: an album's worth of ids makes a long URL.
    void updatePlaylist(const QString &id, const QString &name, const QStringList &addIds,
                        const QList<int> &removeIndexes, std::function<void(QString error)> done);
    void deletePlaylist(const QString &id, std::function<void(QString error)> done);
    // One page of songs from search3. An empty query lists every song (Navidrome, gonic).
    void searchSongs(const QString &query, int size, int offset, Done<QList<Song>> done);
    // search3 for the search palette: up to `artists`, `albums` and `songs` of each.
    void search(const QString &query, int artists, int albums, int songs, Done<SearchResult> done);
    // getScanStatus's count: how many songs the library has, -1 when unknown.
    void songCount(Done<int> done);
    void lyrics(const QString &songId, Done<Lyrics> done);
    enum class Item { Song, Album, Artist };
    // star or unstar.
    void setStarred(Item item, const QString &id, bool starred, std::function<void(QString error)> done);
    // scrobble. Without `submission` it tells the server what is playing now (the first listen's
    // song); with it, it records every listen. `answered` is false when the server was not reached.
    void scrobble(const QList<Listen> &listens, bool submission,
                  std::function<void(QString error, bool answered)> done);
    // getOpenSubsonicExtensions. An error (a plain Subsonic server) is an empty list.
    void extensions(Done<QStringList> done);
    // OpenSubsonic's formPost: parameters may go in a POST body instead of the URL.
    void setFormPost(bool on) { m_formPost = on; }
    bool formPost() const { return m_formPost; }
    void playQueue(Done<SavedQueue> done);
    // savePlayQueue: `ids` in play order; none clears the saved queue. Sent as a POST form when the
    // server takes one (a long queue makes a URL longer than proxies accept).
    void savePlayQueue(const QStringList &ids, const QString &current, qint64 positionMs,
                       std::function<void(QString error)> done);
    // The reply delivers image bytes, or a JSON error envelope with HTTP 200.
    QNetworkReply *coverArt(const QString &id, int size);

private:
    QNetworkReply *get(const QString &endpoint, const QUrlQuery &params);
    QNetworkReply *post(const QString &endpoint, const QUrlQuery &params);
    // A POST form when the server takes one, else a GET; done with the envelope's error.
    void send(const QString &endpoint, const QUrlQuery &params, std::function<void(QString error)> done);
    void callJson(const QString &endpoint, const QUrlQuery &params,
                  std::function<void(ApiResult)> done);
    // callJson with the body parsed by `parse`; an error leaves the value default-constructed.
    template <typename T>
    void fetch(const QString &endpoint, const QUrlQuery &params, T (*parse)(const QJsonObject &),
               Done<T> done);

    Credentials m_credentials;
    QNetworkAccessManager *m_network;
    bool m_formPost = false;
};

QString networkErrorText(QNetworkReply *reply);

} // namespace legato

#include "subsonic.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

// The Subsonic API version legato speaks. 1.16.1 is the base of OpenSubsonic.
constexpr auto ApiVersion = "1.16.1"_L1;
constexpr auto ClientName = "Legato"_L1;
constexpr int TimeoutMs = 30'000;
// A ping at start decides between the app and "Can't reach"; 30 s would be a long blank wait.
constexpr int PingTimeoutMs = 10'000;

QList<QJsonValue> asList(const QJsonValue &value)
{
    // Some servers send a single object instead of a one-element array.
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        return {array.begin(), array.end()};
    }
    if (value.isObject())
        return {value};
    return {};
}

} // namespace

bool Credentials::isComplete() const
{
    return !baseUrl.isEmpty() && !username.isEmpty() && !password.isEmpty();
}

Credentials Credentials::fromEnvironment()
{
    return {qEnvironmentVariable("LEGATO_TEST_SERVER_URL"), qEnvironmentVariable("LEGATO_TEST_USERNAME"),
            qEnvironmentVariable("LEGATO_TEST_PASSWORD")};
}

namespace subsonic {

QString token(const QString &password, const QString &salt)
{
    const QByteArray input = (password + salt).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(input, QCryptographicHash::Md5).toHex());
}

QString newSalt()
{
    QByteArray bytes(16, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(bytes.data()), 4);
    return QString::fromLatin1(bytes.toHex());
}

QString normalizeAddress(const QString &input)
{
    QString text = input.trimmed();
    if (text.isEmpty())
        return {};
    if (!text.contains("://"_L1))
        text.prepend("https://"_L1);
    QUrl url(text, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() || (scheme != "http"_L1 && scheme != "https"_L1))
        return {};
    // Credentials in the address would bypass the password field and end up in the settings file.
    if (!url.userInfo().isEmpty())
        return {};
    url.setScheme(scheme);
    url.setQuery(QString());
    url.setFragment(QString());
    QString path = url.path();
    while (path.endsWith(u'/'))
        path.chop(1);
    if (path.endsWith("/rest"_L1))
        path.chop(5);
    while (path.endsWith(u'/'))
        path.chop(1);
    url.setPath(path);
    return url.toString();
}

PingResult parsePing(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject response = doc.object().value("subsonic-response"_L1).toObject();
    if (parseError.error != QJsonParseError::NoError || response.isEmpty())
        return {PingResult::NotSubsonic, 0, u"no subsonic-response in the answer"_s};
    if (response.value("status"_L1).toString() == "ok"_L1)
        return {PingResult::Ok, 0, {}};
    const QJsonObject error = response.value("error"_L1).toObject();
    const int code = error.value("code"_L1).toInt();
    const QString message = error.value("message"_L1).toString();
    if (code == 40 || code == 41 || code == 44)
        return {PingResult::Refused, code, message};
    return {PingResult::NotSubsonic, 0, u"server error %1: %2"_s.arg(code).arg(message)};
}

ApiResult parseEnvelope(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return {{}, u"unexpected server response: %1"_s.arg(parseError.errorString())};
    const QJsonObject response = doc.object().value("subsonic-response"_L1).toObject();
    if (response.isEmpty())
        return {{}, u"unexpected server response: no subsonic-response"_s};
    const QString status = response.value("status"_L1).toString();
    if (status == "ok"_L1)
        return {response, {}};
    const QJsonObject error = response.value("error"_L1).toObject();
    if (!error.isEmpty())
        return {{}, u"server error %1: %2"_s.arg(error.value("code"_L1).toInt()).arg(error.value("message"_L1).toString())};
    return {{}, u"unexpected server response: status \"%1\" without an error"_s.arg(status)};
}

namespace {

QDateTime parseDate(const QJsonValue &value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

// OpenSubsonic's genres, in the server's order, else the classic genre.
QStringList parseGenres(const QJsonObject &o)
{
    QStringList names;
    for (const QJsonValue &value : o.value("genres"_L1).toArray()) {
        const QString name = value.toObject().value("name"_L1).toString().trimmed();
        if (!name.isEmpty() && !names.contains(name))
            names.append(name);
    }
    const QString classic = o.value("genre"_L1).toString().trimmed();
    if (names.isEmpty() && !classic.isEmpty())
        names.append(classic);
    return names;
}

// OpenSubsonic's ItemDate: {year, month, day}. Invalid unless it has all three: a year alone is
// Album::year already.
QDate parseItemDate(const QJsonValue &value)
{
    const QJsonObject o = value.toObject();
    return QDate(o.value("year"_L1).toInt(), o.value("month"_L1).toInt(), o.value("day"_L1).toInt());
}

// OpenSubsonic's list of artists, or the one the classic fields name.
QList<ArtistRef> parseArtistRefs(const QJsonObject &o, QLatin1StringView listKey)
{
    QList<ArtistRef> refs;
    for (const QJsonValue &value : asList(o.value(listKey))) {
        const QJsonObject a = value.toObject();
        const QString name = a.value("name"_L1).toString();
        if (!name.isEmpty())
            refs.append({a.value("id"_L1).toString(), name});
    }
    if (refs.isEmpty() && !o.value("artistId"_L1).toString().isEmpty())
        refs.append({o.value("artistId"_L1).toString(), o.value("artist"_L1).toString()});
    return refs;
}

Album parseAlbumObject(const QJsonObject &o)
{
    Album album;
    album.id = o.value("id"_L1).toString();
    album.name = o.value("name"_L1).toString();
    album.artist = o.value("artist"_L1).toString();
    album.coverArt = o.value("coverArt"_L1).toString();
    album.year = o.value("year"_L1).toInt();
    album.artistId = o.value("artistId"_L1).toString();
    album.artists = parseArtistRefs(o, "artists"_L1);
    album.genres = parseGenres(o);
    album.songCount = o.value("songCount"_L1).toInt();
    album.duration = o.value("duration"_L1).toInt();
    album.playCount = o.value("playCount"_L1).toInt();
    // The date it was starred, absent otherwise.
    album.starred = o.contains("starred"_L1);
    album.created = parseDate(o.value("created"_L1));
    album.played = parseDate(o.value("played"_L1));
    album.releaseDate = parseItemDate(o.value("releaseDate"_L1));
    for (const QJsonValue &label : o.value("recordLabels"_L1).toArray()) {
        const QString name = label.toObject().value("name"_L1).toString().trimmed();
        if (!name.isEmpty())
            album.recordLabels.append(name);
    }
    for (const QJsonValue &type : o.value("releaseTypes"_L1).toArray())
        album.releaseTypes.append(type.toString().toLower());
    return album;
}

QList<Album> parseAlbums(const QJsonValue &list)
{
    QList<Album> albums;
    for (const QJsonValue &value : asList(list)) {
        Album album = parseAlbumObject(value.toObject());
        if (!album.id.isEmpty())
            albums.append(std::move(album));
    }
    return albums;
}

QList<Song> parseSongs(const QJsonValue &list, const QString &fallbackCover = {})
{
    QList<Song> songs;
    for (const QJsonValue &value : asList(list)) {
        const QJsonObject o = value.toObject();
        Song song;
        song.id = o.value("id"_L1).toString();
        // Folders come back from some calls too.
        if (song.id.isEmpty() || o.value("isDir"_L1).toBool())
            continue;
        song.title = o.value("title"_L1).toString();
        song.artist = o.value("artist"_L1).toString();
        song.album = o.value("album"_L1).toString();
        song.coverArt = o.value("coverArt"_L1).toString(fallbackCover);
        song.suffix = o.value("suffix"_L1).toString();
        song.duration = o.value("duration"_L1).toInt();
        song.bitRate = o.value("bitRate"_L1).toInt();
        song.bitDepth = o.value("bitDepth"_L1).toInt();
        song.samplingRate = o.value("samplingRate"_L1).toInt();
        song.starred = o.contains("starred"_L1);
        song.albumId = o.value("albumId"_L1).toString();
        song.artistId = o.value("artistId"_L1).toString();
        song.artists = parseArtistRefs(o, "artists"_L1);
        song.track = o.value("track"_L1).toInt();
        song.disc = o.value("discNumber"_L1).toInt();
        song.year = o.value("year"_L1).toInt();
        song.size = o.value("size"_L1).toInteger();
        song.playCount = o.value("playCount"_L1).toInt();
        song.comment = o.value("comment"_L1).toString().trimmed();
        song.genres = parseGenres(o);
        song.created = parseDate(o.value("created"_L1));
        song.played = parseDate(o.value("played"_L1));
        songs.append(song);
    }
    return songs;
}

Artist parseArtistObject(const QJsonObject &o)
{
    return {o.value("id"_L1).toString(), o.value("name"_L1).toString(), o.value("coverArt"_L1).toString(),
            o.value("albumCount"_L1).toInt(), o.contains("starred"_L1)};
}

QList<Artist> parseArtistList(const QJsonValue &list)
{
    QList<Artist> artists;
    for (const QJsonValue &value : asList(list)) {
        Artist artist = parseArtistObject(value.toObject());
        if (!artist.id.isEmpty())
            artists.append(std::move(artist));
    }
    return artists;
}

Playlist parsePlaylistObject(const QJsonObject &o)
{
    return {o.value("id"_L1).toString(),
            o.value("name"_L1).toString(),
            o.value("comment"_L1).toString(),
            o.value("owner"_L1).toString(),
            o.value("coverArt"_L1).toString(),
            o.value("songCount"_L1).toInt(),
            o.value("duration"_L1).toInt(),
            parseDate(o.value("changed"_L1)),
            o.value("readonly"_L1).toBool()};
}

} // namespace

QList<Album> parseAlbumList(const QJsonObject &body)
{
    return parseAlbums(body.value("albumList2"_L1).toObject().value("album"_L1));
}

AlbumWithSongs parseAlbum(const QJsonObject &body)
{
    const QJsonObject o = body.value("album"_L1).toObject();
    AlbumWithSongs details{parseAlbumObject(o), parseSongs(o.value("song"_L1), o.value("coverArt"_L1).toString())};
    // Some servers leave the album's artist id out of getAlbum's songs.
    for (Song &song : details.songs) {
        if (song.albumId.isEmpty())
            song.albumId = details.album.id;
    }
    return details;
}

Song parseSong(const QJsonObject &body)
{
    const QList<Song> songs = parseSongs(body.value("song"_L1));
    return songs.isEmpty() ? Song{} : songs.constFirst();
}

QList<Song> parseRandomSongs(const QJsonObject &body)
{
    return parseSongs(body.value("randomSongs"_L1).toObject().value("song"_L1));
}

SavedQueue parsePlayQueue(const QJsonObject &body)
{
    const QJsonObject o = body.value("playQueue"_L1).toObject();
    SavedQueue queue;
    queue.songs = parseSongs(o.value("entry"_L1));
    // Subsonic says string, some servers send a number.
    const QJsonValue current = o.value("current"_L1);
    queue.current = current.isString() ? current.toString() : current.isDouble() ? QString::number(current.toInteger()) : QString();
    queue.positionMs = o.value("position"_L1).toInteger();
    queue.changedBy = o.value("changedBy"_L1).toString();
    queue.changed = QDateTime::fromString(o.value("changed"_L1).toString(), Qt::ISODateWithMs);
    return queue;
}

QStringList parseExtensions(const QJsonObject &body)
{
    QStringList names;
    for (const QJsonValue &value : asList(body.value("openSubsonicExtensions"_L1)))
        names.append(value.toObject().value("name"_L1).toString());
    return names;
}

QList<Artist> parseArtists(const QJsonObject &body)
{
    QList<Artist> artists;
    for (const QJsonValue &index : asList(body.value("artists"_L1).toObject().value("index"_L1)))
        artists.append(parseArtistList(index.toObject().value("artist"_L1)));
    return artists;
}

ArtistWithAlbums parseArtist(const QJsonObject &body)
{
    const QJsonObject o = body.value("artist"_L1).toObject();
    return {parseArtistObject(o), parseAlbums(o.value("album"_L1))};
}

QList<Song> parseTopSongs(const QJsonObject &body)
{
    return parseSongs(body.value("topSongs"_L1).toObject().value("song"_L1));
}

QList<Song> parseSimilarSongs(const QJsonObject &body)
{
    return parseSongs(body.value("similarSongs2"_L1).toObject().value("song"_L1));
}

Starred parseStarred(const QJsonObject &body)
{
    const QJsonObject o = body.value("starred2"_L1).toObject();
    return {parseSongs(o.value("song"_L1)), parseAlbums(o.value("album"_L1)), parseArtistList(o.value("artist"_L1))};
}

QList<Playlist> parsePlaylists(const QJsonObject &body)
{
    QList<Playlist> playlists;
    for (const QJsonValue &value : asList(body.value("playlists"_L1).toObject().value("playlist"_L1))) {
        Playlist playlist = parsePlaylistObject(value.toObject());
        if (!playlist.id.isEmpty())
            playlists.append(std::move(playlist));
    }
    return playlists;
}

PlaylistWithSongs parsePlaylist(const QJsonObject &body)
{
    const QJsonObject o = body.value("playlist"_L1).toObject();
    return {parsePlaylistObject(o), parseSongs(o.value("entry"_L1))};
}

QList<Song> parseSearchSongs(const QJsonObject &body)
{
    return parseSongs(body.value("searchResult3"_L1).toObject().value("song"_L1));
}

SearchResult parseSearch(const QJsonObject &body)
{
    const QJsonObject o = body.value("searchResult3"_L1).toObject();
    return {parseArtistList(o.value("artist"_L1)), parseAlbums(o.value("album"_L1)), parseSongs(o.value("song"_L1))};
}

int parseSongCount(const QJsonObject &body)
{
    return body.value("scanStatus"_L1).toObject().value("count"_L1).toInt(-1);
}

Lyrics parseLyrics(const QJsonObject &body)
{
    const QList<QJsonValue> entries = asList(body.value("lyricsList"_L1).toObject().value("structuredLyrics"_L1));
    QJsonObject chosen;
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        if (chosen.isEmpty() || (entry.value("synced"_L1).toBool() && !chosen.value("synced"_L1).toBool()))
            chosen = entry;
    }
    Lyrics lyrics;
    lyrics.synced = chosen.value("synced"_L1).toBool();
    // Positive means the lyrics appear sooner.
    const qint64 offset = chosen.value("offset"_L1).toInteger();
    for (const QJsonValue &value : asList(chosen.value("line"_L1))) {
        const QJsonObject o = value.toObject();
        LyricLine line;
        line.text = o.value("value"_L1).toString().trimmed();
        if (lyrics.synced)
            line.start = std::max<qint64>(0, o.value("start"_L1).toInteger() - offset);
        lyrics.lines.append(line);
    }
    // Unsynced lyrics have no use for empty lines at either end.
    while (!lyrics.lines.isEmpty() && lyrics.lines.constFirst().text.isEmpty() && !lyrics.synced)
        lyrics.lines.removeFirst();
    while (!lyrics.lines.isEmpty() && lyrics.lines.constLast().text.isEmpty())
        lyrics.lines.removeLast();
    return lyrics;
}

QString qualityLabel(const Song &song)
{
    static const QStringList lossless = {u"flac"_s, u"alac"_s, u"wav"_s, u"aiff"_s, u"aif"_s,
                                         u"ape"_s,  u"wv"_s,   u"dsf"_s, u"dff"_s};
    const QString suffix = song.suffix.toLower();
    if (lossless.contains(suffix)) {
        const bool hiRes = song.bitDepth > 16 || song.samplingRate > 48'000;
        return hiRes ? u"Hi-Res Lossless"_s : u"Lossless"_s;
    }
    if (suffix.isEmpty())
        return {};
    return song.bitRate > 0 ? u"%1 %2"_s.arg(suffix.toUpper()).arg(song.bitRate) : suffix.toUpper();
}

QString formatLabel(const Song &song)
{
    const QString format = song.suffix.toUpper();
    if (format.isEmpty())
        return {};
    if (song.bitDepth > 0 && song.samplingRate > 0) {
        // 44.1 kHz keeps its decimal, 96 kHz has none.
        const QString rate = QString::number(song.samplingRate / 1000.0, 'g', 4);
        return u"%1 \u00b7 %2-bit / %3 kHz"_s.arg(format).arg(song.bitDepth).arg(rate);
    }
    return song.bitRate > 0 ? u"%1 \u00b7 %2 kbps"_s.arg(format).arg(song.bitRate) : format;
}

bool isUsefulComment(const QString &comment)
{
    static const QRegularExpression tool(
        u"^\\s*(https?://|www\\.)|\\b(EAC|XLD|dBpoweramp|CUETools|LAME|ripped|encoded|uploaded)\\b"_s,
        QRegularExpression::CaseInsensitiveOption);
    const QString text = comment.trimmed();
    return !text.isEmpty() && !tool.match(text).hasMatch();
}

} // namespace subsonic

QString networkErrorText(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 400)
        return u"could not reach the server: HTTP %1"_s.arg(status);
    return u"could not reach the server: %1"_s.arg(reply->errorString());
}

SubsonicClient::SubsonicClient(Credentials credentials, QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_credentials(std::move(credentials))
    , m_network(network)
{
}

QString SubsonicClient::cacheKey() const
{
    QString raw = m_credentials.username + u'@' + m_credentials.baseUrl;
    while (raw.endsWith(u'/'))
        raw.chop(1);
    for (QChar &c : raw) {
        const bool safe = (c.isLetterOrNumber() && c.unicode() < 128) || c == u'.' || c == u'-';
        if (!safe)
            c = u'_';
    }
    return raw;
}

QUrl SubsonicClient::url(const QString &endpoint, const QUrlQuery &params) const
{
    QString base = m_credentials.baseUrl;
    while (base.endsWith(u'/'))
        base.chop(1);
    QUrl url(base + "/rest/"_L1 + endpoint);
    const QString salt = subsonic::newSalt();
    QList<std::pair<QString, QString>> items = {
        {u"u"_s, m_credentials.username},
        {u"t"_s, subsonic::token(m_credentials.password, salt)},
        {u"s"_s, salt},
        {u"v"_s, ApiVersion},
        {u"c"_s, ClientName},
        {u"f"_s, u"json"_s},
    };
    for (const auto &[key, value] : params.queryItems(QUrl::FullyDecoded))
        items.append({key, value});
    // Encoded by hand: QUrlQuery leaves '+' as it is, and servers read it as a space.
    QByteArray query;
    for (const auto &[key, value] : items) {
        if (!query.isEmpty())
            query += '&';
        query += QUrl::toPercentEncoding(key) + '=' + QUrl::toPercentEncoding(value);
    }
    url.setQuery(QString::fromLatin1(query), QUrl::StrictMode);
    return url;
}

QUrl SubsonicClient::streamUrl(const QString &songId) const
{
    // format=raw: the server sends the original file and never transcodes.
    return url(u"stream"_s, QUrlQuery{{u"id"_s, songId}, {u"format"_s, u"raw"_s}});
}

QNetworkReply *SubsonicClient::get(const QString &endpoint, const QUrlQuery &params)
{
    QNetworkRequest request(url(endpoint, params));
    request.setTransferTimeout(TimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      u"Legato/%1"_s.arg(QCoreApplication::applicationVersion()));
    return m_network->get(request);
}

QNetworkReply *SubsonicClient::post(const QString &endpoint, const QUrlQuery &params)
{
    // The same parameters as a GET, sign-in included, moved into the body.
    QUrl target = url(endpoint, params);
    const QByteArray body = target.query(QUrl::FullyEncoded).toLatin1();
    target.setQuery(QString());
    QNetworkRequest request(target);
    request.setTransferTimeout(TimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      u"Legato/%1"_s.arg(QCoreApplication::applicationVersion()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
    return m_network->post(request, body);
}

void SubsonicClient::callJson(const QString &endpoint, const QUrlQuery &params,
                              std::function<void(ApiResult)> done)
{
    QNetworkReply *reply = get(endpoint, params);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done({{}, networkErrorText(reply)});
            return;
        }
        done(subsonic::parseEnvelope(reply->readAll()));
    });
}

void SubsonicClient::ping(std::function<void(PingResult)> done)
{
    QNetworkRequest request(url(u"ping"_s));
    request.setTransferTimeout(PingTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      u"Legato/%1"_s.arg(QCoreApplication::applicationVersion()));
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // No HTTP status means nothing answered. A status, even an error, means something did.
        if (reply->error() != QNetworkReply::NoError && status == 0) {
            done({PingResult::Unreachable, 0, reply->errorString()});
            return;
        }
        if (status >= 400) {
            done({PingResult::NotSubsonic, 0, u"HTTP %1"_s.arg(status)});
            return;
        }
        done(subsonic::parsePing(reply->readAll()));
    });
}

template <typename T>
void SubsonicClient::fetch(const QString &endpoint, const QUrlQuery &params, T (*parse)(const QJsonObject &),
                           Done<T> done)
{
    callJson(endpoint, params, [parse, done = std::move(done)](const ApiResult &result) {
        done(result.ok() ? parse(result.body) : T{}, result.error);
    });
}

void SubsonicClient::albumList(const QString &type, int size, int offset, Done<QList<Album>> done)
{
    QUrlQuery params{{u"type"_s, type}, {u"size"_s, QString::number(size)}, {u"offset"_s, QString::number(offset)}};
    // A range that runs backwards lists the newest years first.
    if (type == "byYear"_L1) {
        params.addQueryItem(u"fromYear"_s, u"9999"_s);
        params.addQueryItem(u"toYear"_s, u"0"_s);
    }
    fetch(u"getAlbumList2"_s, params, &subsonic::parseAlbumList, std::move(done));
}

void SubsonicClient::album(const QString &id, Done<AlbumWithSongs> done)
{
    fetch(u"getAlbum"_s, QUrlQuery{{u"id"_s, id}}, &subsonic::parseAlbum, std::move(done));
}

void SubsonicClient::song(const QString &id, Done<Song> done)
{
    fetch(u"getSong"_s, QUrlQuery{{u"id"_s, id}}, &subsonic::parseSong, std::move(done));
}

void SubsonicClient::randomSongs(int size, Done<QList<Song>> done)
{
    fetch(u"getRandomSongs"_s, QUrlQuery{{u"size"_s, QString::number(size)}}, &subsonic::parseRandomSongs,
          std::move(done));
}

void SubsonicClient::artists(Done<QList<Artist>> done)
{
    fetch(u"getArtists"_s, {}, &subsonic::parseArtists, std::move(done));
}

void SubsonicClient::artist(const QString &id, Done<ArtistWithAlbums> done)
{
    fetch(u"getArtist"_s, QUrlQuery{{u"id"_s, id}}, &subsonic::parseArtist, std::move(done));
}

void SubsonicClient::topSongs(const QString &artistName, int count, Done<QList<Song>> done)
{
    fetch(u"getTopSongs"_s, QUrlQuery{{u"artist"_s, artistName}, {u"count"_s, QString::number(count)}},
          &subsonic::parseTopSongs, std::move(done));
}

void SubsonicClient::similarSongs(const QString &artistId, int count, Done<QList<Song>> done)
{
    fetch(u"getSimilarSongs2"_s, QUrlQuery{{u"id"_s, artistId}, {u"count"_s, QString::number(count)}},
          &subsonic::parseSimilarSongs, std::move(done));
}

void SubsonicClient::starred(Done<Starred> done)
{
    fetch(u"getStarred2"_s, {}, &subsonic::parseStarred, std::move(done));
}

void SubsonicClient::playlists(Done<QList<Playlist>> done)
{
    fetch(u"getPlaylists"_s, {}, &subsonic::parsePlaylists, std::move(done));
}

void SubsonicClient::playlist(const QString &id, Done<PlaylistWithSongs> done)
{
    fetch(u"getPlaylist"_s, QUrlQuery{{u"id"_s, id}}, &subsonic::parsePlaylist, std::move(done));
}

void SubsonicClient::searchSongs(const QString &query, int size, int offset, Done<QList<Song>> done)
{
    const QUrlQuery params{{u"query"_s, query},
                           {u"songCount"_s, QString::number(size)},
                           {u"songOffset"_s, QString::number(offset)},
                           {u"albumCount"_s, u"0"_s},
                           {u"artistCount"_s, u"0"_s}};
    fetch(u"search3"_s, params, &subsonic::parseSearchSongs, std::move(done));
}

void SubsonicClient::search(const QString &query, int artists, int albums, int songs, Done<SearchResult> done)
{
    const QUrlQuery params{{u"query"_s, query},
                           {u"artistCount"_s, QString::number(artists)},
                           {u"albumCount"_s, QString::number(albums)},
                           {u"songCount"_s, QString::number(songs)}};
    fetch(u"search3"_s, params, &subsonic::parseSearch, std::move(done));
}

void SubsonicClient::songCount(Done<int> done)
{
    callJson(u"getScanStatus"_s, {}, [done = std::move(done)](const ApiResult &result) {
        done(result.ok() ? subsonic::parseSongCount(result.body) : -1, result.error);
    });
}

void SubsonicClient::lyrics(const QString &songId, Done<Lyrics> done)
{
    fetch(u"getLyricsBySongId"_s, QUrlQuery{{u"id"_s, songId}}, &subsonic::parseLyrics, std::move(done));
}

void SubsonicClient::extensions(Done<QStringList> done)
{
    fetch(u"getOpenSubsonicExtensions"_s, {}, &subsonic::parseExtensions, std::move(done));
}

void SubsonicClient::playQueue(Done<SavedQueue> done)
{
    fetch(u"getPlayQueue"_s, {}, &subsonic::parsePlayQueue, std::move(done));
}

void SubsonicClient::savePlayQueue(const QStringList &ids, const QString &current, qint64 positionMs,
                                   std::function<void(QString)> done)
{
    QUrlQuery params;
    for (const QString &id : ids)
        params.addQueryItem(u"id"_s, id);
    if (!ids.isEmpty()) {
        params.addQueryItem(u"current"_s, current);
        params.addQueryItem(u"position"_s, QString::number(std::max<qint64>(0, positionMs)));
    }
    send(u"savePlayQueue"_s, params, std::move(done));
}

void SubsonicClient::send(const QString &endpoint, const QUrlQuery &params, std::function<void(QString)> done)
{
    QNetworkReply *reply = m_formPost ? post(endpoint, params) : get(endpoint, params);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return done(networkErrorText(reply));
        done(subsonic::parseEnvelope(reply->readAll()).error);
    });
}

void SubsonicClient::createPlaylist(const QString &name, const QStringList &songIds, Done<Playlist> done)
{
    QUrlQuery params{{u"name"_s, name}};
    for (const QString &id : songIds)
        params.addQueryItem(u"songId"_s, id);
    QNetworkReply *reply = m_formPost ? post(u"createPlaylist"_s, params) : get(u"createPlaylist"_s, params);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return done({}, networkErrorText(reply));
        const ApiResult result = subsonic::parseEnvelope(reply->readAll());
        if (!result.error.isEmpty())
            return done({}, result.error);
        done(subsonic::parsePlaylist(result.body).playlist, {});
    });
}

void SubsonicClient::updatePlaylist(const QString &id, const QString &name, const QStringList &addIds,
                                    const QList<int> &removeIndexes, std::function<void(QString)> done)
{
    QUrlQuery params{{u"playlistId"_s, id}};
    if (!name.isEmpty())
        params.addQueryItem(u"name"_s, name);
    for (int index : removeIndexes)
        params.addQueryItem(u"songIndexToRemove"_s, QString::number(index));
    for (const QString &song : addIds)
        params.addQueryItem(u"songIdToAdd"_s, song);
    send(u"updatePlaylist"_s, params, std::move(done));
}

void SubsonicClient::deletePlaylist(const QString &id, std::function<void(QString)> done)
{
    send(u"deletePlaylist"_s, QUrlQuery{{u"id"_s, id}}, std::move(done));
}

void SubsonicClient::setStarred(Item item, const QString &id, bool starred, std::function<void(QString)> done)
{
    const QString key = item == Item::Album ? u"albumId"_s : item == Item::Artist ? u"artistId"_s : u"id"_s;
    callJson(starred ? u"star"_s : u"unstar"_s, QUrlQuery{{key, id}},
             [done = std::move(done)](const ApiResult &result) { done(result.error); });
}

void SubsonicClient::scrobble(const QList<Listen> &listens, bool submission,
                              std::function<void(QString, bool)> done)
{
    if (listens.isEmpty())
        return;
    QUrlQuery params{{u"submission"_s, submission ? u"true"_s : u"false"_s}};
    for (const Listen &listen : submission ? listens : listens.first(1)) {
        params.addQueryItem(u"id"_s, listen.songId);
        if (submission)
            params.addQueryItem(u"time"_s, QString::number(listen.time));
    }
    QNetworkReply *reply = get(u"scrobble"_s, params);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        // No HTTP status means nothing answered, as for ping.
        if (reply->error() != QNetworkReply::NoError) {
            const bool answered = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 0;
            done(networkErrorText(reply), answered);
            return;
        }
        done(subsonic::parseEnvelope(reply->readAll()).error, true);
    });
}

QNetworkReply *SubsonicClient::coverArt(const QString &id, int size)
{
    return get(u"getCoverArt"_s, QUrlQuery{{u"id"_s, id}, {u"size"_s, QString::number(size)}});
}

} // namespace legato

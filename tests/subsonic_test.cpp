#include "subsonic.h"

#include <QNetworkAccessManager>
#include <QTest>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

class SubsonicTest : public QObject
{
    Q_OBJECT

private slots:
    void tokenMatchesSubsonicDocs()
    {
        // Example from https://www.subsonic.org/pages/api.jsp: password "sesame", salt "c19b2d".
        QCOMPARE(subsonic::token(u"sesame"_s, u"c19b2d"_s), u"26719a1196d2a940705a59634eb18eab"_s);
    }

    void saltsDiffer()
    {
        const QString a = subsonic::newSalt();
        QCOMPARE(a.size(), 32);
        QVERIFY(a != subsonic::newSalt());
    }

    void normalizesAddresses()
    {
        QCOMPARE(subsonic::normalizeAddress(u" music.example.org "_s), u"https://music.example.org"_s);
        QCOMPARE(subsonic::normalizeAddress(u"http://10.0.0.5:4533/"_s), u"http://10.0.0.5:4533"_s);
        QCOMPARE(subsonic::normalizeAddress(u"HTTPS://example.org/music/rest/"_s), u"https://example.org/music"_s);
        QCOMPARE(subsonic::normalizeAddress(u"example.org/rest?u=a#x"_s), u"https://example.org"_s);
        QVERIFY(subsonic::normalizeAddress(u""_s).isEmpty());
        QVERIFY(subsonic::normalizeAddress(u"ftp://example.org"_s).isEmpty());
        QVERIFY(subsonic::normalizeAddress(u"https://"_s).isEmpty());
        // A password typed into the address must not reach the settings file.
        QVERIFY(subsonic::normalizeAddress(u"https://me:secret@example.org"_s).isEmpty());
    }

    void classifiesPings()
    {
        QCOMPARE(subsonic::parsePing(R"({"subsonic-response":{"status":"ok","version":"1.16.1"}})").kind,
                 PingResult::Ok);
        const PingResult wrong = subsonic::parsePing(
            R"({"subsonic-response":{"status":"failed","error":{"code":40,"message":"Wrong username or password"}}})");
        QCOMPARE(wrong.kind, PingResult::Refused);
        QCOMPARE(wrong.code, 40);
        QCOMPARE(subsonic::parsePing(R"({"subsonic-response":{"status":"failed","error":{"code":41}}})").kind,
                 PingResult::Refused);
        QCOMPARE(subsonic::parsePing(R"({"subsonic-response":{"status":"failed","error":{"code":0}}})").kind,
                 PingResult::NotSubsonic);
        QCOMPARE(subsonic::parsePing("<html>nginx</html>").kind, PingResult::NotSubsonic);
    }

    void parsesAlbumList()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok","version":"1.16.1","openSubsonic":true,
            "albumList2":{"album":[{"id":"a1","name":"Blue","artist":"Joni Mitchell","coverArt":"al-a1","year":1971},
                                    {"name":"no id"}]}}})";
        const ApiResult result = subsonic::parseEnvelope(json);
        QVERIFY(result.ok());
        const QList<Album> albums = subsonic::parseAlbumList(result.body);
        QCOMPARE(albums.size(), 1);
        QCOMPARE(albums[0].coverArt, u"al-a1"_s);
        QCOMPARE(albums[0].year, 1971);
    }

    void acceptsASingleObjectForAList()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok",
            "album":{"id":"a1","coverArt":"al-a1","song":{"id":"s1","title":"Carey","suffix":"flac"}}}})";
        const QList<Song> songs = subsonic::parseAlbum(subsonic::parseEnvelope(json).body).songs;
        QCOMPARE(songs.size(), 1);
        // A song without its own cover uses the album's.
        QCOMPARE(songs[0].coverArt, u"al-a1"_s);
    }

    void readsStarredSongs()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok","album":{"id":"a1","song":[
            {"id":"s1","starred":"2024-05-01T10:00:00Z"},{"id":"s2"}]}}})";
        const QList<Song> songs = subsonic::parseAlbum(subsonic::parseEnvelope(json).body).songs;
        QCOMPARE(songs.size(), 2);
        QVERIFY(songs[0].starred);
        QVERIFY(!songs[1].starred);
    }

    // Navidrome 0.64's shape: OpenSubsonic artists, release types, disc numbers.
    void parsesAnAlbumPage()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok","openSubsonic":true,"album":{
            "id":"a1","name":"Duets","artist":"A & B","artistId":"ar1","coverArt":"al-a1","year":2020,
            "genre":"Jazz","songCount":2,"duration":400,"created":"2026-11-01T01:47:30.860716597Z",
            "starred":"2026-01-01T00:00:00Z","releaseTypes":["Album","Compilation"],
            "artists":[{"id":"ar1","name":"A"},{"id":"ar2","name":"B"}],
            "song":[{"id":"s1","title":"One","artist":"A","artistId":"ar1","track":1,"discNumber":1,
                     "size":1000,"playCount":3,"comment":"Ripped with EAC"},
                    {"id":"s2","title":"Two","artist":"A feat. B","track":1,"discNumber":2,
                     "artists":[{"id":"ar1","name":"A"},{"id":"ar2","name":"B"}]},
                    {"id":"d1","isDir":true}]}}})";
        const AlbumWithSongs album = subsonic::parseAlbum(subsonic::parseEnvelope(json).body);
        QCOMPARE(album.album.artists, QList<ArtistRef>({{u"ar1"_s, u"A"_s}, {u"ar2"_s, u"B"_s}}));
        QCOMPARE(album.album.releaseTypes, QStringList({u"album"_s, u"compilation"_s}));
        QVERIFY(album.album.starred);
        QCOMPARE(album.album.created.date(), QDate(2026, 11, 1));
        QCOMPARE(album.songs.size(), 2);
        // Without a list, the classic artist and its id make the one link.
        QCOMPARE(album.songs[0].artists, QList<ArtistRef>({{u"ar1"_s, u"A"_s}}));
        QCOMPARE(album.songs[1].artists.size(), 2);
        QCOMPARE(album.songs[1].disc, 2);
        QCOMPARE(album.songs[0].size, 1000);
        QCOMPARE(album.songs[0].playCount, 3);
        // getAlbum's songs belong to the album even when they do not say so.
        QCOMPARE(album.songs[1].albumId, u"a1"_s);
    }

    // getSong and the fields only Get info shows: genres, when it was added and last played, the
    // album's labels and release date.
    void parsesASongAndItsDetails()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok","song":{
            "id":"s1","title":"One","genre":"Rock","genres":[{"name":"Rock"},{"name":"Indie"},{"name":"Rock"}],
            "created":"2026-11-01T01:49:37.22605989Z","played":"2026-11-03T08:55:23.176Z"}}})";
        const Song song = subsonic::parseSong(subsonic::parseEnvelope(json).body);
        QCOMPARE(song.id, u"s1"_s);
        QCOMPARE(song.genres, QStringList({u"Rock"_s, u"Indie"_s}));
        QCOMPARE(song.created.date(), QDate(2026, 11, 1));
        QCOMPARE(song.played.toUTC().date(), QDate(2026, 11, 3));
        QVERIFY(subsonic::parseSong(subsonic::parseEnvelope(R"({"subsonic-response":{"status":"ok"}})").body).id.isEmpty());

        const QByteArray album = R"({"subsonic-response":{"status":"ok","album":{"id":"a1","name":"X",
            "genre":"Jazz","recordLabels":[{"name":"Blue Note"}],"releaseDate":{"year":1959,"month":8,"day":17},
            "song":[]}}})";
        const Album parsed = subsonic::parseAlbum(subsonic::parseEnvelope(album).body).album;
        QCOMPARE(parsed.genres, QStringList{u"Jazz"_s});
        QCOMPARE(parsed.recordLabels, QStringList{u"Blue Note"_s});
        QCOMPARE(parsed.releaseDate, QDate(1959, 8, 17));
        // A year alone is no date: Album::year has it.
        const QByteArray yearOnly = R"({"subsonic-response":{"status":"ok","album":{"id":"a1",
            "releaseDate":{"year":1959}}}})";
        QVERIFY(!subsonic::parseAlbum(subsonic::parseEnvelope(yearOnly).body).album.releaseDate.isValid());
    }

    void parsesArtistsAcrossIndexLetters()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok","artists":{"index":[
            {"name":"A","artist":[{"id":"1","name":"Abba","albumCount":3,"coverArt":"ar-1"}]},
            {"name":"B","artist":{"id":"2","name":"Björk","starred":"x"}}]}}})";
        const QList<Artist> artists = subsonic::parseArtists(subsonic::parseEnvelope(json).body);
        QCOMPARE(artists.size(), 2);
        QCOMPARE(artists[0].albumCount, 3);
        QCOMPARE(artists[1].name, u"Björk"_s);
        QVERIFY(artists[1].starred);
    }

    void parsesStarredAndPlaylists()
    {
        const QByteArray starredJson = R"({"subsonic-response":{"status":"ok","starred2":{
            "song":[{"id":"s1"}],"album":[{"id":"a1"},{"id":"a2"}]}}})";
        const Starred starred = subsonic::parseStarred(subsonic::parseEnvelope(starredJson).body);
        QCOMPARE(starred.songs.size(), 1);
        QCOMPARE(starred.albums.size(), 2);
        QVERIFY(starred.artists.isEmpty());

        const QByteArray playlistJson = R"({"subsonic-response":{"status":"ok","playlist":{
            "id":"p1","name":"Late","comment":"For the night","songCount":1,"duration":200,
            "changed":"2026-03-03T10:00:00Z","coverArt":"pl-p1","entry":[{"id":"s1","title":"x"}]}}})";
        const PlaylistWithSongs playlist = subsonic::parsePlaylist(subsonic::parseEnvelope(playlistJson).body);
        QCOMPARE(playlist.playlist.name, u"Late"_s);
        QCOMPARE(playlist.playlist.changed.date(), QDate(2026, 3, 3));
        QCOMPARE(playlist.songs.size(), 1);

        QCOMPARE(subsonic::parseSongCount(subsonic::parseEnvelope(
                     R"({"subsonic-response":{"status":"ok","scanStatus":{"scanning":false,"count":3762}}})").body),
                 3762);
        QCOMPARE(subsonic::parseSongCount(subsonic::parseEnvelope(
                     R"({"subsonic-response":{"status":"ok","scanStatus":{"scanning":false}}})").body),
                 -1);
    }

    void parsesSearchResults()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok","searchResult3":{
            "artist":{"id":"r1","name":"Low"},"album":[{"id":"a1","name":"Things We Lost"}],
            "song":[{"id":"s1","title":"Sunflower","albumId":"a1"},{"id":"s2","title":"Laser Beam"}]}}})";
        const SearchResult found = subsonic::parseSearch(subsonic::parseEnvelope(json).body);
        QCOMPARE(found.artists.size(), 1);
        QCOMPARE(found.artists.first().name, u"Low"_s);
        QCOMPARE(found.albums.size(), 1);
        QCOMPARE(found.songs.size(), 2);
        QCOMPARE(found.songs.first().albumId, u"a1"_s);

        const SearchResult none = subsonic::parseSearch(
            subsonic::parseEnvelope(R"({"subsonic-response":{"status":"ok","searchResult3":{}}})").body);
        QVERIFY(none.artists.isEmpty() && none.albums.isEmpty() && none.songs.isEmpty());
    }

    void parsesThePlayQueueAndExtensions()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok","playQueue":{
            "current":"s2","position":83500,"username":"u","changed":"2026-11-02T20:15:04.123Z",
            "changedBy":"Symfonium","entry":[{"id":"s1","title":"One"},{"id":"s2","title":"Two"}]}}})";
        const SavedQueue queue = subsonic::parsePlayQueue(subsonic::parseEnvelope(json).body);
        QCOMPARE(queue.songs.size(), 2);
        QCOMPARE(queue.current, u"s2"_s);
        QCOMPARE(queue.positionMs, 83500);
        QCOMPARE(queue.changedBy, u"Symfonium"_s);
        QCOMPARE(queue.changed, QDateTime(QDate(2026, 11, 2), QTime(20, 15, 4, 123), QTimeZone::UTC));

        // A number for current (some servers), and nothing saved.
        QCOMPARE(subsonic::parsePlayQueue(subsonic::parseEnvelope(
                     R"({"subsonic-response":{"status":"ok","playQueue":{"current":42,"entry":[{"id":"42"}]}}})").body).current,
                 u"42"_s);
        QVERIFY(subsonic::parsePlayQueue(
                    subsonic::parseEnvelope(R"({"subsonic-response":{"status":"ok","playQueue":{}}})").body).songs.isEmpty());

        const QStringList names = subsonic::parseExtensions(subsonic::parseEnvelope(
            R"({"subsonic-response":{"status":"ok","openSubsonicExtensions":[
                {"name":"formPost","versions":[1]},{"name":"songLyrics","versions":[1]}]}})").body);
        QCOMPARE(names, (QStringList{u"formPost"_s, u"songLyrics"_s}));
    }

    void labelsFormats()
    {
        Song song;
        song.suffix = u"flac"_s;
        song.bitDepth = 24;
        song.samplingRate = 96000;
        QCOMPARE(subsonic::formatLabel(song), u"FLAC · 24-bit / 96 kHz"_s);
        song.bitDepth = 16;
        song.samplingRate = 44100;
        QCOMPARE(subsonic::formatLabel(song), u"FLAC · 16-bit / 44.1 kHz"_s);
        song = {};
        song.suffix = u"mp3"_s;
        song.bitRate = 320;
        QCOMPARE(subsonic::formatLabel(song), u"MP3 · 320 kbps"_s);
    }

    void hidesToolComments()
    {
        QVERIFY(subsonic::isUsefulComment(u"Recorded live in one take at the Village Vanguard."_s));
        QVERIFY(!subsonic::isUsefulComment(u"https://www.deezer.com/album/123456 "_s));
        QVERIFY(!subsonic::isUsefulComment(u"Exact Audio Copy (EAC) V1.6"_s));
        QVERIFY(!subsonic::isUsefulComment(u"Ripped by someone with XLD"_s));
        QVERIFY(!subsonic::isUsefulComment(u"  "_s));
    }

    void prefersSyncedLyricsAndAppliesTheOffset()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"ok","lyricsList":{"structuredLyrics":[
            {"synced":false,"line":[{"value":"plain"}]},
            {"synced":true,"offset":100,"line":[{"start":50,"value":"first"},{"start":2000,"value":""},
                                                {"start":3000,"value":" last "},{"start":4000,"value":""}]}]}}})";
        const Lyrics lyrics = subsonic::parseLyrics(subsonic::parseEnvelope(json).body);
        QVERIFY(lyrics.synced);
        // The gap in the middle stays as a pause; the one at the end goes.
        QCOMPARE(lyrics.lines.size(), 3);
        // Positive offsets show lines sooner, never before the start.
        QCOMPARE(lyrics.lines[0].start, 0);
        QCOMPARE(lyrics.lines[1].start, 1900);
        QVERIFY(lyrics.lines[1].text.isEmpty());
        QCOMPARE(lyrics.lines[2].text, u"last"_s);
    }

    void readsUnsyncedAndMissingLyrics()
    {
        const QByteArray plain = R"({"subsonic-response":{"status":"ok","lyricsList":{"structuredLyrics":[
            {"synced":false,"line":[{"value":""},{"value":"one"},{"value":"two"}]}]}}})";
        const Lyrics lyrics = subsonic::parseLyrics(subsonic::parseEnvelope(plain).body);
        QVERIFY(!lyrics.synced);
        QCOMPARE(lyrics.lines.size(), 2);
        QCOMPARE(lyrics.lines[0].start, -1);
        const QByteArray none = R"({"subsonic-response":{"status":"ok","lyricsList":{}}})";
        QVERIFY(subsonic::parseLyrics(subsonic::parseEnvelope(none).body).lines.isEmpty());
    }

    void reportsApiErrors()
    {
        const QByteArray json = R"({"subsonic-response":{"status":"failed","version":"1.16.1",
            "error":{"code":40,"message":"Wrong username or password"}}})";
        const ApiResult result = subsonic::parseEnvelope(json);
        QVERIFY(!result.ok());
        QCOMPARE(result.error, u"server error 40: Wrong username or password"_s);
    }

    void reportsBrokenResponses()
    {
        QVERIFY(!subsonic::parseEnvelope("<html>").ok());
        QVERIFY(!subsonic::parseEnvelope(R"({"other":1})").ok());
    }

    void labelsQuality_data()
    {
        QTest::addColumn<QString>("suffix");
        QTest::addColumn<int>("bitRate");
        QTest::addColumn<int>("bitDepth");
        QTest::addColumn<int>("samplingRate");
        QTest::addColumn<QString>("label");
        QTest::newRow("cd flac") << u"flac"_s << 900 << 16 << 44100 << u"Lossless"_s;
        QTest::newRow("hi-res depth") << u"flac"_s << 2000 << 24 << 48000 << u"Hi-Res Lossless"_s;
        QTest::newRow("hi-res rate") << u"ALAC"_s << 0 << 16 << 96000 << u"Hi-Res Lossless"_s;
        QTest::newRow("mp3") << u"mp3"_s << 320 << 0 << 44100 << u"MP3 320"_s;
        QTest::newRow("opus no rate") << u"opus"_s << 0 << 0 << 0 << u"OPUS"_s;
    }

    void labelsQuality()
    {
        QFETCH(QString, suffix);
        QFETCH(int, bitRate);
        QFETCH(int, bitDepth);
        QFETCH(int, samplingRate);
        QFETCH(QString, label);
        Song song;
        song.suffix = suffix;
        song.bitRate = bitRate;
        song.bitDepth = bitDepth;
        song.samplingRate = samplingRate;
        QCOMPARE(subsonic::qualityLabel(song), label);
    }

    void buildsAuthenticatedUrls()
    {
        QNetworkAccessManager network;
        SubsonicClient client({u"https://music.example.org/"_s, u"jo+ni"_s, u"secret"_s}, &network);
        const QUrl url = client.url(u"getAlbum"_s, QUrlQuery{{u"id"_s, u"a+1"_s}});
        QCOMPARE(url.path(), u"/rest/getAlbum"_s);
        const QString query = url.query(QUrl::FullyEncoded);
        // '+' must reach the server encoded, or it reads a space.
        QVERIFY2(query.contains("u=jo%2Bni"_L1), qPrintable(query));
        QVERIFY2(query.contains("id=a%2B1"_L1), qPrintable(query));
        QVERIFY(!query.contains("secret"_L1));
        const QUrlQuery parsed(url);
        QCOMPARE(parsed.queryItemValue(u"t"_s),
                 subsonic::token(u"secret"_s, parsed.queryItemValue(u"s"_s)));
        QCOMPARE(client.cacheKey(), u"jo_ni_https___music.example.org"_s);
    }
};

QTEST_GUILESS_MAIN(SubsonicTest)
#include "subsonic_test.moc"

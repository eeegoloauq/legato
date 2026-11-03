#include "lrclib.h"

#include <QTest>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

class LrclibTest : public QObject
{
    Q_OBJECT

private slots:
    void lrcLines()
    {
        const Lyrics lyrics = lrclib::parseLrc(u"[ar: Someone]\n"
                                               "[00:12.34] First line\n"
                                               "[00:15.5]Second\n"
                                               "[00:20.123] Third\n"
                                               "[01:02:50] Colon hundredths\n"
                                               "not a line\n"
                                               "[01:10.00] \n"_s);
        QVERIFY(lyrics.synced);
        QCOMPARE(lyrics.lines.size(), 4); // the trailing empty line goes
        QCOMPARE(lyrics.lines.at(0).start, 12340);
        QCOMPARE(lyrics.lines.at(0).text, u"First line"_s);
        QCOMPARE(lyrics.lines.at(1).start, 15500);
        QCOMPARE(lyrics.lines.at(2).start, 20123);
        QCOMPARE(lyrics.lines.at(3).start, 62500);
    }

    void repeatedTagsAreSorted()
    {
        const Lyrics lyrics = lrclib::parseLrc(u"[00:30.00][00:10.00]Chorus\n[00:20.00]Verse\n"_s);
        QCOMPARE(lyrics.lines.size(), 3);
        QCOMPARE(lyrics.lines.at(0).text, u"Chorus"_s);
        QCOMPARE(lyrics.lines.at(1).text, u"Verse"_s);
        QCOMPARE(lyrics.lines.at(2).start, 30000);
    }

    void offsetShowsLinesSooner()
    {
        const Lyrics lyrics = lrclib::parseLrc(u"[offset:+500]\n[00:01.00]A\n[00:00.20]B\n"_s);
        QCOMPARE(lyrics.lines.at(0).start, 0); // never before the song
        QCOMPARE(lyrics.lines.at(1).start, 500);
    }

    void openingGapStaysWhenSynced()
    {
        const Lyrics lyrics = lrclib::parseLrc(u"[00:00.00]\n[00:08.00]Words\n"_s);
        QCOMPARE(lyrics.lines.size(), 2);
        QVERIFY(lyrics.lines.at(0).text.isEmpty());
    }

    void plainText()
    {
        const Lyrics lyrics = lrclib::parsePlain(u"\n  One  \n\nTwo\n\n"_s);
        QVERIFY(!lyrics.synced);
        QCOMPARE(lyrics.lines.size(), 3);
        QCOMPARE(lyrics.lines.at(0).text, u"One"_s);
        QCOMPARE(lyrics.lines.at(0).start, -1);
        QVERIFY(lyrics.lines.at(1).text.isEmpty());
    }

    void recordPrefersSynced()
    {
        const lrclib::Result both = lrclib::parseRecord(
            {{u"plainLyrics"_s, u"Hello"_s}, {u"syncedLyrics"_s, u"[00:01.00]Hello"_s}});
        QCOMPARE(both.answer, lrclib::Answer::Lyrics);
        QVERIFY(both.lyrics.synced);

        const lrclib::Result plain = lrclib::parseRecord({{u"plainLyrics"_s, u"Hello"_s}, {u"syncedLyrics"_s, QJsonValue()}});
        QCOMPARE(plain.answer, lrclib::Answer::Lyrics);
        QVERIFY(!plain.lyrics.synced);

        const lrclib::Result instrumental = lrclib::parseRecord({{u"instrumental"_s, true}});
        QCOMPARE(instrumental.answer, lrclib::Answer::Instrumental);
        QVERIFY(instrumental.lyrics.lines.isEmpty());

        QCOMPARE(lrclib::parseRecord({{u"plainLyrics"_s, u"  \n"_s}}).answer, lrclib::Answer::None);
    }

    void searchTakesTheSameLengthWithWords()
    {
        const QJsonArray records{
            QJsonObject{{u"id"_s, 1}, {u"duration"_s, 250.0}, {u"plainLyrics"_s, u"Other edit"_s}},
            QJsonObject{{u"id"_s, 2}, {u"duration"_s, 201.0}, {u"instrumental"_s, true}},
            QJsonObject{{u"id"_s, 3}, {u"duration"_s, 198.5}, {u"plainLyrics"_s, u"This one"_s}},
        };
        QCOMPARE(lrclib::pickSearchRecord(records, 200).value("id"_L1).toInt(), 3);
        QVERIFY(lrclib::pickSearchRecord(records, 100).isEmpty());
    }

    void getSendsOnlyTheSong()
    {
        Song song;
        song.id = u"server-id"_s;
        song.title = u"Karma Police"_s;
        song.artist = u"Radiohead"_s;
        song.album = u"OK Computer"_s;
        song.duration = 264;
        const QUrl url = lrclib::getUrl(song);
        QCOMPARE(url.host(), u"lrclib.net"_s);
        QCOMPARE(url.path(), u"/api/get"_s);
        const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(u"track_name"_s), song.title);
        QCOMPARE(query.queryItemValue(u"artist_name"_s), song.artist);
        QCOMPARE(query.queryItemValue(u"album_name"_s), song.album);
        QCOMPARE(query.queryItemValue(u"duration"_s), u"264"_s);
        QCOMPARE(query.queryItems().size(), 4);
    }
};

QTEST_GUILESS_MAIN(LrclibTest)
#include "lrclib_test.moc"

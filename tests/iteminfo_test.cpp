#include "iteminfo.h"

#include <QLocale>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

QString valueOf(const QList<InfoRow> &rows, const QString &label)
{
    for (const InfoRow &row : rows) {
        if (row.label == label)
            return row.value;
    }
    return {};
}

QStringList labels(const QList<InfoRow> &rows)
{
    QStringList out;
    for (const InfoRow &row : rows)
        out.append(row.label);
    return out;
}

Song flac(const QString &id, int seconds)
{
    Song song;
    song.id = id;
    song.suffix = u"flac"_s;
    song.bitDepth = 24;
    song.samplingRate = 96'000;
    song.bitRate = 2'400;
    song.duration = seconds;
    song.size = 40'000'000;
    return song;
}

} // namespace

class ItemInfoTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
    }

    void songRowsInOrderWithoutEmptyOnes()
    {
        Song song = flac(u"s1"_s, 245);
        song.album = u"Kind of Blue"_s;
        song.track = 3;
        song.year = 1959;
        song.genres = {u"Jazz"_s};
        song.playCount = 1'204;
        song.created = QDateTime(QDate(2026, 11, 1), QTime(12, 0));
        const QList<InfoRow> rows = songInfo(song);
        QCOMPARE(labels(rows), QStringList({u"Album"_s, u"Track"_s, u"Year"_s, u"Genre"_s, u"Length"_s,
                                            u"Format"_s, u"Bit Rate"_s, u"Size"_s, u"Plays"_s, u"Date Added"_s}));
        QCOMPARE(valueOf(rows, u"Length"_s), u"4:05"_s);
        QCOMPARE(valueOf(rows, u"Format"_s), u"FLAC · 24-bit / 96 kHz"_s);
        QCOMPARE(valueOf(rows, u"Bit Rate"_s), u"2,400 kbps"_s);
        QCOMPARE(valueOf(rows, u"Size"_s), u"40.0 MB"_s);
        QCOMPARE(valueOf(rows, u"Plays"_s), u"1,204"_s);
        // The long date, without the weekday.
        QCOMPARE(valueOf(rows, u"Date Added"_s), u"1 November 2026"_s);
        QCOMPARE(valueOf(rows, u"Track"_s), u"3"_s);
    }

    void trackNamesTheDiscBeyondTheFirst()
    {
        Song song;
        song.track = 4;
        song.disc = 2;
        QCOMPARE(valueOf(songInfo(song), u"Track"_s), u"4 on disc 2"_s);
    }

    void longSongsShowHours()
    {
        QCOMPARE(valueOf(songInfo(flac(u"s"_s, 3729)), u"Length"_s), u"1:02:09"_s);
    }

    void lossyFormatLeavesTheBitRateToItsRow()
    {
        Song song;
        song.suffix = u"mp3"_s;
        song.bitRate = 320;
        song.samplingRate = 44'100;
        const QList<InfoRow> rows = songInfo(song);
        QCOMPARE(valueOf(rows, u"Format"_s), u"MP3 · 44.1 kHz"_s);
        QCOMPARE(valueOf(rows, u"Bit Rate"_s), u"320 kbps"_s);
    }

    void albumTotalsFromItsSongs()
    {
        Album album;
        album.year = 1959;
        album.releaseDate = QDate(1959, 8, 17);
        album.recordLabels = {u"Columbia"_s, u"Legacy"_s};
        album.genres = {u"Jazz"_s, u"Modal Jazz"_s};
        const QList<InfoRow> rows = albumInfo(album, {flac(u"1"_s, 1200), flac(u"2"_s, 1260)});
        // All of them, under a plural label.
        QCOMPARE(valueOf(rows, u"Genres"_s), u"Jazz, Modal Jazz"_s);
        QCOMPARE(valueOf(rows, u"Released"_s), u"17 August 1959"_s);
        QCOMPARE(valueOf(rows, u"Songs"_s), u"2"_s);
        QCOMPARE(valueOf(rows, u"Length"_s), u"41 minutes"_s);
        QCOMPARE(valueOf(rows, u"Size"_s), u"80.0 MB"_s);
        QCOMPARE(valueOf(rows, u"Format"_s), u"FLAC · 24-bit / 96 kHz"_s);
        QCOMPARE(valueOf(rows, u"Label"_s), u"Columbia, Legacy"_s);
        // Never played: plays says so, and no last played.
        QCOMPARE(valueOf(rows, u"Plays"_s), u"None"_s);
        QVERIFY(!labels(rows).contains(u"Last Played"_s));
    }

    void albumOfMixedFormatsSaysSo()
    {
        Album album;
        album.year = 2001;
        Song mp3;
        mp3.suffix = u"mp3"_s;
        const QList<InfoRow> rows = albumInfo(album, {flac(u"1"_s, 60), mp3});
        QCOMPARE(valueOf(rows, u"Format"_s), u"Mixed"_s);
        // Without a full date, the year.
        QCOMPARE(valueOf(rows, u"Released"_s), u"2001"_s);
    }
};

QTEST_GUILESS_MAIN(ItemInfoTest)
#include "iteminfo_test.moc"

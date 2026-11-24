#include "player.h"

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <clocale>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

bool writeTone(const QString &path)
{
    // Two seconds of silent PCM, with a duration mpv can read from the file.
    constexpr quint32 SampleRate = 16000;
    const QByteArray samples(2 * SampleRate * sizeof(qint16), '\0');
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + samples.size());
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << SampleRate
        << quint32(SampleRate * sizeof(qint16)) << quint16(sizeof(qint16)) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(samples.size());
    out.writeRawData(samples.constData(), samples.size());
    return out.status() == QDataStream::Ok && file.flush();
}

} // namespace

class PlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { std::setlocale(LC_NUMERIC, "C"); }

    void usesCatalogDurationAcrossNaturalAndManualTrackChanges()
    {
        QTemporaryDir files;
        QVERIFY(files.isValid());
        QList<Song> songs;
        QList<QUrl> urls;
        for (int i = 0; i < 3; ++i) {
            Song song;
            song.id = QString::number(i);
            song.duration = i == 1 ? 3 : 0;
            songs.append(song);
            const QString path = files.filePath(song.id + u".wav"_s);
            QVERIFY(writeTone(path));
            urls.append(QUrl(path));
        }

        Player player;
        QVERIFY(player.available());
        player.setSilentFallback(true);
        player.setCrossfade(5);
        QTest::qWait(100);  // Drain the decks' startup property notifications.
        player.play(songs, urls, 0, {});
        QCOMPARE(player.duration(), 0);
        QTRY_VERIFY(player.position() > 0.1);
        QCOMPARE(player.songId(), u"0"_s);
        QVERIFY(player.canSeek());
        QCOMPARE(player.duration(), 0);

        QTRY_COMPARE(player.songId(), u"1"_s);
        QCOMPARE(player.duration(), 3);
        QTRY_VERIFY(player.canSeek() && player.position() > 0.1);
        QCOMPARE(player.duration(), 3);

        player.next();
        QCOMPARE(player.songId(), u"2"_s);
        QCOMPARE(player.duration(), 0);
        QTRY_VERIFY(player.canSeek() && player.position() > 0.1);
        QCOMPARE(player.duration(), 0);
        QTRY_VERIFY(!player.playing());
        QVERIFY(player.position() > 1);
    }
};

QTEST_GUILESS_MAIN(PlayerTest)
#include "player_test.moc"

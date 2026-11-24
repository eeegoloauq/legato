#include "player.h"

#include <QDataStream>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <clocale>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

QByteArray wave()
{
    // Two seconds of silent PCM, with a duration mpv can read from the file.
    constexpr quint32 SampleRate = 16000;
    const QByteArray samples(2 * SampleRate * sizeof(qint16), '\0');
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + samples.size());
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << SampleRate
        << quint32(SampleRate * sizeof(qint16)) << quint16(sizeof(qint16)) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(samples.size());
    out.writeRawData(samples.constData(), samples.size());
    return data;
}

// Serve distinct HTTP URLs as Navidrome does. Local file paths may be rewritten by
// mpv on different platforms, whereas the queue identifies these streams by URL.
class AudioServer : public QTcpServer
{
public:
    AudioServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket, request = QByteArray{}]() mutable {
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n"))
                        return;
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: "
                                  + QByteArray::number(m_wave.size()) + "\r\nConnection: close\r\n\r\n" + m_wave);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    QUrl stream(int index) const { return QUrl(u"http://127.0.0.1:%1/%2.wav"_s.arg(serverPort()).arg(index)); }

private:
    const QByteArray m_wave = wave();
};

} // namespace

class PlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { std::setlocale(LC_NUMERIC, "C"); }

    void usesCatalogDurationAcrossNaturalAndManualTrackChanges()
    {
        AudioServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QList<Song> songs;
        QList<QUrl> urls;
        for (int i = 0; i < 3; ++i) {
            Song song;
            song.id = QString::number(i);
            song.duration = i == 1 ? 3 : 0;
            songs.append(song);
            urls.append(server.stream(i));
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
        QCOMPARE(player.duration(), 0);

        QTRY_COMPARE(player.songId(), u"1"_s);
        QCOMPARE(player.duration(), 3);
        QTRY_VERIFY(player.position() > 0.1);
        QCOMPARE(player.duration(), 3);

        player.next();
        QCOMPARE(player.songId(), u"2"_s);
        QCOMPARE(player.duration(), 0);
        QTRY_VERIFY(player.position() > 0.1);
        QCOMPARE(player.duration(), 0);
        QTRY_VERIFY(!player.playing());
        QVERIFY(player.position() > 1);
    }
};

QTEST_GUILESS_MAIN(PlayerTest)
#include "player_test.moc"

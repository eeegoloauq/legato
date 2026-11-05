#include "coverstore.h"
#include "subsonic.h"

#include <QMutex>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

// Answers each request with the next status in its list, then with the last one: an image for 200.
class FakeServer : public QTcpServer
{
public:
    explicit FakeServer(QList<int> statuses)
        : m_statuses(std::move(statuses))
    {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    if (!socket->readAll().contains("\r\n\r\n"))
                        return;
                    const int status = m_statuses.at(std::min(int(requests), int(m_statuses.size()) - 1));
                    ++requests;
                    const QByteArray body = status == 200 ? QByteArray("\x89PNG not really") : QByteArray();
                    socket->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: image/png\r\n"
                                  "Content-Length: " + QByteArray::number(body.size())
                                  + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    QString url() const { return u"http://127.0.0.1:%1"_s.arg(serverPort()); }

    std::atomic_int requests = 0;

private:
    QList<int> m_statuses;
};

struct Result {
    QMutex mutex;
    bool done = false;
    QByteArray bytes;
    QString error;

    CoverStore::Done callback()
    {
        return [this](const QByteArray &b, const QString &e) {
            const QMutexLocker lock(&mutex);
            bytes = b;
            error = e;
            done = true;
        };
    }
    bool isDone()
    {
        const QMutexLocker lock(&mutex);
        return done;
    }
};

} // namespace

class CoverStoreTest : public QObject
{
    Q_OBJECT

    QNetworkAccessManager m_network;

    std::unique_ptr<SubsonicClient> clientFor(const FakeServer &server)
    {
        return std::make_unique<SubsonicClient>(Credentials{server.url(), u"test"_s, u"test"_s}, &m_network);
    }

private slots:
    void init()
    {
        CoverStore::retryDelaysMs = {20, 20};
        CoverStore::againEveryMs = 20;
    }

    void asksAgainAfterAServerError()
    {
        FakeServer server({503, 502, 200});
        auto client = clientFor(server);
        QTemporaryDir dir;
        CoverStore store(client.get(), dir.path());
        Result result;
        store.load(u"al-1"_s, 300, nullptr, result.callback());
        QTRY_VERIFY(result.isDone());
        QCOMPARE(result.error, QString());
        QVERIFY(result.bytes.startsWith("\x89PNG"));
        QCOMPARE(server.requests.load(), 3);
        QTRY_VERIFY(!store.cachedFile(u"al-1"_s, 300).isEmpty());
    }

    void takesNotFoundAsTheAnswer()
    {
        FakeServer server({404, 200});
        auto client = clientFor(server);
        QTemporaryDir dir;
        CoverStore store(client.get(), dir.path());
        Result result;
        store.load(u"al-1"_s, 300, nullptr, result.callback());
        QTRY_VERIFY(result.isDone());
        QVERIFY(result.error.contains("404"_L1));
        QTest::qWait(100);
        QCOMPARE(server.requests.load(), 1);
    }

    // Only a cover on screen keeps asking past the retries; the player's waiter gets the error.
    void givesUpWithNoCoverOnScreen()
    {
        FakeServer server({503});
        auto client = clientFor(server);
        QTemporaryDir dir;
        CoverStore store(client.get(), dir.path());
        Result result;
        store.load(u"al-1"_s, 300, nullptr, result.callback());
        QTRY_VERIFY(result.isDone());
        QVERIFY(result.error.contains("503"_L1));
        QCOMPARE(server.requests.load(), 1 + int(CoverStore::retryDelaysMs.size()));
    }

    void keepsAskingWhileOnScreen()
    {
        FakeServer server({503, 503, 503, 503, 503, 200});
        auto client = clientFor(server);
        QTemporaryDir dir;
        CoverStore store(client.get(), dir.path());
        Result result;
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        store.load(u"al-1"_s, 300, cancelled, result.callback());
        QTRY_VERIFY(result.isDone());
        QCOMPARE(result.error, QString());
        QCOMPARE(server.requests.load(), 6);
    }

    void stopsOnceScrolledPast()
    {
        CoverStore::retryDelaysMs = {200};
        FakeServer server({503, 200});
        auto client = clientFor(server);
        QTemporaryDir dir;
        CoverStore store(client.get(), dir.path());
        Result result;
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        store.load(u"al-1"_s, 300, cancelled, result.callback());
        QTRY_COMPARE(server.requests.load(), 1);
        *cancelled = true;
        QTRY_VERIFY(result.isDone());
        QVERIFY(!result.error.isEmpty());
        QTest::qWait(300);
        QCOMPARE(server.requests.load(), 1);
    }

    // A retry still waiting would ask the new server for the old one's id.
    void dropsARetryForAnotherServer()
    {
        CoverStore::retryDelaysMs = {200};
        FakeServer server({503, 200});
        auto client = clientFor(server);
        QTemporaryDir dir;
        CoverStore store(client.get(), dir.path() + u"/a"_s);
        Result result;
        store.load(u"al-1"_s, 300, nullptr, result.callback());
        QTRY_COMPARE(server.requests.load(), 1);
        store.setDirectory(dir.path() + u"/b"_s);
        QTRY_VERIFY(result.isDone());
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(server.requests.load(), 1);
    }
};

QTEST_GUILESS_MAIN(CoverStoreTest)
#include "coverstore_test.moc"

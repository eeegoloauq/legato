#include "coverstore.h"

#include "diskcache.h"
#include "subsonic.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QSaveFile>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

void runInPool(std::function<void()> task)
{
    QThreadPool::globalInstance()->start(std::move(task));
}

QString keyFor(const QString &id, int size)
{
    return id + u'#' + QString::number(size);
}

// Set on a reply legato aborted itself, which is not asked for again.
constexpr char Dropped[] = "legatoDropped";

} // namespace

QList<int> CoverStore::retryDelaysMs{2000, 6000, 20000};
int CoverStore::againEveryMs = 60000;

CoverStore::CoverStore(SubsonicClient *client, QString dir, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_dir(std::move(dir))
{
    QDir().mkpath(m_dir);
}

int CoverStore::bucketFor(int pixels)
{
    if (pixels <= 300)
        return 300;
    if (pixels <= 600)
        return 600;
    return 1200;
}

void CoverStore::setDirectory(const QString &dir)
{
    QDir().mkpath(dir);
    const QMutexLocker lock(&m_dirLock);
    m_dir = dir;
}

QString CoverStore::pathFor(const QString &id, int size) const
{
    const QByteArray hash = QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Md5).toHex();
    const QMutexLocker lock(&m_dirLock);
    return m_dir + u'/' + QString::fromLatin1(hash) + u'-' + QString::number(size);
}

QString CoverStore::cachedFile(const QString &id, int size) const
{
    const QString path = pathFor(id, size);
    return QFileInfo::exists(path) ? path : QString();
}

void CoverStore::load(const QString &id, int size, Cancelled cancelled, Done done)
{
    runInPool([this, id, size, cancelled = std::move(cancelled), done = std::move(done)]() mutable {
        if (cancelled && *cancelled)
            return done({}, u"cancelled"_s);
        QFile file(pathFor(id, size));
        if (file.open(QIODevice::ReadOnly)) {
            diskcache::markUsed(file.fileName());
            return done(file.readAll(), {});
        }
        QMetaObject::invokeMethod(this, [this, id, size, waiter = Waiter{std::move(cancelled), std::move(done)}]() mutable {
            download(id, size, std::move(waiter));
        }, Qt::QueuedConnection);
    });
}

void CoverStore::download(const QString &id, int size, Waiter waiter)
{
    if (!m_client->isConfigured()) {
        runInPool([done = std::move(waiter.done)] { done({}, u"no server configured"_s); });
        return;
    }
    const QString key = keyFor(id, size);
    auto it = m_pending.find(key);
    if (it != m_pending.end()) {
        // Downloading, or waiting to ask again.
        it->waiters.append(std::move(waiter));
        return;
    }
    m_pending.insert(key, {nullptr, {std::move(waiter)}});
    request(id, size);
}

void CoverStore::request(const QString &id, int size)
{
    const QString key = keyFor(id, size);
    QNetworkReply *reply = m_client->coverArt(id, size);
    m_pending[key].reply = reply;

    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > SubsonicClient::MaxCoverBytes || total > SubsonicClient::MaxCoverBytes) {
            reply->setProperty(Dropped, true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, size, path = pathFor(id, size)] {
        reply->deleteLater();
        QByteArray bytes;
        QString error;
        bool again = false;
        if (reply->error() != QNetworkReply::NoError) {
            error = networkErrorText(reply);
            // No answer, a timeout among them, or the server's own failure: likely to pass. A 4xx
            // and what legato aborted itself are not.
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            again = !reply->property(Dropped).toBool() && (status == 0 || status >= 500);
        } else {
            bytes = reply->readAll();
            // Errors arrive as a normal API response with HTTP 200, not as an image.
            if (reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("application/json"_L1)) {
                const ApiResult result = subsonic::parseEnvelope(bytes);
                error = result.ok() ? u"getCoverArt returned JSON without an error"_s : result.error;
                bytes.clear();
            }
        }
        // Another server since: its ids are not this one's.
        if (again && pathFor(id, size) == path && retryLater(id, size))
            return;
        const QList<Waiter> waiters = m_pending.take(keyFor(id, size)).waiters;
        runInPool([bytes, error, path, waiters] {
            if (!bytes.isEmpty()) {
                // QSaveFile writes to a temporary file and renames it, so a crash never leaves half
                // a cover in the cache. A failed write only costs a later download.
                QSaveFile file(path);
                if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size())
                    file.commit();
            }
            for (const Waiter &waiter : waiters)
                waiter.done(bytes, error);
        });
    });

    // Covers scrolled past before they arrive are not downloaded to the end.
    auto *check = new QTimer(reply);
    check->setInterval(250);
    connect(check, &QTimer::timeout, this, [this, key] { dropCancelled(key); });
    check->start();
}

bool CoverStore::retryLater(const QString &id, int size)
{
    Pending &pending = m_pending[keyFor(id, size)];
    const auto waiting = [](const Waiter &w) { return !w.cancelled || !*w.cancelled; };
    // A cover on screen; the player's, the colours' and MPRIS's waiters cannot be cancelled.
    const auto onScreen = [](const Waiter &w) { return w.cancelled && !*w.cancelled; };
    if (std::none_of(pending.waiters.cbegin(), pending.waiters.cend(), waiting))
        return false;
    int pause = againEveryMs;
    if (pending.attempt < retryDelaysMs.size())
        pause = retryDelaysMs.at(pending.attempt++);
    else if (std::none_of(pending.waiters.cbegin(), pending.waiters.cend(), onScreen))
        return false;
    pending.reply = nullptr;
    QTimer::singleShot(pause, this, [this, id, size, path = pathFor(id, size)] {
        const QString key = keyFor(id, size);
        auto it = m_pending.find(key);
        if (it == m_pending.end())
            return;
        const bool wanted = std::any_of(it->waiters.cbegin(), it->waiters.cend(),
                                        [](const Waiter &w) { return !w.cancelled || !*w.cancelled; });
        const bool sameServer = m_client->isConfigured() && pathFor(id, size) == path;
        if (wanted && sameServer)
            return request(id, size);
        const QList<Waiter> waiters = m_pending.take(key).waiters;
        const QString error = wanted ? u"the server changed"_s : u"cancelled"_s;
        runInPool([waiters, error] {
            for (const Waiter &waiter : waiters)
                waiter.done({}, error);
        });
    });
    return true;
}

void CoverStore::dropCancelled(const QString &key)
{
    auto it = m_pending.find(key);
    if (it == m_pending.end())
        return;
    const bool allCancelled = std::all_of(it->waiters.cbegin(), it->waiters.cend(),
                                          [](const Waiter &w) { return w.cancelled && *w.cancelled; });
    if (allCancelled && it->reply) {
        it->reply->setProperty(Dropped, true);
        it->reply->abort();
    }
}

} // namespace legato

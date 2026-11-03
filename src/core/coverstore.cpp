#include "coverstore.h"

#include "subsonic.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QSaveFile>
#include <QThreadPool>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

void runInPool(std::function<void()> task)
{
    QThreadPool::globalInstance()->start(std::move(task));
}

} // namespace

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
        if (file.open(QIODevice::ReadOnly))
            return done(file.readAll(), {});
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
    const QString key = id + u'#' + QString::number(size);
    auto it = m_pending.find(key);
    if (it != m_pending.end()) {
        it->waiters.append(std::move(waiter));
        return;
    }
    QNetworkReply *reply = m_client->coverArt(id, size);
    m_pending.insert(key, {reply, {std::move(waiter)}});

    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > SubsonicClient::MaxCoverBytes || total > SubsonicClient::MaxCoverBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, path = pathFor(id, size)] {
        reply->deleteLater();
        const QList<Waiter> waiters = m_pending.take(key).waiters;
        QByteArray bytes;
        QString error;
        if (reply->error() != QNetworkReply::NoError) {
            error = networkErrorText(reply);
        } else {
            bytes = reply->readAll();
            // Errors arrive as a normal API response with HTTP 200, not as an image.
            if (reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("application/json"_L1)) {
                const ApiResult result = subsonic::parseEnvelope(bytes);
                error = result.ok() ? u"getCoverArt returned JSON without an error"_s : result.error;
                bytes.clear();
            }
        }
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

void CoverStore::dropCancelled(const QString &key)
{
    auto it = m_pending.find(key);
    if (it == m_pending.end())
        return;
    const bool allCancelled = std::all_of(it->waiters.cbegin(), it->waiters.cend(),
                                          [](const Waiter &w) { return w.cancelled && *w.cancelled; });
    if (allCancelled && it->reply)
        it->reply->abort();
}

} // namespace legato

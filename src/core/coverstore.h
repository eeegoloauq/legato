// Disk cache for cover art.
//
// Each cover is stored once per size bucket. File names are hashes of the server's cover id,
// so ids from the server can never form a path.
#pragma once

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>

class QNetworkReply;

namespace legato {

class SubsonicClient;

class CoverStore : public QObject
{
    Q_OBJECT
public:
    using Cancelled = std::shared_ptr<std::atomic_bool>;
    // Called on a thread-pool thread with the encoded image, or with an error.
    using Done = std::function<void(QByteArray bytes, QString error)>;

    // dir should be specific to one server, because cover ids are only unique per server.
    CoverStore(SubsonicClient *client, QString dir, QObject *parent = nullptr);

    // The server size to ask for when `pixels` are shown. Few buckets keep the cache small.
    static int bucketFor(int pixels);

    // Another server's covers. Downloads already running finish into the old directory.
    void setDirectory(const QString &dir);

    // The cover's file when it is on disk already, else empty. For other programs to show it
    // (MPRIS); the name has no extension, readers go by content.
    QString cachedFile(const QString &id, int size) const;
    // Thread-safe. Reads the cover from disk, or downloads and stores it. `cancelled` may be null.
    void load(const QString &id, int size, Cancelled cancelled, Done done);

private:
    struct Waiter {
        Cancelled cancelled;
        Done done;
    };
    struct Pending {
        QPointer<QNetworkReply> reply;
        QList<Waiter> waiters;
    };

    QString pathFor(const QString &id, int size) const;
    void download(const QString &id, int size, Waiter waiter); // GUI thread only
    void dropCancelled(const QString &key);                    // GUI thread only

    SubsonicClient *m_client;
    mutable QMutex m_dirLock;
    QString m_dir;
    QHash<QString, Pending> m_pending;
};

} // namespace legato

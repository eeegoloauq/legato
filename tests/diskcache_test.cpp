#include "diskcache.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

void setUsed(const QString &path, const QDateTime &when)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(when, QFileDevice::FileModificationTime));
}

// A file of `size` bytes, last used `daysAgo` days ago.
void put(const QString &path, qint64 size, int daysAgo)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(size, 'x'));
    file.close();
    setUsed(path, QDateTime::currentDateTime().addDays(-daysAgo));
}

QDateTime used(const QString &path)
{
    return QFileInfo(path).lastModified();
}

} // namespace

class DiskCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void keepsCachesUnderTheLimit()
    {
        QTemporaryDir dir;
        const QString covers = dir.filePath(u"covers"_s), lyrics = dir.filePath(u"lyrics"_s);
        put(covers + u"/server/old"_s, 400, 30);
        put(lyrics + u"/older"_s, 300, 40);
        put(covers + u"/server/recent"_s, 400, 2);
        put(covers + u"/other/newest"_s, 100, 0);

        // 1200 bytes, 700 allowed: the two oldest go, across folders.
        QCOMPARE(diskcache::trim({covers, lyrics}, 700), 700);
        QVERIFY(!QFile::exists(lyrics + u"/older"_s));
        QVERIFY(!QFile::exists(covers + u"/server/old"_s));
        QVERIFY(QFile::exists(covers + u"/server/recent"_s));
        QVERIFY(QFile::exists(covers + u"/other/newest"_s));
        // Folders stay for the caches to write into.
        QVERIFY(QFileInfo(lyrics).isDir());
    }

    void leavesACacheUnderTheLimitAlone()
    {
        QTemporaryDir dir;
        put(dir.filePath(u"covers/a"_s), 100, 10);
        QCOMPARE(diskcache::trim({dir.filePath(u"covers"_s), dir.filePath(u"missing"_s)}, 100), 0);
        QVERIFY(QFile::exists(dir.filePath(u"covers/a"_s)));
    }

    void leavesOtherFoldersAlone()
    {
        QTemporaryDir dir;
        put(dir.filePath(u"covers/a"_s), 100, 1);
        put(dir.filePath(u"qt/lib"_s), 1000, 100);
        QCOMPARE(diskcache::trim({dir.filePath(u"covers"_s)}, 0), 100);
        QVERIFY(QFile::exists(dir.filePath(u"qt/lib"_s)));
    }

    void usedFilesStay()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(u"a"_s), b = dir.filePath(u"b"_s);
        put(a, 100, 20);
        put(b, 100, 10);
        diskcache::markUsed(a);
        QVERIFY(used(a).secsTo(QDateTime::currentDateTime()) < 60);
        QCOMPARE(diskcache::trim({dir.path()}, 100), 100);
        QVERIFY(QFile::exists(a));
        QVERIFY(!QFile::exists(b));
    }

    void markingIsCoarse()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(u"a"_s);
        put(a, 1, 0);
        const QDateTime before = QDateTime::currentDateTime().addSecs(-3600);
        setUsed(a, before);
        // Used an hour ago: within the day, so no write.
        diskcache::markUsed(a);
        QCOMPARE(used(a).toSecsSinceEpoch(), before.toSecsSinceEpoch());
        diskcache::markUsed(a, std::chrono::minutes(30));
        QVERIFY(used(a) > before.addSecs(60));
    }
};

QTEST_GUILESS_MAIN(DiskCacheTest)
#include "diskcache_test.moc"

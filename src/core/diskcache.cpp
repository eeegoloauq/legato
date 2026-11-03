#include "diskcache.h"

#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QList>

#include <algorithm>
#include <filesystem>

namespace legato::diskcache {

namespace fs = std::filesystem;

void markUsed(const QString &path, std::chrono::seconds granularity)
{
    // By path, not through an open QFile: Windows sets a file's time only through a handle opened
    // for writing.
#ifdef Q_OS_WIN
    const fs::path file = path.toStdWString();
#else
    const fs::path file = QFile::encodeName(path).toStdString();
#endif
    std::error_code error;
    const fs::file_time_type now = fs::file_time_type::clock::now();
    const fs::file_time_type last = fs::last_write_time(file, error);
    if (error || now - last < granularity)
        return;
    fs::last_write_time(file, now, error);
}

qint64 trim(const QStringList &roots, qint64 limit)
{
    struct Entry {
        QString path;
        qint64 size;
        qint64 used; // ms since the epoch
    };
    QList<Entry> files;
    qint64 total = 0;
    for (const QString &root : roots) {
        QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info = it.nextFileInfo();
            files.append({info.filePath(), info.size(), info.lastModified().toMSecsSinceEpoch()});
            total += info.size();
        }
    }
    if (total <= limit)
        return 0;

    std::sort(files.begin(), files.end(), [](const Entry &a, const Entry &b) { return a.used < b.used; });
    qint64 removed = 0;
    for (const Entry &file : std::as_const(files)) {
        if (total - removed <= limit)
            break;
        // A file open elsewhere on Windows stays; it is counted again next time.
        if (QFile::remove(file.path))
            removed += file.size;
    }
    return removed;
}

} // namespace legato::diskcache

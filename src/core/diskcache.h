// A size limit for the disk caches (covers, crossfade levels, lyrics), which would otherwise grow
// with every album shown and song played. The least recently used files go first: a file's
// modification time is set when it is used, as lumeo's artwork cache does.
#pragma once

#include <QString>
#include <QStringList>

#include <chrono>

namespace legato::diskcache {

constexpr qint64 Limit = qint64(512) << 20;

// Marks a cached file as used now. Only when its time is older than `granularity`, so reading the
// same covers again and again does not write to the disk each time.
void markUsed(const QString &path, std::chrono::seconds granularity = std::chrono::hours(24));

// Removes the least recently used files under `roots` (and their folders) until the files take at
// most `limit` bytes together. Folders stay: a cache writes into its folder without creating it
// again. Blocking; run it off the GUI thread. Returns the bytes removed.
qint64 trim(const QStringList &roots, qint64 limit = Limit);

} // namespace legato::diskcache

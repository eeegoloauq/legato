#include "instantmix.h"

#include <QHash>
#include <QRandomGenerator>
#include <QSet>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

// Songs without an artist id count as an artist of their own name; without either, as one each.
QString artistKey(const Song &song)
{
    if (!song.artistId.isEmpty())
        return song.artistId;
    if (!song.artist.isEmpty())
        return u"name:"_s + song.artist.toCaseFolded();
    return u"song:"_s + song.id;
}

} // namespace

QList<Song> arrangeMix(const QList<Song> &similar, const std::optional<Song> &seed, QRandomGenerator &random)
{
    QSet<QString> seen;
    if (seed)
        seen.insert(seed->id);
    QList<Song> pool;
    pool.reserve(similar.size());
    QHash<QString, int> left;
    for (const Song &song : similar) {
        if (song.id.isEmpty() || seen.contains(song.id))
            continue;
        seen.insert(song.id);
        pool.append(song);
        ++left[artistKey(song)];
    }
    std::shuffle(pool.begin(), pool.end(), random);

    QList<Song> out;
    out.reserve(pool.size() + 1);
    QString previous;
    if (seed) {
        out.append(*seed);
        previous = artistKey(*seed);
    }
    while (!pool.isEmpty()) {
        // The artist with the most songs left goes now when waiting would leave two of theirs side
        // by side: that many songs need every other place from here on.
        QString most;
        int mostLeft = 0;
        for (auto it = left.cbegin(); it != left.cend(); ++it) {
            if (it.key() != previous && it.value() > mostLeft) {
                most = it.key();
                mostLeft = it.value();
            }
        }
        const int places = int(pool.size());
        const auto pick = std::find_if(pool.begin(), pool.end(), [&](const Song &song) {
            const QString key = artistKey(song);
            return 2 * mostLeft - 1 >= places ? key == most : key != previous;
        });
        const auto at = pick != pool.end() ? pick : pool.begin();
        previous = artistKey(*at);
        if (--left[previous] == 0)
            left.remove(previous);
        out.append(*at);
        pool.erase(at);
    }
    return out;
}

} // namespace legato

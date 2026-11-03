// Get info (docs/design.md, "Get info"): what a song's or an album's dialog lists, as label and
// value pairs. Rows without a value are left out.
#pragma once

#include "subsonic.h"

namespace legato {

struct InfoRow {
    QString label;
    QString value;

    bool operator==(const InfoRow &) const = default;
};

// "41 minutes", "1 hour, 5 minutes".
QString durationText(qint64 seconds);

QList<InfoRow> songInfo(const Song &song);
// The album and its songs, which give the totals and the format.
QList<InfoRow> albumInfo(const Album &album, const QList<Song> &songs);

} // namespace legato

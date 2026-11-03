// Instant Mix (docs/design.md, "Instant Mix"): the order of the songs the server suggests.
#pragma once

#include "subsonic.h"

#include <QList>

#include <optional>

class QRandomGenerator;

namespace legato {

// The queue of a mix: `seed` first when there is one, then `similar` without repeats or the seed,
// shuffled so one artist plays twice in a row only when there is nothing else left to put between.
QList<Song> arrangeMix(const QList<Song> &similar, const std::optional<Song> &seed, QRandomGenerator &random);

} // namespace legato

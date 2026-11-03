// Colors taken from a cover: the drifting blobs of the full-screen player and the accent of the
// playback controls (docs/architecture.md, "Artwork and colors").
#pragma once

#include <QColor>
#include <QList>

class QImage;

namespace legato {

// Three colors, most prominent first. Grey covers give a neutral palette.
QList<QColor> extractPalette(const QImage &image);

// A version of `color` with enough contrast against the light or dark background.
QColor accentFrom(const QColor &color, bool dark);

} // namespace legato

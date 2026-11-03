// Colors taken from a cover: the drifting blobs of the full-screen player and the accent of the
// playback controls (docs/architecture.md, "Artwork and colors").
#pragma once

#include <QColor>
#include <QList>

class QImage;

namespace legato {

// Three colors, most prominent first. Grey covers give a neutral palette.
QList<QColor> extractPalette(const QImage &image);

// A version of `color` that reads as text on the light or dark background: its hue, at least some
// saturation, and the lightness that gives WCAG's 4.5:1 contrast. Lightness alone is not enough: a
// yellow and a blue of the same HSL lightness are far apart in brightness.
QColor accentFrom(const QColor &color, bool dark);

// WCAG's relative luminance (0 black, 1 white) and the contrast ratio of two colors (1 to 21).
double luminance(const QColor &color);
double contrast(const QColor &a, const QColor &b);

} // namespace legato

#include "palette.h"

#include <QImage>

#include <algorithm>
#include <array>

namespace legato {

namespace {

constexpr int Buckets = 12; // hue sectors of 30 degrees

struct Bucket {
    double weight = 0;
    double r = 0, g = 0, b = 0;

    void add(const QColor &c, double w)
    {
        weight += w;
        r += c.redF() * w;
        g += c.greenF() * w;
        b += c.blueF() * w;
    }
    QColor color() const { return QColor::fromRgbF(float(r / weight), float(g / weight), float(b / weight)); }
};

} // namespace

QList<QColor> extractPalette(const QImage &image)
{
    if (image.isNull())
        return {};
    const QImage small = image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB32);
    std::array<Bucket, Buckets> hues;
    Bucket neutral;
    for (int y = 0; y < small.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(small.constScanLine(y));
        for (int x = 0; x < small.width(); ++x) {
            const QColor c = QColor::fromRgb(line[x]);
            const double s = c.hsvSaturationF();
            const double v = c.valueF();
            // Saturated, bright pixels define the look; dark and grey ones count as neutral.
            if (s < 0.18 || v < 0.15) {
                neutral.add(c, 1.0);
                continue;
            }
            hues[size_t(c.hsvHue() * Buckets / 360) % Buckets].add(c, s * v);
        }
    }
    std::sort(hues.begin(), hues.end(), [](const Bucket &a, const Bucket &b) { return a.weight > b.weight; });

    QList<QColor> colors;
    for (const Bucket &bucket : hues) {
        // A hue needs a visible share of the cover to count.
        if (bucket.weight < 6.0 || colors.size() == 3)
            break;
        colors.append(bucket.color());
    }
    if (colors.isEmpty())
        colors.append(neutral.weight > 0 ? neutral.color() : QColor(0x5a, 0x52, 0xd6));
    // Fill up with lighter and darker shades of the main color.
    const QColor base = colors.first();
    while (colors.size() < 3)
        colors.append(colors.size() == 1 ? base.lighter(135) : base.darker(140));
    return colors;
}

QColor accentFrom(const QColor &color, bool dark)
{
    float h, s, l, a;
    color.getHslF(&h, &s, &l, &a);
    if (h < 0) // achromatic
        return dark ? QColor(0x95, 0x90, 0xff) : QColor(0x5a, 0x52, 0xd6);
    s = std::max(s, 0.45f);
    l = dark ? std::clamp(l, 0.62f, 0.75f) : std::clamp(l, 0.36f, 0.50f);
    return QColor::fromHslF(h, s, l);
}

} // namespace legato

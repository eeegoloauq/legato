#include "palette.h"

#include <QImage>

#include <algorithm>
#include <array>
#include <cmath>

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

double luminance(const QColor &color)
{
    const auto linear = [](float c) { return c <= 0.04045f ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    const QColor rgb = color.toRgb();
    return 0.2126 * linear(rgb.redF()) + 0.7152 * linear(rgb.greenF()) + 0.0722 * linear(rgb.blueF());
}

double contrast(const QColor &a, const QColor &b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor accentFrom(const QColor &color, bool dark)
{
    // Theme.bg: the page under the playing row's title and the controls.
    const QColor background = dark ? QColor(0x10, 0x10, 0x10) : QColor(0xfa, 0xfa, 0xfa);
    float h, s, l, a;
    color.getHslF(&h, &s, &l, &a);
    if (h < 0) // achromatic
        return dark ? QColor(0x95, 0x90, 0xff) : QColor(0x5a, 0x52, 0xd6);
    s = std::max(s, 0.45f);
    l = dark ? std::clamp(l, 0.62f, 0.75f) : std::clamp(l, 0.36f, 0.50f);
    // Lighter on dark, darker on light, until it reads.
    QColor accent = QColor::fromHslF(h, s, l);
    while (contrast(accent, background) < 4.5 && l > 0.05f && l < 0.95f) {
        l += dark ? 0.02f : -0.02f;
        accent = QColor::fromHslF(h, s, std::clamp(l, 0.0f, 1.0f));
    }
    return accent;
}

} // namespace legato

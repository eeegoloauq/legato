#include "automix.h"

#include <QTest>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace legato;

namespace {

constexpr double W = automix::WindowSeconds;

// `seconds` of windows at `db`.
QList<float> level(double seconds, float db)
{
    return QList<float>(qsizetype(std::lround(seconds / W)), db);
}

// `seconds` of windows going from `from` to `to` dB in a straight line.
QList<float> ramp(double seconds, float from, float to)
{
    const qsizetype n = std::lround(seconds / W);
    QList<float> out(n);
    for (qsizetype i = 0; i < n; ++i)
        out[i] = from + (to - from) * float(i) / float(n);
    return out;
}

QList<float> operator+(QList<float> a, const QList<float> &b)
{
    a.append(b);
    return a;
}

// A part with these levels and the same beat all through.
automix::Part part(const QList<float> &level, float beat = 0)
{
    return {level, QList<float>(level.size(), beat)};
}

constexpr int Rate = 22050;

// Repeatable noise in -1..1.
double noise(quint32 &seed)
{
    seed = seed * 1664525u + 1013904223u;
    return double(seed >> 8) / double(1u << 23) - 1.0;
}

// `seconds` of a drum loop at `bpm`: a kick (a falling 60 Hz tone) on every beat and a short hiss,
// as of a hi-hat, between them, over a quiet held chord.
QList<qint16> drums(double seconds, double bpm)
{
    QList<qint16> out(qsizetype(seconds * Rate));
    const double period = 60.0 / bpm;
    quint32 seed = 1;
    for (qsizetype i = 0; i < out.size(); ++i) {
        const double t = double(i) / Rate;
        const double inBeat = std::fmod(t, period);
        const double offBeat = std::fmod(t + period / 2, period);
        const double kick = std::exp(-inBeat * 25) * std::sin(2 * std::numbers::pi * 60 * inBeat);
        const double hat = std::exp(-offBeat * 80) * noise(seed) * 0.3;
        const double chord = 0.05 * (std::sin(2 * std::numbers::pi * 220 * t) + std::sin(2 * std::numbers::pi * 277 * t));
        out[i] = qint16(std::clamp((kick * 0.6 + hat + chord) * 32767, -32768.0, 32767.0));
    }
    return out;
}

float median(QList<float> values)
{
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

} // namespace

class AutomixTest : public QObject
{
    Q_OBJECT

private slots:
    void envelope()
    {
        const int rate = 22050;
        QList<qint16> samples(rate, 0);
        // Half a second of a square wave at half scale, then half a second of digital silence.
        for (int i = 0; i < rate / 2; ++i)
            samples[i] = i % 2 ? 16384 : -16384;
        const QList<float> levels = automix::envelope(samples.constData(), samples.size(), rate);
        QCOMPARE(levels.size(), 20);
        QVERIFY(qAbs(levels.first() + 6.02f) < 0.05f);
        QCOMPARE(levels.last(), automix::Silence);
    }

    void hardEnding()
    {
        // Loud to the last sound, then a second of silence; the next song starts loud after half a
        // second of silence. Back to back, both silences skipped.
        const auto plan = automix::plan(part(level(29, -10) + level(1, -100)), part(level(0.5, -100) + level(19.5, -10)), 10);
        QVERIFY(plan);
        QVERIFY(qAbs(plan->stop - 1.0) < 0.06);
        QVERIFY(plan->overlap() < 0.3);
        QVERIFY(qAbs(plan->inFrom - 0.5) < 0.06);
        QCOMPARE(plan->bassSwap, -1.0);
    }

    void longFadeOut()
    {
        // A fade of 18 s to below hearing, then 2 s of silence: the next song comes in once the old one
        // is 12 dB down, and overlaps until the old one is gone.
        const QList<float> tail = level(10, -10) + ramp(18, -10, -70) + level(2, -100);
        const auto plan = automix::plan(part(tail), part(level(20, -10)), 10);
        QVERIFY(plan);
        QVERIFY(plan->overlap() > 8 && plan->overlap() <= 10);
        // The old song is 45 dB down 13.5 s into its fade, 6.5 s before its end.
        QVERIFY(qAbs(plan->stop - 6.5) < 0.2);
        QCOMPARE(plan->outFadeFrom, 0.0);
        // A loud start fades in quickly, not over the whole overlap; it brings its bass at once.
        QCOMPARE(plan->inRamp, 1.5);
        QCOMPARE(plan->bassSwap, 0.0);
        // The limit cuts the old song's fade short; the new song still comes in at the same point.
        const auto shorter = automix::plan(part(tail), part(level(20, -10)), 4);
        QVERIFY(shorter);
        QVERIFY(qAbs(shorter->overlap() - 4) < 1e-9);
        QVERIFY(qAbs(shorter->start - plan->start) < 1e-9);
    }

    void quietIntro()
    {
        // Four quiet seconds before the song gets loud play under the old song's hard ending, at their
        // own volume, so the new song is at full level as the old one stops.
        const auto plan = automix::plan(part(level(29, -10) + level(1, -100)), part(level(4, -30) + level(16, -10)), 10);
        QVERIFY(plan);
        QVERIFY(plan->overlap() > 3.4 && plan->overlap() < 4.1);
        QCOMPARE(plan->inRamp, 0.0);
        QCOMPARE(plan->outFadeFrom, plan->overlap());
        QVERIFY(qAbs(plan->stop - 1.0) < 0.06);
        // Its bass comes as the old song stops.
        QCOMPARE(plan->bassSwap, plan->outFadeFrom);
        // Longer intros are cut to six seconds under the old song.
        const auto longer = automix::plan(part(level(30, -10)), part(level(15, -30) + level(5, -10)), 10);
        QVERIFY(longer);
        QVERIFY(qAbs(longer->overlap() - 6) < 1e-9);
    }

    void beats()
    {
        // A drum loop has a clear beat at any tempo in range.
        for (const double bpm : {75.0, 120.0, 170.0}) {
            const QList<qint16> loop = drums(20, bpm);
            const QList<float> beat = automix::beats(loop.constData(), loop.size(), Rate);
            QCOMPARE(beat.size(), 400);
            QVERIFY2(median(beat) > 0.5, qPrintable(QString::number(median(beat))));
        }
        // Noise and a held chord have none.
        QList<qint16> hiss(20 * Rate);
        quint32 seed = 7;
        for (qint16 &s : hiss)
            s = qint16(noise(seed) * 8000);
        QVERIFY(median(automix::beats(hiss.constData(), hiss.size(), Rate)) < 0.2);
        QList<qint16> chord(20 * Rate);
        for (qsizetype i = 0; i < chord.size(); ++i)
            chord[i] = qint16(8000 * std::sin(2 * std::numbers::pi * 220 * double(i) / Rate));
        QVERIFY(median(automix::beats(chord.constData(), chord.size(), Rate)) < 0.2);
        const QList<qint16> silent(20 * Rate, 0);
        QCOMPARE(median(automix::beats(silent.constData(), silent.size(), Rate)), 0.0f);
        // Both measures cover the same windows.
        const QList<qint16> loop = drums(20, 120);
        const automix::Part measured = automix::measure(loop.constData(), loop.size(), Rate);
        QCOMPARE(measured.level.size(), measured.beat.size());
    }

    void beatsClash()
    {
        // The long fade-out, both songs with a beat: the old one goes within a second of the new one
        // coming in, at the same point; too short to swap the bass.
        const QList<float> tail = level(10, -10) + ramp(18, -10, -70) + level(2, -100);
        const auto blend = automix::plan(part(tail), part(level(20, -10)), 10);
        automix::Judged judged;
        const auto plan = automix::plan(part(tail, 0.6f), part(level(20, -10), 0.6f), 10, &judged);
        QVERIFY(plan);
        QVERIFY(judged.beatsClash);
        QVERIFY(plan->overlap() <= 1.0 + 1e-9);
        QVERIFY(qAbs(plan->start - blend->start) < 1e-9);
        QCOMPARE(plan->bassSwap, -1.0);
        // One beat over a song without: the long blend stays.
        const auto one = automix::plan(part(tail, 0.6f), part(level(20, -10), 0.1f), 10, &judged);
        QVERIFY(!judged.beatsClash);
        QVERIFY(qAbs(one->overlap() - blend->overlap()) < 1e-9);
        // A quiet intro with a beat does not start under a song with one: back to back instead.
        const QList<float> hard = level(29, -10) + level(1, -100);
        const auto intro = automix::plan(part(hard, 0.6f), part(level(4, -30) + level(16, -10), 0.6f), 10, &judged);
        QVERIFY(judged.beatsClash);
        QVERIFY(intro->overlap() < 0.3);
        QVERIFY(qAbs(intro->stop - 1.0) < 0.06);
    }

    void silence()
    {
        QVERIFY(!automix::plan(part(level(30, -100)), part(level(20, -10)), 10));
        QVERIFY(!automix::plan(part(level(30, -10)), part(level(20, -100)), 10));
        QVERIFY(!automix::plan({}, part(level(20, -10)), 10));
    }
};

QTEST_GUILESS_MAIN(AutomixTest)
#include "automix_test.moc"

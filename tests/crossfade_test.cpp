#include "crossfade.h"

#include <QTest>

#include <cmath>
#include <numbers>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

Song track(const QString &albumId, int disc, int number)
{
    Song s;
    s.albumId = albumId;
    s.disc = disc;
    s.track = number;
    return s;
}

} // namespace

class CrossfadeTest : public QObject
{
    Q_OBJECT

private slots:
    void albumContinues()
    {
        QVERIFY(crossfade::albumContinues(track(u"a"_s, 1, 3), track(u"a"_s, 1, 4)));
        // The next disc starts over at track 1; a missing disc number is disc 1.
        QVERIFY(crossfade::albumContinues(track(u"a"_s, 1, 12), track(u"a"_s, 2, 1)));
        QVERIFY(crossfade::albumContinues(track(u"a"_s, 0, 2), track(u"a"_s, 1, 3)));
        // Shuffled songs of one album, the same song again, and other albums fade.
        QVERIFY(!crossfade::albumContinues(track(u"a"_s, 1, 3), track(u"a"_s, 1, 7)));
        QVERIFY(!crossfade::albumContinues(track(u"a"_s, 1, 3), track(u"a"_s, 1, 3)));
        QVERIFY(!crossfade::albumContinues(track(u"a"_s, 1, 3), track(u"b"_s, 1, 4)));
        QVERIFY(!crossfade::albumContinues(track(u""_s, 1, 3), track(u""_s, 1, 4)));
        // Without track numbers, the album decides.
        QVERIFY(crossfade::albumContinues(track(u"a"_s, 0, 0), track(u"a"_s, 0, 0)));
    }

    void length()
    {
        QCOMPARE(crossfade::length(6, 240, 200), 6.0);
        // A third of the shorter song.
        QCOMPARE(crossfade::length(6, 240, 12), 4.0);
        QCOMPARE(crossfade::length(12, 15, 300), 5.0);
        // Unknown durations do not limit it.
        QCOMPARE(crossfade::length(6, 0, 0), 6.0);
        QCOMPARE(crossfade::length(-1, 240, 200), 0.0);
    }

    void planGains()
    {
        // A fixed plan is the plain equal-power fade.
        const crossfade::Plan fixed = crossfade::fixedPlan(6);
        QCOMPARE(fixed.overlap(), 6.0);
        const auto [fixedOut, fixedIn] = crossfade::gains(fixed, 1.5);
        const auto [plainOut, plainIn] = crossfade::gains(0.25);
        QCOMPARE(fixedOut, plainOut);
        QCOMPARE(fixedIn, plainIn);
        // A quiet intro under the old song: the new one at full volume from the start, the old one
        // at full volume until its fade begins, then gone by the end of the overlap.
        crossfade::Plan plan;
        plan.start = 8;
        plan.stop = 2;
        plan.outFadeFrom = 2;
        QCOMPARE(crossfade::gains(plan, 1), std::make_pair(1.0, 1.0));
        QVERIFY(qAbs(crossfade::gains(plan, 6).first) < 1e-9);
        // Halfway through its own fade.
        QVERIFY(qAbs(crossfade::gains(plan, 4).first - std::cos(std::numbers::pi / 4)) < 1e-9);
        // An old song that stops at the end of the overlap without fading.
        plan.outFadeFrom = 6;
        QCOMPARE(crossfade::gains(plan, 5.9).first, 1.0);
        QCOMPARE(crossfade::gains(plan, 6.1).first, 0.0);
    }

    void gains()
    {
        const auto [startOut, startIn] = crossfade::gains(0);
        QCOMPARE(startOut, 1.0);
        QCOMPARE(startIn, 0.0);
        const auto [endOut, endIn] = crossfade::gains(1);
        QVERIFY(qAbs(endOut) < 1e-9);
        QCOMPARE(endIn, 1.0);
        // Equal power all the way through.
        for (double p = 0; p <= 1.0; p += 0.125) {
            const auto [out, in] = crossfade::gains(p);
            QVERIFY(qAbs(out * out + in * in - 1) < 1e-9);
        }
        QCOMPARE(crossfade::gains(2).second, 1.0);
    }

    void bassCuts()
    {
        // Plain fades keep both songs whole.
        QCOMPARE(crossfade::bassCuts(crossfade::fixedPlan(6), 3), std::make_pair(0.0, 0.0));
        // Before the swap the new song plays without its bass; after it, the old one.
        crossfade::Plan plan;
        plan.start = 8;
        plan.bassSwap = 3;
        QCOMPARE(crossfade::bassCuts(plan, 0), std::make_pair(0.0, 1.0));
        const auto [out, in] = crossfade::bassCuts(plan, 3 + crossfade::BassSwapSeconds / 2);
        QVERIFY(qAbs(out - 0.5) < 1e-9 && qAbs(in - 0.5) < 1e-9);
        QCOMPARE(crossfade::bassCuts(plan, 6), std::make_pair(1.0, 0.0));
    }

    void mpvVolume()
    {
        QCOMPARE(crossfade::mpvVolume(80, 1), 80.0);
        QCOMPARE(crossfade::mpvVolume(80, 0), 0.0);
        // Half the amplitude is (1/2)^(1/3) of the volume, as mpv cubes it.
        const double v = crossfade::mpvVolume(100, 0.5);
        QVERIFY(qAbs(v / 100 * v / 100 * v / 100 - 0.5) < 1e-9);
    }
};

QTEST_GUILESS_MAIN(CrossfadeTest)
#include "crossfade_test.moc"

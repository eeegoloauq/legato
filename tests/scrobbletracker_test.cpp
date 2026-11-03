#include "scrobbletracker.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace legato;
using Step = ScrobbleTracker::Step;

namespace {

// Plays from `from` to `to` in quarter-second steps, as mpv reports the position; returns the
// steps that were not None.
QList<Step> play(ScrobbleTracker &tracker, double from, double to, double duration)
{
    QList<Step> out;
    for (double t = from; t <= to + 1e-9; t += 0.25) {
        const Step step = tracker.advance(t, duration, 1000);
        if (step != Step::None)
            out.append(step);
    }
    return out;
}

} // namespace

class ScrobbleTrackerTest : public QObject
{
    Q_OBJECT

private slots:
    void countsAtHalfTheSong()
    {
        ScrobbleTracker tracker;
        tracker.start(u"a"_s, 1);
        QVERIFY(play(tracker, 0, 99.5, 200).isEmpty());
        QCOMPARE(play(tracker, 99.75, 100, 200), QList<Step>{Step::Counted});
        // Once per play.
        QVERIFY(play(tracker, 100.25, 199, 200).isEmpty());
    }

    void countsAtFourMinutesInALongSong()
    {
        ScrobbleTracker tracker;
        tracker.start(u"a"_s, 1);
        QVERIFY(play(tracker, 0, 239.75, 1200).isEmpty());
        QCOMPARE(play(tracker, 240, 240, 1200), QList<Step>{Step::Counted});
    }

    void shortSongsNeverCount()
    {
        ScrobbleTracker tracker;
        tracker.start(u"a"_s, 1);
        QVERIFY(play(tracker, 0, 30, 30).isEmpty());
    }

    void seekingForwardAddsNothing()
    {
        ScrobbleTracker tracker;
        tracker.start(u"a"_s, 1);
        play(tracker, 0, 10, 200);
        // Straight to near the end: the jump is not listening.
        QVERIFY(play(tracker, 190, 200, 200).isEmpty());
        QCOMPARE(tracker.listened(), 20.0);
    }

    void seekingBackKeepsWhatWasHeard()
    {
        ScrobbleTracker tracker;
        tracker.start(u"a"_s, 1);
        play(tracker, 0, 60, 200);
        QVERIFY(play(tracker, 20, 59.75, 200).isEmpty());
        QCOMPARE(play(tracker, 60, 60, 200), QList<Step>{Step::Counted});
    }

    void repeatOneIsANewPlay()
    {
        ScrobbleTracker tracker;
        tracker.start(u"a"_s, 1);
        QCOMPARE(play(tracker, 0, 120, 120), QList<Step>{Step::Counted});
        QCOMPARE(tracker.advance(0.1, 120, 5000), Step::Restarted);
        QCOMPARE(tracker.startedAt(), 5000);
        QCOMPARE(play(tracker, 0.25, 60.25, 120), QList<Step>{Step::Counted});
    }

    // Restarting from the middle (Previous after 3 s) is not the song repeating.
    void goingBackToTheStartMidSongIsTheSamePlay()
    {
        ScrobbleTracker tracker;
        tracker.start(u"a"_s, 1);
        play(tracker, 0, 50, 120);
        QCOMPARE(tracker.advance(0, 120, 5000), Step::None);
        QCOMPARE(tracker.startedAt(), 1);
    }

    void nothingIsTrackedWithoutASong()
    {
        ScrobbleTracker tracker;
        tracker.start({}, 1);
        QVERIFY(play(tracker, 0, 200, 200).isEmpty());
    }
};

QTEST_GUILESS_MAIN(ScrobbleTrackerTest)
#include "scrobbletracker_test.moc"

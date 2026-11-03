// When a play counts as a listen (docs/architecture.md, "Scrobbling").
//
// Last.fm's rule, which ListenBrainz and the servers follow: a song longer than 30 seconds counts
// once it has played for half its length or four minutes, whichever comes first. Playing time is
// summed from small position steps, so a seek forward adds nothing, and a song that starts over
// from its end (repeat one) is a new play.
#pragma once

#include <QString>

namespace legato {

class ScrobbleTracker
{
public:
    enum class Step {
        None,
        // The song came round again on repeat: report it as playing anew.
        Restarted,
        // The play just reached the listening threshold: submit it.
        Counted,
    };

    // A song started playing at `startedAt` (milliseconds since the epoch). An empty id stops tracking.
    void start(const QString &songId, qint64 startedAt);
    // The playing position is now `seconds` of `duration`. Counted is returned once per play.
    Step advance(double seconds, double duration, qint64 now);

    const QString &songId() const { return m_songId; }
    qint64 startedAt() const { return m_startedAt; }
    double listened() const { return m_listened; }

    // Steps longer than this are seeks, not playing.
    static constexpr double MaxStep = 2.0;
    static constexpr double MinDuration = 30.0;
    static constexpr double MaxThreshold = 240.0;

private:
    QString m_songId;
    qint64 m_startedAt = 0;
    double m_last = 0;
    double m_listened = 0;
    bool m_counted = false;
};

} // namespace legato

#include "scrobbletracker.h"

#include <algorithm>

namespace legato {

void ScrobbleTracker::start(const QString &songId, qint64 startedAt)
{
    m_songId = songId;
    m_startedAt = startedAt;
    m_last = 0;
    m_listened = 0;
    m_counted = false;
}

ScrobbleTracker::Step ScrobbleTracker::advance(double seconds, double duration, qint64 now)
{
    if (m_songId.isEmpty())
        return Step::None;
    const double step = seconds - m_last;
    // mpv loops a song on repeat by going back to its start: the end was reached, so a new play.
    if (step < 0 && duration > 0 && m_last >= duration - MaxStep && seconds < MaxStep) {
        start(m_songId, now);
        m_last = seconds;
        return Step::Restarted;
    }
    m_last = seconds;
    if (step > 0 && step <= MaxStep)
        m_listened += step;
    if (m_counted || duration <= MinDuration || m_listened < std::min(duration / 2, MaxThreshold))
        return Step::None;
    m_counted = true;
    return Step::Counted;
}

} // namespace legato

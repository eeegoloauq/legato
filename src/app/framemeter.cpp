#include "framemeter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>

#include <algorithm>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

constexpr qint64 Second = 1'000'000'000;
// Frames older than this are dropped; a run is shorter.
constexpr qint64 Keep = 60 * Second;
// A gap this long is a stall the eye sees at any refresh rate.
constexpr double StallMs = 50;

double percentile(std::vector<double> sorted, double p)
{
    if (sorted.empty())
        return 0;
    const size_t i = std::min(sorted.size() - 1, size_t(std::ceil(p * double(sorted.size()))) - 1);
    return sorted[i];
}

} // namespace

FrameMeter::FrameMeter(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
    m_ticker.setInterval(250);
    connect(&m_ticker, &QTimer::timeout, this, &FrameMeter::tick);
    m_ticker.start();
}

void FrameMeter::setWindow(QQuickWindow *window)
{
    if (window == m_window)
        return;
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
    m_window = window;
    if (m_window) {
        // Runs on the render thread: only the timestamp list is touched there.
        connect(m_window, &QQuickWindow::frameSwapped, this, [this] {
            const qint64 now = m_clock.nsecsElapsed();
            QMutexLocker lock(&m_mutex);
            m_frames.push_back(now);
            if (m_frames.front() < now - Keep && !m_running)
                m_frames.erase(m_frames.begin(), std::lower_bound(m_frames.begin(), m_frames.end(), now - Keep));
        }, Qt::DirectConnection);
        connect(m_window, &QWindow::activeChanged, this, &FrameMeter::checkWindow);
        connect(m_window, &QWindow::visibilityChanged, this, &FrameMeter::checkWindow);
        connect(m_window, &QWindow::screenChanged, this, &FrameMeter::updated);
    }
    emit windowChanged();
}

double FrameMeter::refreshRate() const
{
    return m_window && m_window->screen() ? m_window->screen()->refreshRate() : 0;
}

std::vector<qint64> FrameMeter::framesSince(qint64 from) const
{
    QMutexLocker lock(&m_mutex);
    return {std::lower_bound(m_frames.begin(), m_frames.end(), from), m_frames.end()};
}

void FrameMeter::tick()
{
    m_fps = double(framesSince(m_clock.nsecsElapsed() - Second).size());
    emit updated();
}

void FrameMeter::checkWindow()
{
    if (m_running && m_window && (!m_window->isActive() || !m_window->isExposed()))
        m_interrupted = true;
}

void FrameMeter::start(const QString &name, const QString &effects)
{
    m_runName = name;
    m_runEffects = effects;
    m_runStart = m_clock.nsecsElapsed();
    m_interrupted = false;
    m_running = true;
    checkWindow();
    emit runningChanged();
}

void FrameMeter::stop()
{
    if (!m_running)
        return;
    m_running = false;
    emit runningChanged();
    checkWindow();

    const qint64 end = m_clock.nsecsElapsed();
    std::vector<qint64> frames = framesSince(m_runStart);
    frames.erase(std::upper_bound(frames.begin(), frames.end(), end), frames.end());
    const double seconds = double(end - m_runStart) / double(Second);

    std::vector<double> intervals;
    for (size_t i = 1; i < frames.size(); ++i)
        intervals.push_back(double(frames[i] - frames[i - 1]) / 1e6);
    std::sort(intervals.begin(), intervals.end());

    const double hz = refreshRate();
    const double budget = hz > 0 ? 1000.0 / hz : 1000.0 / 60;
    const auto over = [&](double ms) { return std::count_if(intervals.begin(), intervals.end(), [ms](double v) { return v > ms; }); };
    const long late = long(over(budget * 1.5));
    const long stalls = long(over(StallMs));
    const double fps = intervals.empty() ? 0 : double(intervals.size()) / ((frames.back() - frames.front()) / 1e9);

    QString problem;
    if (m_interrupted)
        problem = u"the window was inactive or hidden during the run"_s;
    else if (frames.size() < size_t(std::max(30.0, seconds * 20)))
        problem = u"too few frames for a result"_s;

    const auto ms = [](double v) { return QString::number(v, 'f', 1); };
    m_report = u"%1 · %2 s · %3 frames · %4 fps\n"
               "p50 %5 · p95 %6 · p99 %7 · max %8 ms\n"
               "late (over 1.5 × %9 ms): %10 · stalls (over 50 ms): %11\n"
               "display %12 Hz · %13"_s
                   .arg(m_runName, ms(seconds))
                   .arg(frames.size())
                   .arg(ms(fps), ms(percentile(intervals, 0.5)), ms(percentile(intervals, 0.95)),
                        ms(percentile(intervals, 0.99)), ms(intervals.empty() ? 0 : intervals.back()), ms(budget))
                   .arg(late)
                   .arg(stalls)
                   .arg(qRound(hz))
                   .arg(m_runEffects.isEmpty() ? u"no effects"_s : m_runEffects);
    if (!problem.isEmpty())
        m_report = u"NOT VALID: %1\n"_s.arg(problem) + m_report;
    emit reportChanged();

    // One line for pasting into a report.
    const QJsonObject json{{"run"_L1, m_runName},         {"valid"_L1, problem.isEmpty()},
                           {"problem"_L1, problem},        {"seconds"_L1, seconds},
                           {"frames"_L1, qint64(frames.size())}, {"fps"_L1, fps},
                           {"p50_ms"_L1, percentile(intervals, 0.5)}, {"p95_ms"_L1, percentile(intervals, 0.95)},
                           {"p99_ms"_L1, percentile(intervals, 0.99)}, {"max_ms"_L1, intervals.empty() ? 0 : intervals.back()},
                           {"late"_L1, qint64(late)},       {"stalls"_L1, qint64(stalls)},
                           {"display_hz"_L1, hz},          {"effects"_L1, m_runEffects}};
    qInfo("legato frame meter: %s", QJsonDocument(json).toJson(QJsonDocument::Compact).constData());
}

} // namespace legato

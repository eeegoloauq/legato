// M0 frame meter: counts the frames the window hands to the display (QQuickWindow::frameSwapped)
// and reports fixed-budget statistics for timed runs.
//
// A run is marked invalid when the window was inactive, hidden or drew too few frames, because a
// hidden window draws nothing and its numbers mean nothing.
#pragma once

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
#include <QtQmlIntegration>

#include <vector>

namespace legato {

class FrameMeter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    // Frames presented in the last second. Zero while nothing on screen changes.
    Q_PROPERTY(double fps READ fps NOTIFY updated)
    Q_PROPERTY(double refreshRate READ refreshRate NOTIFY updated)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString report READ report NOTIFY reportChanged)

public:
    explicit FrameMeter(QObject *parent = nullptr);

    QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    double fps() const { return m_fps; }
    double refreshRate() const;
    bool running() const { return m_running; }
    QString report() const { return m_report; }

    // `effects` names the effects that are on, for the report.
    Q_INVOKABLE void start(const QString &name, const QString &effects);
    Q_INVOKABLE void stop();

signals:
    void windowChanged();
    void updated();
    void runningChanged();
    void reportChanged();

private:
    void tick();
    void checkWindow();
    std::vector<qint64> framesSince(qint64 from) const;

    QPointer<QQuickWindow> m_window;
    QElapsedTimer m_clock;
    QTimer m_ticker;
    mutable QMutex m_mutex;
    std::vector<qint64> m_frames; // ns since m_clock started; written on the render thread
    double m_fps = 0;
    bool m_running = false;
    bool m_interrupted = false;
    qint64 m_runStart = 0;
    QString m_runName;
    QString m_runEffects;
    QString m_report;
};

} // namespace legato

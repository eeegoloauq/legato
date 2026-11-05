// Level and beat of song parts for Automatic crossfade (docs/architecture.md, "Gapless and
// crossfade").
//
// A third mpv instance, never heard, decodes the end of a song or its start to a raw PCM file as fast
// as it can (ao=pcm has no clock), one part at a time; automix.h measures the samples on a worker
// thread. Measures are kept on disk, two bytes per 50 ms, so a song is fetched for this once.
#pragma once

#include "automix.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>
#include <initializer_list>

struct mpv_handle;

namespace legato {

class MixAnalyzer : public QObject
{
    Q_OBJECT

public:
    enum class Part { Head, Tail };
    // Empty levels: the part could not be decoded.
    using Done = std::function<void(const automix::Part &part)>;

    explicit MixAnalyzer(QObject *parent = nullptr);
    ~MixAnalyzer() override;

    // Where envelopes are kept: one directory per server, as song ids are only unique per server.
    // Empty keeps nothing.
    void setDirectory(const QString &dir);
    // Calls `done` once, later, on this thread.
    void analyze(const QString &songId, const QByteArray &url, Part part, Done done);
    // Parts asked for and not started are dropped without a call; the one decoding finishes.
    void dropWaiting();

private:
    struct Job {
        QString songId;
        QByteArray url;
        Part part = Part::Head;
        Done done;
    };

    bool ensureMpv();
    // Async, in the order given.
    void command(std::initializer_list<const char *> args);
    void startNext();
    void drainEvents();
    // The decode ended; `ok` when it reached the end of its part.
    void finish(bool ok);
    // The part is measured; the next one starts.
    void measured(const automix::Part &part);
    QString cachePath(const QString &songId, Part part) const;
    automix::Part readCache(const QString &songId, Part part) const;
    void writeCache(const QString &songId, Part part, const automix::Part &measure) const;

    mpv_handle *m_mpv = nullptr;
    QList<Job> m_waiting;
    // The job decoding; `m_done` is cleared once it was answered (a timeout answers before mpv stops).
    Job m_current;
    bool m_busy = false;
    // The decode is done and its samples are being measured.
    bool m_measuring = false;
    QTemporaryDir m_tmp;
    QTimer m_timeout;
    QString m_dir;
};

} // namespace legato

#include "mixanalyzer.h"

#include "diskcache.h"
#include "mpvoptions.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QSaveFile>
#include <QThreadPool>

#include <algorithm>
#include <cmath>
#include <utility>

#include <mpv/client.h>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

// Mono at half the CD rate: plenty for level and beat (the highest band is at 6 kHz), and a quarter of
// the samples to measure.
constexpr int Rate = 22050;
// The first byte of a cached part: its format. Older files held levels only.
constexpr char CacheFormat = char(0xf2);
// A part from a slow server that takes longer falls back to the fixed fade.
constexpr int TimeoutMs = 20000;

} // namespace

MixAnalyzer::MixAnalyzer(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(TimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        // Answered now; the next part starts once mpv has stopped this one.
        if (m_current.done) {
            qWarning("automix: %s took too long", qPrintable(m_current.songId));
            std::exchange(m_current.done, {})({});
        }
        const char *argv[] = {"stop", nullptr};
        mpv_command_async(m_mpv, 0, argv);
    });
}

MixAnalyzer::~MixAnalyzer()
{
    if (m_mpv) {
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        mpv_terminate_destroy(m_mpv);
    }
}

bool MixAnalyzer::ensureMpv()
{
    if (m_mpv)
        return true;
    if (!m_tmp.isValid())
        return false;
    m_mpv = mpv_create();
    if (!m_mpv)
        return false;
    const auto option = [this](const char *name, const char *value) { mpv_set_option_string(m_mpv, name, value); };
    mpvoptions::setCommon(m_mpv);
    // Reads the part it measures and nothing more: a cache would read the rest of the file ahead.
    option("cache", "no");
    // Written to a file as fast as it decodes, in a format read back without a header.
    option("ao", "pcm");
    option("ao-pcm-waveheader", "no");
    option("audio-format", "s16");
    option("audio-channels", "mono");
    option("audio-samplerate", qPrintable(QString::number(Rate)));
    // Closes the file at the end of each part, before mpv reports the end.
    option("gapless-audio", "no");
    option("network-timeout", "15");
    mpv_request_log_messages(m_mpv, "error");
    if (mpv_initialize(m_mpv) < 0) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }
    mpv_set_wakeup_callback(m_mpv, [](void *context) {
        auto *self = static_cast<MixAnalyzer *>(context);
        QMetaObject::invokeMethod(self, &MixAnalyzer::drainEvents, Qt::QueuedConnection);
    }, this);
    return true;
}

void MixAnalyzer::setDirectory(const QString &dir)
{
    m_dir = dir;
    if (!m_dir.isEmpty())
        QDir().mkpath(m_dir);
}

void MixAnalyzer::analyze(const QString &songId, const QByteArray &url, Part part, Done done)
{
    m_waiting.append({songId, url, part, std::move(done)});
    startNext();
}

void MixAnalyzer::dropWaiting()
{
    m_waiting.clear();
}

void MixAnalyzer::startNext()
{
    while (!m_busy && !m_waiting.isEmpty()) {
        Job job = m_waiting.takeFirst();
        automix::Part cached = readCache(job.songId, job.part);
        if (!cached.level.isEmpty() || !ensureMpv()) {
            // Answered later, as from mpv, so callers see one order either way.
            QMetaObject::invokeMethod(this, [done = std::move(job.done), cached = std::move(cached)] { done(cached); },
                                      Qt::QueuedConnection);
            continue;
        }
        m_current = std::move(job);
        m_busy = true;
        const QString file = m_tmp.filePath(u"part.pcm"_s);
        QFile::remove(file);
        mpv_set_property_string(m_mpv, "ao-pcm-file", qPrintable(file));
        const bool tail = m_current.part == Part::Tail;
        const QByteArray from = tail ? QByteArray::number(-automix::TailSeconds) : "none"_ba;
        const QByteArray to = tail ? "none"_ba : QByteArray::number(automix::HeadSeconds);
        mpv_set_property_string(m_mpv, "start", from.constData());
        mpv_set_property_string(m_mpv, "end", to.constData());
        const char *argv[] = {"loadfile", m_current.url.constData(), "replace", nullptr};
        mpv_command_async(m_mpv, 0, argv);
        m_timeout.start();
    }
}

void MixAnalyzer::drainEvents()
{
    while (m_mpv) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            return;
        if (event->event_id == MPV_EVENT_END_FILE && m_busy) {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            finish(end->reason == MPV_END_FILE_REASON_EOF);
        } else if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
            const auto *msg = static_cast<mpv_event_log_message *>(event->data);
            qWarning("automix: mpv: %s: %s", msg->prefix, QByteArray(msg->text).trimmed().constData());
        }
    }
}

void MixAnalyzer::finish(bool ok)
{
    if (m_measuring)
        return;
    m_timeout.stop();
    QByteArray bytes;
    if (ok) {
        QFile file(m_tmp.filePath(u"part.pcm"_s));
        if (file.open(QIODevice::ReadOnly))
            bytes = file.readAll();
    }
    if (bytes.isEmpty()) {
        measured({});
        return;
    }
    // Measuring a part takes about a tenth of a second: never on the interface's thread.
    m_measuring = true;
    QPointer<MixAnalyzer> self(this);
    QThreadPool::globalInstance()->start([self, bytes = std::move(bytes)] {
        automix::Part part = automix::measure(reinterpret_cast<const qint16 *>(bytes.constData()),
                                              bytes.size() / qsizetype(sizeof(qint16)), Rate);
        // Answered on the interface's thread, where `self` is read.
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, part = std::move(part)] {
            if (self)
                self->measured(part);
        }, Qt::QueuedConnection);
    });
}

void MixAnalyzer::measured(const automix::Part &part)
{
    m_busy = false;
    m_measuring = false;
    if (!part.level.isEmpty())
        writeCache(m_current.songId, m_current.part, part);
    if (m_current.done)
        std::exchange(m_current.done, {})(part);
    m_current = {};
    startNext();
}

QString MixAnalyzer::cachePath(const QString &songId, Part part) const
{
    if (m_dir.isEmpty())
        return {};
    const QByteArray hash = QCryptographicHash::hash(songId.toUtf8(), QCryptographicHash::Md5).toHex();
    return m_dir + u'/' + QString::fromLatin1(hash) + (part == Part::Tail ? u".tail"_s : u".head"_s);
}

// The format byte, then one byte per window for the level in whole dB below full scale (0..100), then
// one for the beat (0..255 for 0..1).
automix::Part MixAnalyzer::readCache(const QString &songId, Part part) const
{
    QFile file(cachePath(songId, part));
    if (file.fileName().isEmpty() || !file.open(QIODevice::ReadOnly))
        return {};
    diskcache::markUsed(file.fileName());
    const QByteArray bytes = file.readAll();
    if (bytes.size() < 3 || bytes.at(0) != CacheFormat || (bytes.size() - 1) % 2 != 0)
        return {};
    const qsizetype n = (bytes.size() - 1) / 2;
    automix::Part out;
    out.level.resize(n);
    out.beat.resize(n);
    for (qsizetype i = 0; i < n; ++i) {
        out.level[i] = -float(quint8(bytes.at(1 + i)));
        out.beat[i] = float(quint8(bytes.at(1 + n + i))) / 255.0f;
    }
    return out;
}

void MixAnalyzer::writeCache(const QString &songId, Part part, const automix::Part &measure) const
{
    if (measure.beat.size() != measure.level.size())
        return;
    QSaveFile file(cachePath(songId, part));
    if (file.fileName().isEmpty() || !file.open(QIODevice::WriteOnly))
        return;
    const qsizetype n = measure.level.size();
    QByteArray bytes(1 + 2 * n, Qt::Uninitialized);
    bytes[0] = CacheFormat;
    for (qsizetype i = 0; i < n; ++i) {
        bytes[1 + i] = char(quint8(std::clamp(std::lround(-measure.level.at(i)), 0L, 100L)));
        bytes[1 + n + i] = char(quint8(std::clamp(std::lround(measure.beat.at(i) * 255.0f), 0L, 255L)));
    }
    file.write(bytes);
    file.commit();
}

} // namespace legato

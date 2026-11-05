#include "logfile.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>
#include <QUrl>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

// Messages come from any thread: mpv's events, the cover pool, Qt's own.
QMutex lock;
// Never closed: messages arrive until the process ends.
QFile *file = nullptr;
qint64 written = 0;
QtMessageHandler previous = nullptr;

char letter(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return 'D';
    case QtInfoMsg: return 'I';
    case QtWarningMsg: return 'W';
    case QtCriticalMsg: return 'C';
    case QtFatalMsg: return 'F';
    }
    return '?';
}

void handle(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        // Before the previous handler: after a fatal message, Qt ends the program.
        const QMutexLocker locked(&lock);
        if (file && written <= LogFile::MaxBytes) {
            const QString time = QDateTime::currentDateTime().toString(u"yyyy-MM-dd HH:mm:ss.zzz"_s);
            const QLatin1StringView category(context.category ? context.category : "default");
            QByteArray line = (time + u' ' + QLatin1Char(letter(type)) + u' '
                               + (category == "default"_L1 ? QString() : category + u": "_s) + message + u'\n')
                                  .toUtf8();
            if (written + line.size() > LogFile::MaxBytes)
                line = "The log stops here, at its size limit.\n";
            file->write(line);
            file->flush();
            written += line.size();
            if (written > LogFile::MaxBytes)
                written = LogFile::MaxBytes + 1;
        }
    }
    if (previous)
        previous(type, context, message);
}

QString stateDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::StateLocation);
}

} // namespace

void LogFile::install()
{
    const QString dir = stateDir();
    if (dir.isEmpty() || !QDir().mkpath(dir))
        return;
    const QString log = dir + u"/legato.log"_s;
    const QString old = dir + u"/legato.old.log"_s;
    // A report is usually about the run that just ended. If another legato still writes the log
    // (Windows will not rename an open file), this run adds to it.
    QFile::remove(old);
    QFile::rename(log, old);
    auto *opened = new QFile(log);
    if (!opened->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete opened;
        return;
    }
    {
        const QMutexLocker locked(&lock);
        file = opened;
        written = file->size();
    }
    previous = qInstallMessageHandler(handle);
}

QString LogFile::path()
{
    const QMutexLocker locked(&lock);
    return file ? file->fileName() : QString();
}

QString LogFile::folder() const
{
    const QString log = path();
    return log.isEmpty() ? QString() : QDir::toNativeSeparators(QFileInfo(log).path());
}

void LogFile::showFolder() const
{
    const QString log = path();
    if (!log.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(log).path()));
}

} // namespace legato

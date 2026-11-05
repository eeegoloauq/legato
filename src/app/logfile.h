// The log file (docs/architecture.md, "Log"): every message Qt, QML, mpv and legato print, also
// written to legato.log in the state folder, with the previous run's beside it as legato.old.log.
// Started as the first thing in main(), so a report can come with what happened.
#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace legato {

class LogFile : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Log)
    QML_SINGLETON
    // Where the logs are, for the user to look: with the system's separators.
    Q_PROPERTY(QString folder READ folder CONSTANT)

public:
    // Each run writes at most this much: a warning in a loop cannot fill the disk.
    static constexpr qint64 MaxBytes = 4 * 1024 * 1024;

    using QObject::QObject;

    // Moves the last run's log aside and sends every message to a new one as well as where it went
    // before (the terminal or the journal). Needs the application's name set.
    static void install();
    // Empty until install(), or when the folder could not be written.
    static QString path();

    QString folder() const;
    // Opens the folder in the file manager.
    Q_INVOKABLE void showFolder() const;
};

} // namespace legato

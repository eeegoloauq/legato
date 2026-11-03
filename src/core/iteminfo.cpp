#include "iteminfo.h"

#include <QCoreApplication>
#include <QLocale>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("ItemInfo", text);
}

// The locale's long date without the weekday: "3 November 2026", "November 3, 2026".
QString dateText(const QDate &date)
{
    if (!date.isValid())
        return {};
    static const QRegularExpression weekday(u"\\s*dddd[,.]?\\s*"_s);
    const QLocale locale;
    QString format = locale.dateFormat(QLocale::LongFormat);
    format.replace(weekday, QString());
    return locale.toString(date, format.trimmed());
}

QString dateText(const QDateTime &time)
{
    return time.isValid() ? dateText(time.toLocalTime().date()) : QString();
}

// "4:05", "1:02:09".
QString clockText(int seconds)
{
    if (seconds <= 0)
        return {};
    const int hours = seconds / 3600;
    const QString rest = u"%1:%2"_s.arg((seconds / 60) % 60, hours > 0 ? 2 : 1, 10, '0'_L1).arg(seconds % 60, 2, 10, '0'_L1);
    return hours > 0 ? u"%1:%2"_s.arg(hours).arg(rest) : rest;
}

// The container and, where the server says, the resolution: "FLAC · 24-bit / 96 kHz", "MP3 ·
// 44.1 kHz". The bit rate has a row of its own.
QString formatText(const Song &song)
{
    const QString format = song.suffix.toUpper();
    if (format.isEmpty())
        return {};
    // 44.1 kHz keeps its decimal, 96 kHz has none.
    const QString rate = QString::number(song.samplingRate / 1000.0, 'g', 4);
    if (song.bitDepth > 0 && song.samplingRate > 0)
        return u"%1 · %2-bit / %3 kHz"_s.arg(format).arg(song.bitDepth).arg(rate);
    if (song.samplingRate > 0)
        return u"%1 · %2 kHz"_s.arg(format, rate);
    return format;
}

QString sizeText(qint64 bytes)
{
    return bytes > 0 ? QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeSIFormat) : QString();
}

QString countText(qint64 count)
{
    return count > 0 ? QLocale().toString(count) : QString();
}

// Always a row, so "never" reads as such and not as a missing field. (Servers leave out a count
// of zero.)
QString playsText(qint64 count)
{
    return count > 0 ? QLocale().toString(count) : tr("None");
}

void add(QList<InfoRow> &rows, const char *label, const QString &value)
{
    if (!value.isEmpty())
        rows.append({tr(label), value});
}

// Every genre here: the album's header shows only the first.
void addGenres(QList<InfoRow> &rows, const QStringList &genres)
{
    add(rows, genres.size() > 1 ? "Genres" : "Genre", genres.join(", "_L1));
}

} // namespace

QString durationText(qint64 seconds)
{
    const qint64 minutes = (seconds + 30) / 60;
    if (minutes < 60)
        return minutes == 1 ? QObject::tr("1 minute") : QObject::tr("%1 minutes").arg(minutes);
    const qint64 hours = minutes / 60;
    const qint64 rest = minutes % 60;
    const QString h = hours == 1 ? QObject::tr("1 hour") : QObject::tr("%1 hours").arg(hours);
    if (rest == 0)
        return h;
    return QObject::tr("%1, %2").arg(h, rest == 1 ? QObject::tr("1 minute") : QObject::tr("%1 minutes").arg(rest));
}

QList<InfoRow> songInfo(const Song &song)
{
    QList<InfoRow> rows;
    add(rows, "Album", song.album);
    QString track;
    if (song.track > 0)
        track = song.disc > 1 ? tr("%1 on disc %2").arg(song.track).arg(song.disc) : QString::number(song.track);
    add(rows, "Track", track);
    add(rows, "Year", song.year > 0 ? QString::number(song.year) : QString());
    addGenres(rows, song.genres);
    add(rows, "Length", clockText(song.duration));
    add(rows, "Format", formatText(song));
    add(rows, "Bit Rate", song.bitRate > 0 ? tr("%1 kbps").arg(QLocale().toString(song.bitRate)) : QString());
    add(rows, "Size", sizeText(song.size));
    rows.append({tr("Plays"), playsText(song.playCount)});
    add(rows, "Last Played", dateText(song.played));
    add(rows, "Date Added", dateText(song.created));
    add(rows, "Comment", song.comment);
    return rows;
}

QList<InfoRow> albumInfo(const Album &album, const QList<Song> &songs)
{
    QList<InfoRow> rows;
    add(rows, "Released", album.releaseDate.isValid() ? dateText(album.releaseDate)
                          : album.year > 0             ? QString::number(album.year)
                                                       : QString());
    addGenres(rows, album.genres);
    add(rows, "Songs", countText(songs.isEmpty() ? album.songCount : songs.size()));
    qint64 seconds = 0;
    qint64 bytes = 0;
    for (const Song &song : songs) {
        seconds += song.duration;
        bytes += song.size;
    }
    if (seconds == 0)
        seconds = album.duration;
    add(rows, "Length", seconds > 0 ? durationText(seconds) : QString());
    if (!songs.isEmpty()) {
        const QString first = formatText(songs.constFirst());
        const bool shared = std::all_of(songs.begin(), songs.end(),
                                        [&](const Song &song) { return formatText(song) == first; });
        add(rows, "Format", shared ? first : tr("Mixed"));
    }
    add(rows, "Size", sizeText(bytes));
    rows.append({tr("Plays"), playsText(album.playCount)});
    add(rows, "Last Played", dateText(album.played));
    add(rows, "Date Added", dateText(album.created));
    add(rows, "Label", album.recordLabels.join(", "_L1));
    return rows;
}

} // namespace legato

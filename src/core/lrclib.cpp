#include "lrclib.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace Qt::Literals::StringLiterals;

namespace legato::lrclib {

namespace {

constexpr auto Base = "https://lrclib.net/api/"_L1;
// How far LRCLIB's length may be from the server's, as /api/get allows.
constexpr int LengthSlack = 2;

void dropEmptyEnds(Lyrics &lyrics)
{
    // A synced song may open with a gap before the first words ("• • •"); unsynced ones may not.
    while (!lyrics.synced && !lyrics.lines.isEmpty() && lyrics.lines.constFirst().text.isEmpty())
        lyrics.lines.removeFirst();
    while (!lyrics.lines.isEmpty() && lyrics.lines.constLast().text.isEmpty())
        lyrics.lines.removeLast();
}

} // namespace

QUrl getUrl(const Song &song)
{
    QUrl url(Base + "get"_L1);
    url.setQuery(QUrlQuery{{u"track_name"_s, song.title},
                           {u"artist_name"_s, song.artist},
                           {u"album_name"_s, song.album},
                           {u"duration"_s, QString::number(song.duration)}});
    return url;
}

QUrl searchUrl(const Song &song)
{
    QUrl url(Base + "search"_L1);
    url.setQuery(QUrlQuery{{u"track_name"_s, song.title}, {u"artist_name"_s, song.artist}});
    return url;
}

Result parseRecord(const QJsonObject &record)
{
    Result result;
    if (record.value("instrumental"_L1).toBool()) {
        result.answer = Answer::Instrumental;
        return result;
    }
    result.lyrics = parseLrc(record.value("syncedLyrics"_L1).toString());
    if (result.lyrics.lines.isEmpty())
        result.lyrics = parsePlain(record.value("plainLyrics"_L1).toString());
    if (!result.lyrics.lines.isEmpty())
        result.answer = Answer::Lyrics;
    return result;
}

QJsonObject pickSearchRecord(const QJsonArray &records, int seconds)
{
    for (const QJsonValue &value : records) {
        const QJsonObject record = value.toObject();
        if (std::abs(record.value("duration"_L1).toDouble() - seconds) > LengthSlack)
            continue;
        // Search lists every upload; one marked instrumental does not rule out another with words.
        if (parseRecord(record).answer == Answer::Lyrics)
            return record;
    }
    return {};
}

Lyrics parseLrc(const QString &text)
{
    static const QRegularExpression tags(u"^((?:\\s*\\[[^\\]]*\\])+)(.*)$"_s);
    static const QRegularExpression tag(u"\\[([^\\]]*)\\]"_s);
    static const QRegularExpression time(u"^(\\d+):(\\d+(?:[.:]\\d+)?)$"_s);
    static const QRegularExpression offsetTag(u"^offset:\\s*([+-]?\\d+)$"_s, QRegularExpression::CaseInsensitiveOption);

    Lyrics lyrics;
    lyrics.synced = true;
    qint64 offset = 0;
    for (const QString &raw : text.split(u'\n')) {
        const QRegularExpressionMatch line = tags.match(raw.trimmed());
        if (!line.hasMatch())
            continue;
        const QString words = line.captured(2).trimmed();
        auto each = tag.globalMatch(line.captured(1));
        while (each.hasNext()) {
            const QString inside = each.next().captured(1).trimmed();
            if (const auto t = time.match(inside); t.hasMatch()) {
                const double seconds = t.captured(1).toInt() * 60 + QString(t.captured(2)).replace(u':', u'.').toDouble();
                lyrics.lines.append({qint64(std::llround(seconds * 1000)), words});
            } else if (const auto o = offsetTag.match(inside); o.hasMatch()) {
                offset = o.captured(1).toLongLong();
            }
        }
    }
    std::stable_sort(lyrics.lines.begin(), lyrics.lines.end(),
                     [](const LyricLine &a, const LyricLine &b) { return a.start < b.start; });
    // A positive offset shows the lines sooner.
    for (LyricLine &line : lyrics.lines)
        line.start = std::max<qint64>(0, line.start - offset);
    dropEmptyEnds(lyrics);
    if (lyrics.lines.isEmpty())
        lyrics.synced = false;
    return lyrics;
}

Lyrics parsePlain(const QString &text)
{
    Lyrics lyrics;
    for (const QString &line : text.split(u'\n'))
        lyrics.lines.append({-1, line.trimmed()});
    dropEmptyEnds(lyrics);
    return lyrics;
}

} // namespace legato::lrclib

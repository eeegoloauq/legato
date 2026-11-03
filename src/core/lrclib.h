// LRCLIB (lrclib.net), the open lyrics database: asked for a song the server has no lyrics for
// (docs/architecture.md, "Lyrics"). Only the song's title, artist, album and length are sent.
#pragma once

#include "subsonic.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

namespace legato::lrclib {

// What LRCLIB knows about a song.
enum class Answer {
    // `lyrics` holds them, synced when LRCLIB has them synced.
    Lyrics,
    // LRCLIB says the song has no words.
    Instrumental,
    // LRCLIB has no such song.
    None,
};

struct Result {
    Answer answer = Answer::None;
    Lyrics lyrics;
};

// /api/get for `song`: LRCLIB matches the title, artist and album exactly and the length within two
// seconds, and asks the sources it knows when its own database has no match.
QUrl getUrl(const Song &song);
// /api/search by title and artist, for songs /api/get misses because the album is named differently.
QUrl searchUrl(const Song &song);

// One LRCLIB record (as /api/get answers, and each entry of /api/search).
Result parseRecord(const QJsonObject &record);
// The first /api/search entry within two seconds of `seconds` that has lyrics; empty otherwise.
QJsonObject pickSearchRecord(const QJsonArray &records, int seconds);

// LRC text: "[mm:ss.xx] words" lines, several time tags on one line, [offset:ms] honoured, other
// tags ([ar:], [length:] ...) dropped. Sorted by time.
Lyrics parseLrc(const QString &text);
// Plain text, one line each, without empty lines at either end.
Lyrics parsePlain(const QString &text);

} // namespace legato::lrclib

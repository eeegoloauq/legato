// Automatic crossfade (docs/architecture.md, "Gapless and crossfade"): where one song's outro fades
// and the next one's intro starts, from the loudness of the parts around the change. The player
// decodes those parts; this only measures them and plans the change.
#pragma once

#include "crossfade.h"

#include <QList>

#include <optional>

namespace legato::automix {

// The parts measured: the end of the old song and the start of the new one.
constexpr double TailSeconds = 30;
constexpr double HeadSeconds = 20;
// Length of one level in an envelope.
constexpr double WindowSeconds = 0.05;
// Most of a change that overlaps; the player also keeps it under a third of either song.
constexpr double MaxOverlap = 10;
// The level of digital silence.
constexpr float Silence = -100;

// What the plan is made from: one value per whole 50 ms window of a part.
struct Part {
    // The level (RMS, dBFS).
    QList<float> level;
    // How clearly a beat repeats around the window, 0 (none) to 1 (a click track).
    QList<float> beat;
};

// The level of each whole 50 ms window of mono samples at `rate`.
QList<float> envelope(const qint16 *samples, qsizetype count, int rate);
// The beat around each whole 50 ms window: how strongly the onsets of the kick and bass and of the
// snare and cymbals repeat at one period of a tempo between 60 and 200 BPM, over six seconds.
QList<float> beats(const qint16 *samples, qsizetype count, int rate);
Part measure(const qint16 *samples, qsizetype count, int rate);

// The change from a song ending with `tail` (its last TailSeconds, or all of a shorter song) into
// one starting with `head`, overlapping at most `maxOverlap` seconds. Nothing when either part is
// silent all through: the player then uses a fixed fade.
//
// The new song comes in once the old one has faded well below its loudest, so a hard ending gets
// next to no overlap and a long fade-out overlaps up to `maxOverlap`. A quiet intro starts that much
// earlier, under the old song, at its own volume; a loud one fades in quickly. Silence at the end of
// the old song and the start of the new one is skipped.
//
// Two beats that are not matched never play together for long: when both songs have one where they
// would overlap, the old song goes within a second of the new one coming in, and a quiet intro does
// not start under it. A longer overlap swaps the bass as the new song arrives, so only one song's low
// end plays at a time.
//
// `judged`, when given, gets what the plan was judged on, for the log.
struct Judged {
    // The mean beat of each song where they would overlap.
    float oldBeat = 0;
    float newBeat = 0;
    // Both had a beat, so the change was made short.
    bool beatsClash = false;
};
std::optional<crossfade::Plan> plan(const Part &tail, const Part &head, double maxOverlap, Judged *judged = nullptr);

} // namespace legato::automix

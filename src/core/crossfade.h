// Crossfade rules (docs/architecture.md, "Gapless and crossfade"): which song changes overlap, for
// how long, and the volumes during the overlap. The player runs the two songs; this only decides.
#pragma once

#include "subsonic.h"

#include <algorithm>
#include <utility>

namespace legato::crossfade {

// `to` is the track after `from` on their album: the next number on the same disc, or the first of
// the next disc. Songs without track numbers count as following when they share the album.
bool albumContinues(const Song &from, const Song &to);

// Seconds the change from a song of `fromDuration` into one of `toDuration` overlaps: `seconds`, but
// at most a third of either song, so a short interlude is never mostly fade. Unknown durations (0)
// do not limit it.
double length(double seconds, double fromDuration, double toDuration);

// Amplitude gains (outgoing, incoming) at `progress` 0..1 of the fade. Equal power: two unrelated
// songs keep the loudness of one through the middle, where straight lines would dip by 3 dB.
std::pair<double, double> gains(double progress);

// How one song changes into the next, in seconds. Times before the end count back from the end of the
// old song; times in the overlap count from its start.
struct Plan {
    // The new song starts this long before the old one ends,
    double start = 0;
    // and the old one stops this long before its end: its trailing silence, or an outro cut short.
    double stop = 0;
    // Where in the new song it starts: after its leading silence.
    double inFrom = 0;
    // When the old song begins to fade out; it plays on at full volume before.
    double outFadeFrom = 0;
    // How long the new song takes to fade in; 0 starts it at full volume (its intro is quiet).
    double inRamp = 0;
    // When the songs swap their low end, in seconds into the overlap: before it the new song plays
    // with its bass cut, after it the old one does, as DJs swap the bass. Below 0 both keep theirs.
    double bassSwap = -1;

    double overlap() const { return std::max(0.0, start - stop); }
};

// A fade of `seconds` at the very end of the old song, both songs on equal-power curves.
Plan fixedPlan(double seconds);

// Amplitude gains (outgoing, incoming) `seconds` into the overlap of `plan`: each song on its own
// quarter of the equal-power curve, so a fixed plan keeps the loudness of one song through the middle.
std::pair<double, double> gains(const Plan &plan, double seconds);

// The bass swap: a low shelf at BassHz that cuts up to BassCutDb, moved across in BassSwapSeconds.
constexpr double BassHz = 200;
constexpr double BassCutDb = 24;
constexpr double BassSwapSeconds = 0.5;

// How much of each song's low end is cut (outgoing, incoming), 0 (none) to 1 (BassCutDb),
// `seconds` into the overlap of `plan`.
std::pair<double, double> bassCuts(const Plan &plan, double seconds);

// The mpv volume that plays at `gain` times the amplitude of the user's `volume`. mpv's volume is
// cubic (its gain is (volume / 100)^3).
double mpvVolume(int volume, double gain);

} // namespace legato::crossfade

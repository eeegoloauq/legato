// The mpv options every instance shares: the two decks (player.h) and the analyzer (mixanalyzer.h).
#pragma once

struct mpv_handle;

namespace legato::mpvoptions {

// Audio only, nothing from the user's mpv setup and nothing of mpv's own that legato does not use.
// Set before mpv_initialize.
void setCommon(mpv_handle *mpv);

} // namespace legato::mpvoptions

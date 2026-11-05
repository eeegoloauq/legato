# 0002: Performance

Date: 2026-11-05. Status: accepted.

How Legato keeps its memory and its frames in check, and where to look when either grows. The work
these directions came from is in the roadmap, Performance.

## Where to look first

A change is worth a second look for cost when it adds or touches:

- an mpv option, a deck or the analyzer;
- a kept page (`keptPages`), a `Loader`, a view's `cacheBuffer`;
- a C++ property QML binds to, or a `model:`;
- something connected to the playback position;
- an animation, a `layer`, a `MultiEffect`, a shader or the glass;
- a decoded cover.

## Memory

**mpv's buffer is ours to bound.** mpv's defaults are for streaming video from the internet: up to
150 MiB read ahead, 50 MiB kept behind and `cache-secs` of an hour, per instance. For a song that
means the whole file, then the next one once `prefetch-playlist` starts, on each deck. Each deck
gets a time-based readahead (`cache-secs`) with a byte ceiling for hi-res files and a small back
buffer; the analyzer reads no cache at all. Prefetch must still start before the preload of a fade.

**A high point stays.** glibc keeps the small chunks a thread freed in that thread's arena and gives
back only the top of it, so a whole song's packets freed by mpv's demuxer stay resident. The fix is
a lower high point. `malloc_trim`, `MALLOC_ARENA_MAX` and a forced garbage collection hide where the
memory went and are not used.

**What mpv loads is what Legato uses.** `load-scripts=no` skips the user's scripts only; mpv's
built-in ones (stats, console, select, auto profiles and others) load in every instance, each a Lua
thread receiving every event. Each is turned off by its own option.

**Hidden is not gone.** A kept page and a delegate in a view's cache buffer keep their covers, as the
decoded copy and the GPU texture. Decoded covers live in one cache with a size in bytes, so memory
has a ceiling however many pages are kept. A `cacheBuffer` is about one screen.

**Covers are decoded at the size shown.** `RoundedImage` asks for the longest side times the device
pixel ratio and the provider scales while decoding. The 512 MB disk caches bound nothing in RAM.

## Work per update

- A getter that converts (`QVariantList` of maps, a regex) runs again for every binding on every
  read. Keep the converted value and change it when its source changes.
- One NOTIFY signal per property, emitted only when the value changed.
- A list QML shows is a model (`syncRows`, `dataChanged`), never a large `QVariantList`.
- `positionChanged` follows mpv's `time-pos`, about 11 times a second for FLAC. What connects to it
  compares numbers; the current lyric line is worked out in C++ and changes only with the line.
- No synchronous mpv call on the GUI thread: `mpv_set_property_string` waits for mpv's core. Use
  the async set or a `set` command.
- Large server replies are parsed off the GUI thread.

## Rendering

- Nothing animates unseen. An animation runs on the item being visible, not on `Player.playing` or
  a `live` flag; a stopped animation costs nothing, a paused one still ticks.
- A delegate in a view's cache buffer counts as visible. Make the animated part (the level meter)
  only in the row that shows it.
- Any redraw under the player bar or the scrolled top bar captures and blurs the page again, so one
  stray animation costs a full-window blur every frame.
- `layer.enabled` is an offscreen texture and a render pass for each item and breaks batching. It does
  not go on something repeated in a list, such as an icon: tint in a shader that reads the image, as
  `RoundedImage` does, or when the icon is loaded.
- Idle means no frames. Paused, or playing on a page without a playing row, `--frame-meter` and
  `QT_LOGGING_RULES=qt.scenegraph.time.renderloop=true` show no steady redraw.

## Measuring

- Before and after a change, the same steps on the same build against `scripts/fake-subsonic.py`:
  start, browse a fixed path, play through a crossfade, pause, idle, repeated 5 to 10 times.
- Memory is PSS and SwapPss from `/proc/<pid>/smaps_rollup`; each deck's buffer is
  `demuxer-cache-state` (`fw-bytes`); allocation stacks come from heaptrack, which slows every
  allocation, so a delay seen under it is not the app's.
- A level that stops growing across the repeats is the working set. One that grows with every repeat
  is a leak.
- Sanitizers find what a reading misses: use after free, overflows, undefined behaviour and memory
  nothing points to at exit. They see only the code the tests run, and memory still referenced (an
  oversized cache) is no leak to them.

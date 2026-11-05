# Roadmap

## Scope

In:

- Library browsing: Home, Recently Added, Artists, Albums, Songs, Favorites, Playlists.
- Playback with gapless, crossfade, queue, shuffle, repeat.
- Instant Mix.
- Synced lyrics from the server, optional LRCLIB fallback.
- Queue resume through the server.
- Multiple servers, one active at a time.
- Search palette and full keyboard control.
- Media keys and the desktop's media controls.
- Light, dark and system themes.

Out, for now: offline downloads, equalizer, visualizer, tray icon, internet radio, merged libraries
of several servers, transcoding, volume leveling (gain tags in real libraries are too often wrong to
apply, and measuring loudness would mean decoding every song in full), song change notifications
(the desktop's media controls already show the song).

## 0.1

Everything in scope above, on Linux (tarball, RPM, Arch package) and Windows (installer).

## Performance

From a read of the code on 2026-11-05. Measured on the desktop: 0.1.1 had 555 MiB PSS a few minutes
into playback, at least 138 MiB of it memory glibc had freed and kept. Directions and how to measure
are in [decision 0002](decisions/0002-performance.md). In order, each measured before and after:

1. Bounded buffers on both decks (`player.cpp`): `cache-secs`, a `demuxer-max-bytes` ceiling and a
   small `demuxer-max-back-bytes`. With mpv's defaults one instance holds the whole file and then the
   next; the mpv command line on a 33 MB FLAC playlist held 83 MB by default and 19 MB with
   `cache-secs=30`. Gapless and both crossfade modes checked by ear.
2. `cache=no` for the analyzer (`mixanalyzer.cpp`): it reads the rest of the file ahead and throws it
   away.
3. mpv's built-in scripts off in all three instances: seven Lua threads each.
4. Nothing animating unseen. Every `SongRow` and `CompactSongRow` makes a `LevelMeter` that runs while
   music plays, rows in the cache buffer and on kept pages included, so the window redraws at 60 fps
   and the glass blurs the page again each frame. `GenCover`'s drift runs on `live`, visible or not;
   `MixingLabel` keeps its layer while hidden.
5. `mpv_set_property_string` on the GUI thread (`player.cpp`, `mixanalyzer.cpp`) made async.
6. `Glyph` puts a layer and a `MultiEffect` on every icon.
7. Covers on the four kept pages and Home's `cacheBuffer: 3000`: one decoded-cover cache with a size
   in bytes in `CoverProvider`, covers released on parked pages. Measured with `keptPages: 1` first.
8. Subsonic replies parsed off the GUI thread; lyrics as a model with the current line in C++.
9. Units and QML tests under AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer in CI,
   with a suppressions file for Qt's and Mesa's own allocations.

## Next

- A web demo on GitHub Pages: the same QML built with Qt for WebAssembly, single-threaded (Pages
  cannot send the headers threads need), over an invented library bundled into the page, made from
  `scripts/fake-subsonic.py`. No playback: the player bar shows a song and does not play it. First a
  size check of the build, compressed as Pages serves it: about 10 MB is fine.
- Windows: media keys and the media flyout (SMTC), window snapping and shadow for the frameless
  window.
- An endless Instant Mix.
- A streaming quality setting (Opus, MP3) for slow connections.
- Flatpak on Flathub, once its KDE runtime has Qt 6.12.
- macOS.

## Experiments

- Beat-matched crossfade and DJ-style cue points. Two mpv instances start tens of milliseconds apart
  and drift, so this needs both songs mixed in one audio stream first.

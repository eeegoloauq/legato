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
are in [decision 0002](decisions/0002-performance.md).

- By ear on the desktop: gapless and both crossfade modes with the bounded mpv buffers
  (`cache-secs=30`), and no more clipping from the bass swap's shelf in the log.

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

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

## Next

- Windows: media keys and the media flyout (SMTC), window snapping and shadow for the frameless
  window.
- An endless Instant Mix.
- A streaming quality setting (Opus, MP3) for slow connections.
- Flatpak on Flathub, once its KDE runtime has Qt 6.12.
- macOS.
- A size limit for the disk caches under `~/.cache/legato/` (covers, crossfade levels, lyrics),
  which grow with every album shown and song played and are never cleaned. Remove the least
  recently used files over the limit, as lumeo's artwork cache does (`core/internal/api/artwork.go`
  in lumeo: 512 MB, a file's mtime set on each use).

## Experiments

- Beat-matched crossfade and DJ-style cue points. Two mpv instances start tens of milliseconds apart
  and drift, so this needs both songs mixed in one audio stream first.

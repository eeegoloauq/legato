# Architecture

Legato is a desktop client for Navidrome and other OpenSubsonic servers. It targets Linux first
(Fedora, GNOME, Wayland) and also runs on Windows.

Goals: a small, calm interface built around album art; few dependencies; lossless playback by
default; good keyboard control.

## Stack

Qt Quick and C++ with libmpv. [Decision 0001](decisions/0001-qt-quick.md) records why.

| Layer | Choice |
|---|---|
| Interface | Qt Quick (QML), Qt 6.12, Basic controls style restyled to the design. |
| Application layer | C++20, exposed to QML as singletons (`Session`, `Library`, `Player`) and models. |
| Audio | libmpv, audio only (no video output, no render API). |
| HTTP and JSON | Qt Network (`QNetworkAccessManager`) and Qt's JSON classes. |
| Effects | Qt Quick Effects (`MultiEffect` blur, `RectangularShadow`) and own shaders built with Qt Shader Tools. |
| Images | Qt's image readers; the Qt Image Formats module adds WebP covers. |
| Media keys / now playing | MPRIS over Qt D-Bus on Linux; SMTC on Windows later. |
| Credentials | The system keychain through QtKeychain (Secret Service on Linux, Credential Manager on Windows). |
| Tests | Qt Test for C++, `qmltestrunner` for QML. |

### Layout

```
src/core     Qt-only logic without QML: Subsonic client, server list, cover cache, palette, play queue
src/app      QML-facing classes: Session (servers, sign-in, keychain), Library, Player (libmpv), the
             list models, the pages' data objects, cover colors, cover image provider, frame meter
src/main.cpp the executable: command line, engine, development options
qml          the interface, one QML module (Legato)
shaders      fragment shaders: rounded covers, drifting blobs, edge fade, shimmer, tint
icons        Material Symbols Rounded, only those in use
tests        Qt Test executables and QML tests
scripts      install-qt.sh (the pinned Qt), fake-subsonic.py (a test server)
packaging    the app icon and its generator, desktop entry, AppStream file, release scripts
docs         design, architecture, roadmap and decisions
```

Everything but `main()` builds into one static library with the QML module, so tests load the same
module the app does. The executable is made in `src/`, a directory without QML, so the deploy step scans only the module.

The QML is a view. It calls `Session`, `Library` and `Player` (sign in, play, seek, enqueue) and
binds to their properties. `Session` owns the server list and the passwords and hands `Library` the
active server's credentials; a switch stops playback and every page reloads from the new server,
dropping answers still on the way from the old one. Pages live in a `StackView` in `Main.qml` with
one history for back and forward; each page is a `ContentPage` that names its title and its
scrolling view, and anything can open a page through the `Navigation` singleton. Network access,
playback, decoding and caching happen in C++, off the GUI thread where they could stall a frame.

Each library page declares the data it shows: `AlbumDetails { albumId: ... }`,
`ArtistDetails`, `PlaylistDetails`, `AlbumList { type: "newest" }`, `ArtistList`, `AllSongs`,
`Favorites` (`src/app/pages.h`). They load when complete and when their id changes, reload on a
server switch, and follow stars set anywhere (`Library.starChanged`). An answer for a page that was
closed or reloaded meanwhile is dropped, so a slow server never writes into the wrong page or a
destroyed one. Their rows are list models (`src/app/models.h`) with a `count` for bindings. Playing,
queueing and starring go through `Library`, which hands songs to `Player`.

## Server API

Legato speaks the OpenSubsonic API, so it works with Navidrome, gonic and other compatible servers.
Navidrome's own native API is internal and is not used.

- Authentication: token auth (`t = md5(password + salt)`, `s` a fresh random salt per request). The
  password never goes over the wire, but Legato needs the password to compute it, so the keychain
  holds the password. The list of servers (address, username, name, which is active) is in the
  settings file; passwords never are. `LEGATO_TEST_SERVER_URL`, `LEGATO_TEST_USERNAME` and
  `LEGATO_TEST_PASSWORD` override the saved servers for tests and development.
- Several servers can be configured. One is active at a time; Settings switches between them.
  Libraries are not merged.
- Streaming uses `stream` with `format=raw`, so the server sends original files and never transcodes.
  A quality setting (MP3 320, Opus 192/128) can come later.

Endpoints in use, tested against Navidrome 0.64.1:

| Feature | Endpoint |
|---|---|
| Home lists, Albums page | `getAlbumList2` (`newest`, `random`, `frequent`, `alphabeticalByName`, `alphabeticalByArtist`, `byYear` with `fromYear=9999&toYear=0` for newest first) |
| Album, artist pages | `getAlbum`, `getArtist`, `getArtists`; artist photos are `getCoverArt` of the artist's `coverArt`, so `getArtistInfo2` is not needed yet |
| Songs page | `search3` with an empty query, 500 songs a page as the list scrolls; the count from `getScanStatus` |
| Search | `search3` (palette: 4 artists, 5 albums, 8 songs, after a 150 ms pause in typing) |
| Artwork | `getCoverArt` with `size` |
| Playback | `stream` |
| Shuffle library | `getRandomSongs` |
| Scrobbling | `scrobble` with `submission=false` at start, `submission=true` after 50 % or 4 minutes of listening |
| Lyrics | `getLyricsBySongId` (OpenSubsonic `songLyrics` extension), synced when available |
| Instant Mix | `getSimilarSongs2` by artist (Navidrome asks its metadata agents, such as Last.fm; with none it finds nothing and Legato says so) |
| Top songs | own play counts first (`playCount` of the artist's songs), `getTopSongs` (Last.fm, matched to the library) as fallback |
| Queue resume | `savePlayQueue` / `getPlayQueue` (current song, position in ms, `changedBy`, `changed`) |
| Favorites | `star`, `unstar` with `id`, `albumId` or `artistId`, `getStarred2` |
| Playlists | `getPlaylists`, `getPlaylist` |
| Multiple artists per song | OpenSubsonic `artists` arrays |
| Singles and EPs | OpenSubsonic `releaseTypes`; if missing, albums with 1–3 songs count as singles |

Scrobbling to Last.fm or ListenBrainz is done by the server. Legato only reports plays to the server,
which also keeps the play counts behind "Most played" from them. A song longer than 30 seconds counts
once it has played for half its length or four minutes (Last.fm's rule); time skipped by seeking
does not count, and a song that starts over on repeat counts again (`src/core/scrobbletracker.*`).
Listens the server did not receive because it was out of reach are kept in memory and sent with the
next one, several in one request; a server switch or quitting drops them.

## Playback engine

mpv is used for streaming, buffering, gapless playback, codec support and audio device
selection. Legato talks to libmpv directly: options and commands from the GUI thread, events drained
on the GUI thread after mpv's wakeup callback.

Legato owns the queue (`src/core/playqueue.*`): the songs as given and the order they play in, with
its own shuffle. mpv's playlist only holds the current song and the next one, so mpv fetches the
next song early (`prefetch-playlist`) and plays the two without a gap (`gapless-audio`). mpv's
own shuffle is not used: it moves the playing song too, and its playlist positions do not say which
list they belong to.

### Gapless and crossfade

mpv plays one file at a time and cannot overlap two songs, so `Player` runs two mpv instances
("decks"). The active deck plays the song the interface shows; the other one is idle, or loads the
next song, or plays the old song out during a fade. The rules (which changes fade, how long, the
volume curve) are in `src/core/crossfade.*`. Every change follows a plan (`crossfade::Plan`):
when the new song starts and the old one stops, counted back from the old song's end, where in the
new song it starts, and the two volume ramps.

Modes (Settings > Playback > Crossfade: Off, Automatic, Fixed):

1. **Off**: gapless playback on the active deck, no overlap.
2. **Fixed**: a song that ends by itself overlaps the next by the configured length
   (1–12 s), at most a third of either song. Ten seconds before the fade the other deck loads the
   next song, paused and silent. At the fade point the decks swap: the queue, the player bar,
   scrobbling and MPRIS move to the new song at once, and the volumes follow equal-power curves
   (cosine out, sine in) in 20 ms steps. mpv's volume is cubic, so a gain g is set as
   volume × g^(1/3). A pause stops both decks and the fade's clock; a seek finishes the fade at once
   (it is a seek in the new song). Next, Previous and a click in Up Next cut without a fade, as in
   other players. A next song that is still loading when the old one ends starts as soon as it has
   loaded; one that fails is tried once more as an ordinary change.
3. **Automatic**: fits each change to the two songs. 60 s before the end, `MixAnalyzer` (`src/app/mixanalyzer.*`) has a third mpv, never
   heard, decode the old song's last 30 s and the new song's first 20 s: `ao=pcm` writes mono 16-bit
   samples at 22,050 Hz to a temporary file as fast as it decodes (a 30 s part takes a fraction of a
   second plus the download; `start=-30` makes mpv fetch only the end with a range request).
   `src/core/automix.*` turns the samples into 50 ms RMS levels and plans the change, judging
   levels on one-second averages against each song's loudest second: the new song comes in once the
   old one has fallen 12 dB, and the old one stops at its last sound (45 dB down), so a long
   fade-out overlaps up to 10 s and a hard ending hardly at all. A quiet intro (more than a second
   before it comes within 6 dB of its loudest) starts up to 6 s earlier, under the old song at full
   volume and at its own level; a loud start fades in over 1.5 s. Silence at the end of the old song
   and at the start of the new one is skipped (the preloaded deck is seeked past it). Overlaps over
   10 s, or a third of either song, cut the old song's fade short. Where a part cannot be decoded
   within 20 s, or is silent all through, the change is a fixed 6 s fade. The levels are cached on
   disk per server under `~/.cache/legato/mix/`, a format byte and two bytes per 50 ms (level and
   beat). Measuring runs on a worker thread (about a tenth of a second per part). Each plan is
   logged under `legato.automix`, with the beats it was judged on, for tuning by ear.

   **Beats**: two beats that are not matched must not play together, the first rule of DJ mixing. The beat is measured from onsets
   in six band-pass bands (60 Hz to 6 kHz) in 10 ms steps: 40 ms band power, rises over 30 ms
   above 1 dB (a held tone's ripple is no onset), counted above their mean over half a second; then
   the autocorrelation of six seconds of onsets at the best period between 0.3 and 1 s (200–60 BPM),
   above its median, against that at no delay (a "pulse clarity", 0–1). Beatless music scores about 0.1–0.3 and music with drums
   0.3–0.8; a part has a beat from 0.3. When both songs have
   one where they would overlap, the change becomes short: the new song still comes in where the
   old one has fallen 12 dB, and the old one goes within 1 s (as radio automation segues), and a
   quiet intro does not start under the old song. One beat over a song without keeps the long
   blend. A song whose last 30 s are one repeated beat to a hard end gets no overlap either way.

   **Bass swap**: an overlap of 2 s or more plays the new song with its low end cut until its body
   arrives (where the old song begins to fade), then swaps in 0.5 s: the old song's low end is cut
   and the new one's comes back, so two kicks and two basslines never stack. Each deck has a low
   shelf at 200 Hz in its filter chain from the start (`af=@swap:lavfi=[lowshelf@swap=...]`, 0 dB:
   adding a filter later would restart the audio chain, a gap), and `af-command swap g <dB>
   lowshelf@swap` moves its gain to -24 dB and back in the fade's 20 ms steps (the target must name
   the filter: the default "all" fails). Fixed crossfade stays a plain fade.

"Keep albums gapless" (on by default) never crossfades into the next track of the same album (the
next number on the same disc, or track 1 of the next disc), so live albums and continuous mixes
stay seamless. Songs of one album in shuffled order still fade.

Beat-matching is not possible with two mpv decks: each is its own audio stream, started tens of
milliseconds apart and drifting. It would need both songs mixed in one stream.

### Queue resume

`QueueSync` (`src/app/queuesync.*`) keeps the queue on the server. It saves with `savePlayQueue`
2 s after a song change, a queue edit or a seek, half a second after a pause, every 30 s while a
song plays, and at quit (waiting up to 1.5 s for the answer). Only Legato's own changes mark the
queue for saving, so a Legato left open and idle never writes over a queue another client saved
since. A server switch first sends the last change to the server being left.

The ids go in a POST form when the server lists OpenSubsonic's `formPost` extension (checked with
`getOpenSubsonicExtensions` after signing in): 500 ids make a URL of about 14 KB, and proxies such
as nginx refuse request lines over 8 KB by default. Without it, a long queue is saved from the
current song on, at most 200 songs.

After signing in, `getPlayQueue` puts the saved queue in the player when nothing plays yet, in the
saved play order with shuffle off. The song shows paused at its position; mpv loads nothing until
play is pressed, and the server is not told it plays. This also continues a queue saved by another
Subsonic app. Until something is played or changed in Legato, the window coming to the front (at most
every 10 s) asks again and takes a newer queue saved elsewhere meanwhile.

## Desktop integration

- Media keys and the GNOME player controls work through MPRIS, implemented over Qt D-Bus
  (`src/app/mpris.*`, bus name `org.mpris.MediaPlayer2.legato`). Changes go out as one
  `PropertiesChanged` per event loop turn with only the values that changed; the position is read
  on request, and `Seeked` reports jumps. The cover is the cached file on disk (`mpris:artUrl`),
  never a server URL, which would carry the sign-in token. Stop pauses and goes back to the start.
- Wayland does not let apps grab global shortcuts. Control from outside the window goes through
  MPRIS (media keys, `playerctl`, desktop shortcuts). The XDG GlobalShortcuts portal can be added later.
- No song change notifications: GNOME's media controls come from MPRIS and already show the song.
- No tray icon. GNOME shows none without an extension, and MPRIS covers the use case.
- The window is frameless on every platform. Legato draws its own drag area, window buttons and
  resize edges, so GNOME does not add a header bar above the design. The app id is
  `io.github.eeegoloauq.legato`: the Wayland app id, the `.desktop` file and the AppStream id.
- Settings live in `~/.config/legato/settings.conf`, caches (covers, crossfade levels, lyrics) in
  `~/.cache/legato/`; on Windows both under `%LOCALAPPDATA%\legato`.
- Linux cannot blur behind a window, so the sidebar is solid. The glass (header, player bar, menus,
  the search's results, the notice) blurs Legato's own content, which works everywhere
  (`qml/GlassPanel.qml`). A panel finds where it is in what it blurs before every frame: a binding
  sees only its own position, which stays at zero inside a parent that moves (the player bar, a
  menu), and the player bar blurred the top of the page until 0.1.1. Settings > Transparency turns
  the blur off.

## Artwork and colors

`CoverStore` downloads covers with `getCoverArt` in a few size buckets and keeps them on disk under
the cache directory, one folder per server and user. File names are hashes of the cover id, so a
server's ids never form a path. `CoverProvider` serves `image://cover/<id>` to QML: it decodes and
center-crops on a worker thread at exactly the size the image is shown, so the GPU never scales
covers and scrolling never decodes on the GUI thread.

The accent color of the playing song (progress line, primary buttons, full-screen background) comes
from `src/core/palette.*`: three prominent colors of the cover, and a version of the first that
reads as text on the light or dark background (WCAG 4.5:1, measured as relative luminance: HSL
lightness put a yellow accent at 1.2:1 on white), so every cover gives a visible accent.

## Lyrics

Lyrics come from the server first (embedded tags or `.lrc` files, via `getLyricsBySongId`). LRCLIB
is asked only when the server has none, and only when Settings > Playback > "Find missing lyrics on
LRCLIB" is on. It is off by default because it sends the song's title, artist, album and length to
lrclib.net. LRCLIB is free, needs no key and holds crowd-sourced synced lyrics; Musixmatch needs a
commercial key and Genius has no lyrics API.

- One song at a time, when it starts playing: `/api/get` (exact title, artist and album, length
  within 2 s), then `/api/search` by title and artist for an entry of the same length, which
  catches albums named differently on the server. No database download.
- Answers are kept in `~/.cache/legato/lyrics/` by artist, title, album and length (not the server's
  id, so another server reuses them): lyrics and "instrumental" for good, "none" for 30 days. A
  failed request is not kept: LRCLIB is often busy (503), and the song is asked again next time.
- Synced lyrics are preferred over plain ones. Rules in `src/core/lrclib.*` (tested), network and
  cache in `src/app/lyricsfinder.*`.

## Packaging and releases

A tag `vX.Y.Z` runs `.github/workflows/release.yml`, which runs the scripts in `packaging/` (the same
ones a person runs by hand) and publishes one GitHub release:

- `build-dist.sh`: the Linux bundle (the binary with the Qt it was tested with; libmpv and QtKeychain
  stay the system's), the icons in every size and the tarball.
- `build-rpm.sh` (`legato.spec`) and `build-arch.sh` (`PKGBUILD`) repackage that tarball for Fedora
  and Arch without building again, so every Linux file carries the same bytes.
- `build-windows.sh`: the Windows bundle with libmpv and QtKeychain, and the installer (`legato.iss`,
  per user, no administrator rights).

Release notes are written once, in the AppStream file's `<releases>`
(`packaging/io.github.eeegoloauq.legato.metainfo.xml`): `release-notes.py` turns them into the GitHub
release text and the RPM changelog, under the tag message as the headline. A version with a dash
(`v0.1.1-beta.1`) is a pre-release. `ci.yml` runs the tests on Linux and Windows for every push to
`main` that touches code, and a release waits for it.

## Dependencies

Every dependency is attack surface, so the list stays short: Qt and libmpv. No npm, no Cargo.

- Qt 6.12.0 with the Quick, Quick Controls, Network, D-Bus, Shader Tools and Image Formats modules,
  from `scripts/install-qt.sh`, which pins Qt's version and the aqtinstall commit that downloads it.
  The same Qt builds, tests and goes into the release bundle.
- QtKeychain from the system on Linux (Fedora and Arch `qtkeychain-qt6`, Debian `qtkeychain-qt6-dev`
  to build), linked dynamically like libmpv: a distribution's signed package is a safer source than a
  download, and the packages require it. On Windows, `packaging/build-qtkeychain.sh` builds a pinned
  release, checked by its SHA-256.
- libmpv from the system on Linux (Fedora `mpv-libs`, Arch `mpv`, Debian `libmpv2`). On Windows,
  `packaging/fetch-libmpv.sh` downloads a pinned build, checked by its SHA-256.
- C++ has no lockfile. A new library is added only when a few dozen lines of own code cannot do the
  job, is scanned with `osv-scanner` where it has a manifest, and is not adopted in its first week.
- QtKeychain over libsecret directly: the same code serves the Windows and macOS keychains later.

## Licensing

Legato's own code is MIT.

Qt is used under the LGPL-3.0: the release bundle ships Qt's shared libraries unmodified beside the
binary, so they can be replaced. Every Qt module linked must be available under the LGPL; check each
new one. Qt Shader Tools is not linked: its `qsb` compiler runs at build time, and the Qt GPL
exception of its license covers the shaders it writes.

mpv is GPL-2.0-or-later by default. On Linux Legato links the system libmpv dynamically, and MIT
code may be combined with it. The Windows installer carries a GPL build of libmpv, so that bundle
meets the GPL: it ships mpv's license texts and names the exact source and build recipe
(`licenses/mpv/` in the install). QtKeychain is BSD-licensed; its license ships beside it.

The app icon's noteheads are the noteheadBlack glyph of the Bravura music font (SIL Open Font
License 1.1); `packaging/icon.py` draws the icon from it.

## Alternatives considered

| Option | Why not |
|---|---|
| Tauri 2 + SvelteKit (WebKitGTK) | Built first and dropped: 60 FPS cap and no blur on the target machine ([decision 0001](decisions/0001-qt-quick.md)). |
| Electron | Bundles Chromium; large, memory-heavy, a security update every few weeks. |
| Qt Quick + Rust via `cxx-qt` | A second build system and a binding layer for little gain; Qt covers HTTP, JSON, D-Bus and threads. |
| GTK4 / libadwaita | Looks like a GNOME app, not like the target design; weak on other platforms. |
| Slint | Less mature for a rich, animated UI. |
| SwiftUI | macOS only. |
| A pure decoder stack (FFmpeg or symphonia with an audio output library) | Streaming, seeking and gapless playback would have to be written from scratch. |
| mpv as a subprocess (JSON IPC) | Same packaging problem as libmpv, plus more glue code. |

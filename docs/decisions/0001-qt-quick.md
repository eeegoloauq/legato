# 0001: Qt Quick and C++ instead of Tauri

Date: 2026-11-02. Status: accepted. The Qt M0 passed on the target machine the same day.

## Context

The M0 rendering test ran the Tauri 2 + SvelteKit build on the target machine: Fedora, GNOME
Wayland, NVIDIA GTX 1660 Ti with the proprietary driver, a 144 Hz monitor, WebKitGTK 2.54.1.

- The default launch failed with `Error 71 (Protocol error) dispatching to Wayland display`.
- The common workaround, `WEBKIT_DISABLE_DMABUF_RENDERER=1`, made the window appear but turned off
  accelerated compositing. WebKit does not draw `backdrop-filter` in that mode, so the glass header
  and player bar showed no blur. Card shadows were then painted on the CPU: about 30 FPS with all
  effects.
- Without effects the app still ran at about 62 FPS. WebKitGTK paces frames with the DRM call
  `drmWaitVBlank`. When that call fails, which it does on this NVIDIA driver, WebKit falls back to a
  timer fixed at 60 FPS (`DisplayVBlankMonitorTimer`). WebKit also enables
  `PreferPageRenderingUpdatesNear60FPS` by default. The upstream bug has been open since 2019:
  https://bugs.webkit.org/show_bug.cgi?id=199376
- 144 FPS was only reached with an `LD_PRELOAD` helper that replaces the failing DRM call with a
  timer. That is not vsync and cannot ship.

M0's pass condition was smooth rendering on the target machine with the stock engine. It failed.

## Options

| | Tauri / WebKitGTK | Electron (Chromium) | Qt Quick + C++ |
|---|---|---|---|
| Rendering on the target machine | 60 FPS cap, blur depends on a workaround | 144 Hz and blur expected | 144 Hz and blur expected |
| Engine on each OS | Three different engines | Bundled, identical | Own renderer, identical |
| Footprint | Small | About 100 MB download, a few hundred MB of RAM | Small package on Linux (system Qt), 40-80 MB bundles elsewhere |
| Security upkeep | Distribution updates WebKit | A Chromium update roughly every eight weeks, forever | Distribution updates Qt on Linux |
| Desktop integration (MPRIS, media controls, libmpv) | Rust crates | Native addons or npm packages | Qt D-Bus, direct libmpv calls, native code on Windows and macOS |
| Dependencies | npm tree and crates | npm tree and Electron | Qt and libmpv |
| UI development speed | Fast | Fastest | Slower |
| Mobile | Weak | None | Android and iOS |

Chromium suits teams that share code with a web player, as Spotify does. Legato has no web player.
A music player stays open all day, so idle memory and CPU matter more than in most apps.

## Decision

Qt Quick (QML) for the interface, C++ for the application layer, libmpv for playback. The Rust core
is dropped: Qt covers HTTP, JSON, hashing, D-Bus and threads, and the native media integrations on
Windows (C++/WinRT) and macOS (Objective-C++) are written in C++-family languages. A Rust core would
add a second build system and a binding layer (`cxx-qt`) to keep two small files.

Costs accepted: the M0 interface is rewritten in QML, UI work is slower than with CSS, C++ has no
lockfile (Qt is pinned by `scripts/install-qt.sh`, libmpv comes from the system), and Qt's LGPL terms must be checked per module.

## Conventions taken from lumeo

lumeo, another Qt Quick and libmpv desktop app by the same author, already settled several choices;
Legato follows them unless it has a reason not to.

- One QML module, built as a static library with everything but `main()`, so tests load the module
  the app loads. The executable is made in `src/`, a directory without QML.
- Qt 6.12.0 from `scripts/install-qt.sh` for development and the bundle. A release bundle carries
  that Qt (Wayland and X11 platform plugins, no GTK theme); libmpv stays the system's. Whether a
  distribution package uses the system Qt instead is decided at packaging (M3).
- Material Symbols icons (Apache 2.0, only those in use, tinted by the button). Legato uses the
  Rounded style in weight 300, closer to Apple's SF Symbols than the default style.
- One wheel component, shared with lumeo (`WheelScroll.qml`): a layer over the view (a handler
  inside a Flickable lands in its contentItem and misses wheels outside its bounds), a 120 px step
  per notch taken from angleDelta when the event has no scroll phase (Wayland sends the
  compositor's 10-15 px as pixelDelta beside it), 1:1 pixels for touchpad gestures, fast notches
  adding up, and Shift with the wheel left to what is under the page.
- CMake presets: `dev` with ccache and mold, `release` without tests.

## Next

A Qt M0 with the same content as the WebKit test: the cover grid with real covers, the glass
header and player bar with real blur, animated generated covers, light and dark themes, a frame
meter, and libmpv playing one song. It must run smoothly at 144 Hz on the target machine with no
environment variables. If it passes, the Tauri and Svelte code is removed and
[architecture](../architecture.md) and [roadmap](../roadmap.md) are rewritten for the new stack.

Done on 2026-11-02: the Qt M0 passed, the Tauri, Svelte and Rust code was removed, and the
architecture and roadmap describe the Qt stack.

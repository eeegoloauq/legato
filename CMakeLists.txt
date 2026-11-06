cmake_minimum_required(VERSION 3.22)
project(legato LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Address, undefined-behaviour and leak checks for the tests (the sanitize preset);
# tests/CMakeLists.txt points them at their suppressions. -g1 keeps the reports' file and line
# without the disk full debug information takes.
option(LEGATO_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(LEGATO_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined
    -fno-omit-frame-pointer -g1)
  add_link_options(-fsanitize=address,undefined)
endif()

# 6.9 brings RectangularShadow, the cheap cover shadow. Development and the bundle use the Qt of
# scripts/install-qt.sh.
find_package(Qt6 6.9 REQUIRED COMPONENTS Quick QuickControls2 Network ShaderTools)
# The system keychain (docs/architecture.md): the distribution's package, like libmpv; on Windows
# one built by packaging/build-qtkeychain.sh.
find_package(Qt6Keychain REQUIRED)
# On Linux, Session asks the session bus whether a Secret Service exists; MPRIS uses it too.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()
# libmpv is the system's on Linux (docs/architecture.md, "Licensing"). Windows has none to find: a
# pinned build is unpacked into MPV_DIR (packaging/fetch-libmpv.sh) and ships in the bundle.
if(WIN32)
  set(MPV_DIR "" CACHE PATH "An unpacked mpv-dev build: libmpv-2.dll, libmpv.dll.a, include/")
  add_library(libmpv SHARED IMPORTED)
  set_target_properties(libmpv PROPERTIES
    IMPORTED_LOCATION ${MPV_DIR}/libmpv-2.dll
    IMPORTED_IMPLIB ${MPV_DIR}/libmpv.dll.a
    INTERFACE_INCLUDE_DIRECTORIES ${MPV_DIR}/include)
  set(mpv libmpv)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
  set(mpv PkgConfig::MPV)
endif()

qt_standard_project_setup(REQUIRES 6.9)

set_source_files_properties(qml/Theme.qml qml/Navigation.qml qml/Prefs.qml qml/HomeFeed.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Everything but main(), so the tests load the module the app does.
qt_add_library(legato STATIC)
qt_add_qml_module(legato
  URI Legato
  # Where tooling such as qmllint looks for a module named Legato.
  OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Legato
  QML_FILES
    qml/Main.qml
    qml/AlbumCard.qml
    qml/AlbumMenu.qml
    qml/AlbumPage.qml
    qml/AlbumsPage.qml
    qml/ArtistCard.qml
    qml/ArtistLinks.qml
    qml/ArtistMenu.qml
    qml/ArtistPage.qml
    qml/ArtistsPage.qml
    qml/AskDialog.qml
    qml/BarSlider.qml
    qml/Carousel.qml
    qml/CompactSongRow.qml
    qml/ContentPage.qml
    qml/CoverButton.qml
    qml/CoverGrid.qml
    qml/Dropdown.qml
    qml/FavoritesPage.qml
    qml/FrameMeterPanel.qml
    qml/FullScreenPlayer.qml
    qml/GenCover.qml
    qml/GlassPanel.qml
    qml/Glyph.qml
    qml/HomeFeed.qml
    qml/HomePage.qml
    qml/IconButton.qml
    qml/InfoCard.qml
    qml/LevelMeter.qml
    qml/LyricsView.qml
    qml/Marquee.qml
    qml/MixingLabel.qml
    qml/MenuDivider.qml
    qml/Notice.qml
    qml/Navigation.qml
    qml/PageHeading.qml
    qml/PageHero.qml
    qml/PageTint.qml
    qml/PinAction.qml
    qml/PlayerBar.qml
    qml/PlaylistCard.qml
    qml/PlaylistCover.qml
    qml/PlaylistMenu.qml
    qml/PlaylistPage.qml
    qml/PlaylistSubmenu.qml
    qml/PlaylistsPage.qml
    qml/PopupMenu.qml
    qml/Prefs.qml
    qml/PushButton.qml
    qml/QualityBadge.qml
    qml/QueueRow.qml
    qml/ResizeEdges.qml
    qml/RoundedImage.qml
    qml/Scroller.qml
    qml/SearchField.qml
    qml/SectionTitle.qml
    qml/SettingsPage.qml
    qml/Shadow.qml
    qml/SidePanel.qml
    qml/Sidebar.qml
    qml/SidebarItem.qml
    qml/SignInView.qml
    qml/SongListView.qml
    qml/SongByline.qml
    qml/SongMenu.qml
    qml/SongRow.qml
    qml/SongsPage.qml
    qml/StatusDot.qml
    qml/StoredSetting.qml
    qml/Theme.qml
    qml/ToggleSwitch.qml
    qml/TopBar.qml
    qml/UnreachableView.qml
    qml/UpNextList.qml
    qml/WheelScroll.qml
    qml/WindowControls.qml
    qml/WindowDragArea.qml
  RESOURCES
    # Material Symbols Rounded, Apache 2.0 (icons/LICENSE): only the ones in use.
    icons/add.svg
    icons/artist.svg
    icons/chevron_left.svg
    icons/chevron_right.svg
    icons/close.svg
    icons/contrast.svg
    icons/format_list_bulleted.svg
    icons/crop_square.svg
    icons/dock_to_right.svg
    icons/expand_more.svg
    icons/grid_view.svg
    icons/home.svg
    icons/info.svg
    icons/lyrics.svg
    icons/library_music.svg
    icons/more_horiz.svg
    icons/music_note.svg
    icons/open_in_full.svg
    icons/pause.svg
    icons/play_arrow.svg
    icons/radio.svg
    icons/refresh.svg
    icons/remove.svg
    icons/repeat.svg
    icons/repeat_one.svg
    icons/schedule.svg
    icons/search.svg
    icons/settings.svg
    icons/shuffle.svg
    icons/skip_next.svg
    icons/skip_previous.svg
    icons/star.svg
    icons/star_filled.svg
    icons/volume_down.svg
    icons/volume_off.svg
    icons/volume_up.svg
  SOURCES
    src/core/subsonic.h src/core/subsonic.cpp
    src/core/coverstore.h src/core/coverstore.cpp
    src/core/diskcache.h src/core/diskcache.cpp
    src/core/palette.h src/core/palette.cpp
    src/core/crossfade.h src/core/crossfade.cpp
    src/core/automix.h src/core/automix.cpp
    src/core/instantmix.h src/core/instantmix.cpp
    src/core/playqueue.h src/core/playqueue.cpp
    src/core/scrobbletracker.h src/core/scrobbletracker.cpp
    src/core/serverlist.h src/core/serverlist.cpp
    src/core/lrclib.h src/core/lrclib.cpp
    src/core/iteminfo.h src/core/iteminfo.cpp
    src/app/covercolors.h src/app/covercolors.cpp
    src/app/coverprovider.h src/app/coverprovider.cpp
    src/app/framemeter.h src/app/framemeter.cpp
    src/app/library.h src/app/library.cpp
    src/app/logfile.h src/app/logfile.cpp
    src/app/lyricsfinder.h src/app/lyricsfinder.cpp
    src/app/materialowner.h src/app/materialowner.cpp
    src/app/mixanalyzer.h src/app/mixanalyzer.cpp
    src/app/models.h src/app/models.cpp
    src/app/mpvoptions.h src/app/mpvoptions.cpp
    src/app/pages.h src/app/pages.cpp
    src/app/player.h src/app/player.cpp
    src/app/queuesync.h src/app/queuesync.cpp
    src/app/session.h src/app/session.cpp
)

# The generated type registrations include the headers by their bare names.
target_include_directories(legato PUBLIC src/core src/app)
target_link_libraries(legato PUBLIC Qt6::Quick Qt6::QuickControls2 Qt6::Network ${mpv} Qt6Keychain::Qt6Keychain)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(legato PUBLIC Qt6::DBus)
  # MPRIS goes over the session bus (docs/architecture.md, "Desktop integration").
  target_sources(legato PRIVATE src/app/mpris.h src/app/mpris.cpp)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(legato PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Rounded covers and the drifting blobs, compiled for every graphics API Qt might run on.
qt_add_shaders(legato "shaders"
  PREFIX /qt/qml/Legato
  FILES
    shaders/rounded.frag
    shaders/blobs.frag
    shaders/fade.frag
    shaders/shimmer.frag
    shaders/tint.frag
)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

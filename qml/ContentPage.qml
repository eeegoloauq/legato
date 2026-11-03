import QtQuick
import Legato

// A page of the content area (docs/design.md, "Navigation"). Main shows pages in a StackView with
// one history for back and forward; the header reads `title` and `flickable` for its scrolled look,
// and the bars blur whatever the page draws.
FocusScope {
    // Where the page is in the app, as the history stores it: { page: "home" }, later with an id.
    property var route: ({ page: "" })
    property string title
    property Flickable flickable: null
    // The window's background while the page shows (an album's color); transparent for the plain one.
    property color backdrop: "transparent"
    // Search (docs/design.md, "Search"): the songs of an album, a playlist or Favorites, which the
    // header's search lists first, and "Playing from" for them. The page's song list sets these.
    property SongModel localSongs: null
    property string localFrom
}

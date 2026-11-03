import QtQuick
import Legato

// A page of the content area (docs/design.md, "Navigation"). Main shows pages in a StackView with
// one history for back and forward; the header reads `title` and `flickable` for its scrolled look,
// and the bars blur whatever the page draws.
FocusScope {
    id: page

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

    // How far the view is scrolled from its top, which the history keeps for a page it lets go.
    function offset(): real {
        return flickable ? flickable.contentY - flickable.originY : 0
    }

    // A page made again from the history scrolls back to where it was left. Its rows arrive from
    // the server after it is made, so the view follows the content as it grows until it gets
    // there, until the viewer scrolls, or for a few seconds at most.
    function restore(to: real) {
        wanted = to
        Qt.callLater(follow)
    }

    property real wanted: -1
    property real placed: 0
    property bool placing: false

    function follow() {
        if (wanted <= 0 || !flickable || flickable.height <= 0)
            return
        const view = flickable
        const last = Math.max(0, view.contentHeight + view.bottomMargin - view.height)
        placed = Math.min(wanted, last)
        placing = true
        view.contentY = view.originY + placed
        placing = false
        if (last >= wanted)
            wanted = -1
        else
            giveUp.restart()
    }

    Connections {
        target: page.wanted > 0 ? page.flickable : null

        // After the view is done with the change: a model reset moves it to its start once it
        // has measured the new rows.
        function onContentHeightChanged() {
            Qt.callLater(page.follow)
        }

        function onHeightChanged() {
            Qt.callLater(page.follow)
        }

        // The viewer moved the page (a wheel, a flick, a key): that place wins. A header that
        // grows moves contentY and originY together, which leaves the offset as it was.
        function onContentYChanged() {
            if (!page.placing && Math.abs(page.offset() - page.placed) > 1)
                page.wanted = -1
        }
    }

    Timer {
        id: giveUp

        interval: 5000
        onTriggered: page.wanted = -1
    }
}

pragma Singleton

import QtQuick
import Legato

// Home's rows (docs/design.md, "Home"), kept for the session rather than by each Home page: Home
// opens again at once, as it was left, without asking the server. A row is asked again only when
// it may have changed, and only while Home is off screen, so nothing moves while it shows.
QtObject {
    id: feed

    // A Home page is on screen. Main sets it.
    property bool showing: false
    // Every row has had its first answer. Home shows whole, never row by row: a row that came in
    // late pushed the others down (the playlists showed first, then dropped to the bottom).
    property bool ready: false
    readonly property bool loaded: !played.loading && !recent.loading && !random.loading && !frequent.loading

    readonly property AlbumList played: AlbumList {
        type: "recent"
        limit: 15
    }
    readonly property AlbumList recent: AlbumList {
        type: "newest"
        limit: 15
    }
    // Asked again only with its refresh button.
    readonly property AlbumList random: AlbumList {
        type: "random"
        limit: 15
    }
    readonly property AlbumList frequent: AlbumList {
        type: "frequent"
        limit: 15
    }

    // Listens were recorded while Home showed: asked again once it is left.
    property bool playsChanged: false
    // When Recently added was last asked. New albums come from a scan of the server, which says
    // nothing about it, so it is asked again when Home is left, at most every ten minutes.
    property double addedAt: Date.now()
    readonly property int addedMs: 10 * 60 * 1000

    function refreshPlays() {
        playsChanged = false
        played.reload()
        frequent.reload()
    }

    onLoadedChanged: if (loaded) ready = true
    onShowingChanged: {
        if (showing)
            return
        if (playsChanged)
            refreshPlays()
        if (Date.now() - addedAt > addedMs) {
            addedAt = Date.now()
            recent.reload()
        }
    }

    readonly property Connections library: Connections {
        target: Library

        function onListensSent() {
            if (feed.showing)
                feed.playsChanged = true
            else
                feed.refreshPlays()
        }

        // Another server or account: Home waits for all of its rows again. Checked once the rows
        // have started asking, which they do on the same signal.
        function onServerChanged() {
            feed.ready = false
            Qt.callLater(() => {
                if (feed.loaded)
                    feed.ready = true
            })
        }
    }
}

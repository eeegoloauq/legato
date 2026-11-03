pragma Singleton

import QtQuick

// Opens pages and Get info from anywhere: cards, rows and menus call these, and Main shows the page
// on the one history (docs/design.md, "Navigation") or the info card.
QtObject {
    signal requested(var route)
    // Get info (docs/design.md, "Get info"): kind is "song" or "album".
    signal infoRequested(string kind, string id)
    // A card asking for a name (New Playlist, Rename): done(name) runs once it is confirmed.
    signal nameRequested(string title, string action, string initial, var done)
    // A card confirming something that cannot be undone: done() runs once it is confirmed.
    signal confirmRequested(string title, string detail, string action, var done)

    function open(route) {
        requested(route)
    }

    function album(id) {
        if (id)
            requested({ page: "album", id: id })
    }

    function artist(id) {
        if (id)
            requested({ page: "artist", id: id })
    }

    function playlist(id) {
        if (id)
            requested({ page: "playlist", id: id })
    }

    function info(kind, id) {
        if (id)
            infoRequested(kind, id)
    }

    function askName(title, action, initial, done) {
        nameRequested(title, action, initial ?? "", done)
    }

    function confirm(title, detail, action, done) {
        confirmRequested(title, detail, action, done)
    }
}

import QtQuick
import QtQuick.Controls.Basic
import Legato

// An album's "…" menu (docs/design.md, "Menus").
PopupMenu {
    id: menu

    property string albumId
    property string name
    property string artist
    property string artistId
    property bool starred
    property string coverArt
    property int hue

    Action {
        text: qsTr("Play Next")
        onTriggered: Library.queueAlbum(menu.albumId, true)
    }
    Action {
        text: qsTr("Add to Queue")
        onTriggered: Library.queueAlbum(menu.albumId, false)
    }
    PlaylistSubmenu {
        kind: "album"
        itemId: menu.albumId
        suggestion: menu.name
    }
    // The album artist's mix: an album has no mix of its own on the server.
    Action {
        text: qsTr("Instant Mix")
        enabled: menu.artistId !== ""
        onTriggered: Library.instantMix(menu.artistId, menu.artist)
    }

    MenuDivider {}

    Action {
        text: qsTr("Go to Artist")
        enabled: menu.artistId !== ""
        onTriggered: Navigation.artist(menu.artistId)
    }
    Action {
        text: menu.starred ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
        onTriggered: Library.setAlbumStarred(menu.albumId, !menu.starred)
    }
    PinAction {
        kind: "album"
        itemId: menu.albumId
        name: menu.name
        coverArt: menu.coverArt
        hue: menu.hue
    }
    Action {
        text: qsTr("Get Info")
        onTriggered: Navigation.info("album", menu.albumId)
    }
    Action {
        text: qsTr("Search on Spotify")
        onTriggered: Qt.openUrlExternally("https://open.spotify.com/search/"
                                          + encodeURIComponent(menu.artist + " " + menu.name))
    }
}

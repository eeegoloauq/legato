import QtQuick
import QtQuick.Controls.Basic
import Legato

// A playlist's "…" menu (docs/design.md, "Menus"): on the playlist page, and on a right click on a
// playlist card or a playlist in the sidebar.
PopupMenu {
    id: menu

    property string playlistId
    property string name
    property string coverArt
    property int hue

    Action {
        text: qsTr("Play")
        onTriggered: Library.playPlaylist(menu.playlistId)
    }
    Action {
        text: qsTr("Shuffle")
        onTriggered: Library.playPlaylist(menu.playlistId, true)
    }

    MenuDivider {}

    PinAction {
        kind: "playlist"
        itemId: menu.playlistId
        name: menu.name
        coverArt: menu.coverArt
        hue: menu.hue
    }
}

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
    // The user's own: Rename and Delete.
    property bool editable: false

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

    readonly property MenuDivider editDivider: MenuDivider {}
    readonly property Action renameAction: Action {
        text: qsTr("Rename…")
        onTriggered: {
            const id = menu.playlistId
            Navigation.askName(qsTr("Rename Playlist"), qsTr("Rename"), menu.name,
                               name => Library.renamePlaylist(id, name))
        }
    }
    readonly property Action deleteAction: Action {
        text: qsTr("Delete Playlist…")
        onTriggered: {
            const id = menu.playlistId
            Navigation.confirm(qsTr("Delete “%1”?").arg(menu.name),
                               qsTr("It goes from your server for every app. This cannot be undone."),
                               qsTr("Delete"), () => Library.deletePlaylist(id))
        }
    }

    // Rename and Delete come and go with `editable`: the sidebar's one menu serves every playlist.
    onEditableChanged: placeEditActions()
    Component.onCompleted: placeEditActions()

    property bool editShown: false

    function placeEditActions() {
        if (editable === editShown)
            return
        editShown = editable
        if (editable) {
            addItem(editDivider)
            addAction(renameAction)
            addAction(deleteAction)
        } else {
            removeAction(deleteAction)
            removeAction(renameAction)
            removeItem(editDivider)
        }
    }
}

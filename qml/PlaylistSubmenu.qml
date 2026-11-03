pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Legato

// "Add to Playlist" in a song's or an album's menu: New Playlist…, then the user's own playlists.
// Others' public playlists are left out: the server would refuse.
PopupMenu {
    id: submenu

    // "song" or "album", and its id.
    required property string kind
    required property string itemId
    // Suggested for New Playlist: the song's title or the album's name.
    property string suggestion

    title: qsTr("Add to Playlist")
    enabled: itemId !== ""

    Action {
        text: qsTr("New Playlist…")
        // The menu is gone by the time the name is in (a row's menu is made for one opening).
        onTriggered: {
            const kind = submenu.kind
            const id = submenu.itemId
            Navigation.askName(qsTr("New Playlist"), qsTr("Create"), submenu.suggestion,
                               name => Library.newPlaylist(name, kind, id))
        }
    }

    MenuDivider {
        visible: submenu.own > 0
        height: visible ? implicitHeight : 0
    }

    component PlaylistAction: Action {
        required property string playlistId
        required property string name
        required property bool editable

        text: name
        onTriggered: Library.addToPlaylist(playlistId, submenu.kind, submenu.itemId)
    }

    // The user's playlists, in the server's order. A reload of the list removes them all and adds
    // them again in order, so appending keeps that order.
    property int own: 0

    Instantiator {
        model: Library.playlists
        delegate: PlaylistAction {}
        onObjectAdded: (index, object) => {
            const action = object as PlaylistAction
            if (!action.editable)
                return
            submenu.addAction(action)
            ++submenu.own
        }
        onObjectRemoved: (index, object) => {
            const action = object as PlaylistAction
            if (!action.editable)
                return
            submenu.removeAction(action)
            --submenu.own
        }
    }
}

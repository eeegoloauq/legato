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
    property var membership: Object.create(null)
    property int membershipRequest: 0

    title: qsTr("Add to Playlist")
    enabled: itemId !== ""

    function refreshMembership() {
        const request = ++membershipRequest
        membership = Object.create(null)
        Library.loadPlaylistMembership(kind, itemId, submenu, (playlistId, included) => {
            if (request === submenu.membershipRequest)
                submenu.membership = Object.assign(Object.create(null), submenu.membership, { [playlistId]: included })
        })
    }

    onAboutToShow: refreshMembership()
    onAboutToHide: ++membershipRequest
    onItemIdChanged: if (visible) refreshMembership()

    Connections {
        target: Library
        enabled: submenu.visible
        function onPlaylistChanged() { submenu.refreshMembership() }
    }

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
        enabled: submenu.membership[playlistId] !== undefined
        checkable: true
        checked: submenu.membership[playlistId] === true
        onTriggered: Library.setPlaylistMembership(playlistId, submenu.kind, submenu.itemId,
                                                    submenu.membership[playlistId] === false)
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

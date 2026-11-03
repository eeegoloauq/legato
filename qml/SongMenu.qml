import QtQuick
import QtQuick.Controls.Basic
import Legato

// A song's menu (docs/design.md, "Menus"): from "…", a right click or Shift+F10 on a row, or a right
// click on an Up Next row. Add to Playlist comes with its feature.
PopupMenu {
    id: menu

    property SongModel songs
    property int row: -1
    // `songs` is Player.upNext: Remove from Up Next comes first.
    property bool upNext: false
    property string songId
    property string songTitle
    property string artist
    property string albumId
    property string artistId
    property bool starred

    readonly property Action removeAction: Action {
        text: qsTr("Remove from Up Next")
        onTriggered: Player.removeAt(Player.queueIndex + 1 + menu.row)
    }
    readonly property MenuDivider removeDivider: MenuDivider {}

    Component.onCompleted: {
        if (!upNext)
            return
        insertAction(0, removeAction)
        insertItem(1, removeDivider)
    }

    Action {
        text: qsTr("Play Next")
        onTriggered: Library.queueFrom(menu.songs, menu.row, true)
    }
    Action {
        text: qsTr("Add to Queue")
        onTriggered: Library.queueFrom(menu.songs, menu.row, false)
    }
    Action {
        text: qsTr("Instant Mix")
        enabled: menu.artistId !== ""
        onTriggered: Library.instantMixFrom(menu.songs, menu.row)
    }

    MenuDivider {}

    Action {
        text: qsTr("Go to Album")
        enabled: menu.albumId !== ""
        onTriggered: Navigation.album(menu.albumId)
    }
    Action {
        text: qsTr("Go to Artist")
        enabled: menu.artistId !== ""
        onTriggered: Navigation.artist(menu.artistId)
    }

    MenuDivider {}

    Action {
        text: menu.starred ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
        onTriggered: Library.setSongStarred(menu.songId, !menu.starred)
    }
    Action {
        text: qsTr("Get Info")
        onTriggered: Navigation.info("song", menu.songId)
    }
    Action {
        text: qsTr("Search on Spotify")
        onTriggered: Qt.openUrlExternally("https://open.spotify.com/search/"
                                          + encodeURIComponent(menu.artist + " " + menu.songTitle))
    }
}

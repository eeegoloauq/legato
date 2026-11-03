import QtQuick
import QtQuick.Controls.Basic
import Legato

// An artist's "…" menu (docs/design.md, "Menus"), on the artist page and on a right click on an
// artist card. Get info comes with its feature.
PopupMenu {
    id: menu

    property string artistId
    property string name
    property string coverArt
    property int hue

    Action {
        text: qsTr("Instant Mix")
        onTriggered: Library.instantMix(menu.artistId, menu.name)
    }

    MenuDivider {}

    PinAction {
        kind: "artist"
        itemId: menu.artistId
        name: menu.name
        coverArt: menu.coverArt
        hue: menu.hue
    }
    Action {
        text: qsTr("Search on Spotify")
        onTriggered: Qt.openUrlExternally("https://open.spotify.com/search/" + encodeURIComponent(menu.name))
    }
}

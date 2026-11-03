pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import Legato

// A playlist in a grid or a carousel: its cover, name and song count. A click opens it; the play
// button under the pointer plays it.
Item {
    id: card

    required property string playlistId
    required property string name
    required property string coverArt
    required property int songCount
    required property int hue
    required property bool editable
    property real coverSize: 180
    property bool keyFocus: false

    width: GridView.view ? GridView.view.cellWidth : coverSize
    height: GridView.view ? GridView.view.cellHeight : coverSize + 66

    HoverHandler {
        id: hover
    }

    Item {
        id: media

        width: card.coverSize
        height: card.coverSize

        RectangularShadow {
            anchors.fill: parent
            visible: Theme.shadowsOn
            radius: Theme.radiusArt
            offset.y: 6
            blur: 18
            color: Theme.shadow
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            visible: card.keyFocus
            radius: Theme.radiusArt + 4
            color: "transparent"
            border.width: 3
            border.color: Theme.accent
        }

        PlaylistCover {
            anchors.fill: parent
            coverArt: card.coverArt
            name: card.name
            hue: card.hue
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            onTapped: Navigation.playlist(card.playlistId)
        }

        // A right click opens the playlist's menu at the pointer. Made when asked for.
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: eventPoint => {
            menu.at = card.mapFromItem(null, eventPoint.scenePosition)
            menu.active = true
        }
        }

        Loader {
            id: menu

            // The right click's point in the card.
            property point at

            active: false
            onLoaded: (item as PlaylistMenu).openAt(card, at.x, at.y)
            sourceComponent: PlaylistMenu {
                playlistId: card.playlistId
                name: card.name
                coverArt: card.coverArt
                hue: card.hue
                editable: card.editable
                onClosed: menu.active = false
            }
        }

        CoverButton {
            x: 8
            y: parent.height - height - 8
            glyph: "play_arrow"
            shown: (hover.hovered || card.keyFocus) && card.songCount > 0
            Accessible.name: qsTr("Play %1").arg(card.name)
            onClicked: Library.playPlaylist(card.playlistId)
        }
    }

    Column {
        anchors.top: media.bottom
        anchors.topMargin: 8
        width: card.coverSize

        Text {
            width: parent.width
            text: card.name
            color: Theme.text
            font.pixelSize: Theme.fs
            font.weight: Font.Medium
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Text {
            text: card.songCount === 1 ? qsTr("1 song") : qsTr("%1 songs").arg(card.songCount)
            color: Theme.text2
            font.pixelSize: Theme.fsSmall
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import Legato

// An artist in a grid: a round photo, the name under it and how many albums. A click opens
// the artist.
Item {
    id: card

    required property string artistId
    required property string name
    required property string coverArt
    required property int albumCount
    required property int hue
    property real coverSize: 180
    property bool keyFocus: false

    width: GridView.view ? GridView.view.cellWidth : coverSize
    height: GridView.view ? GridView.view.cellHeight : coverSize + 50

    Item {
        id: media

        width: card.coverSize
        height: card.coverSize

        Shadow {
            anchors.fill: parent
            visible: Theme.shadowsOn
            radius: width / 2
            offset.y: 6
            blur: 18
            color: Theme.shadow
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            visible: card.keyFocus
            radius: width / 2
            color: "transparent"
            border.width: 3
            border.color: Theme.accent
        }

        RoundedImage {
            anchors.fill: parent
            radius: width / 2
            coverArt: card.coverArt
        }
    }

    HoverHandler {
        id: hover

        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: Navigation.artist(card.artistId)
    }

    // A right click opens the artist's menu at the pointer. Made when asked for, as an album card's.
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
        onLoaded: (item as ArtistMenu).openAt(card, at.x, at.y)
        sourceComponent: ArtistMenu {
            artistId: card.artistId
            name: card.name
            coverArt: card.coverArt
            hue: card.hue
            onClosed: menu.active = false
        }
    }

    Column {
        anchors.top: media.bottom
        anchors.topMargin: 10
        width: card.coverSize

        Text {
            width: parent.width
            text: card.name
            color: Theme.text
            font.pixelSize: Theme.fs
            font.weight: Font.Medium
            font.underline: hover.hovered
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: card.albumCount > 0
            text: card.albumCount === 1 ? qsTr("1 album") : qsTr("%1 albums").arg(card.albumCount)
            color: Theme.text2
            font.pixelSize: Theme.fsSmall
            horizontalAlignment: Text.AlignHCenter
        }
    }
}

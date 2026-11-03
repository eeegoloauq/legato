import QtQuick
import QtQuick.Controls.Basic
import Legato

// A song in the Up Next panel: cover, title and artist. A click plays it; the × that shows on
// hover removes it, in place of the song's length. A right click asks for its menu (menuRequested,
// with the pointer in the row).
Item {
    id: row

    property bool current: false
    property string title
    property string artist
    property string coverArt
    property real duration
    // The full-screen player sets these: it is dark in either theme.
    property color textColor: Theme.text
    property color dimColor: Theme.text2
    property color hoverFill: Theme.fill

    signal play()
    signal remove()
    signal menuRequested(real x, real y)

    implicitHeight: 52

    HoverHandler {
        id: hover
    }

    TapHandler {
        enabled: !row.current
        onTapped: row.play()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => row.menuRequested(eventPoint.position.x, eventPoint.position.y)
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius - 2
        color: row.current || hover.hovered ? row.hoverFill : "transparent"
    }

    RoundedImage {
        id: cover

        x: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 40
        height: 40
        radius: 5
        coverArt: row.coverArt
    }

    Column {
        anchors.left: cover.right
        anchors.leftMargin: 10
        anchors.right: trailing.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter

        Text {
            width: parent.width
            text: row.title
            color: row.current ? Theme.tint : row.textColor
            font.pixelSize: Theme.fs
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: row.artist
            color: row.dimColor
            font.pixelSize: Theme.fsSmall
            elide: Text.ElideRight
        }
    }

    Item {
        id: trailing

        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(length.implicitWidth, 26)
        height: 26

        Text {
            id: length

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: !removeButton.visible
            text: Math.floor(row.duration / 60) + ":" + String(Math.floor(row.duration % 60)).padStart(2, "0")
            color: row.dimColor
            font.pixelSize: Theme.fsSmall
            font.features: { "tnum": 1 }
        }

        IconButton {
            id: removeButton

            anchors.right: parent.right
            visible: !row.current && (hover.hovered || visualFocus)
            diameter: 26
            glyph: "close"
            glyphSize: 16
            tone: row.dimColor
            tip: qsTr("Remove from Up Next")
            onClicked: row.remove()
        }
    }
}

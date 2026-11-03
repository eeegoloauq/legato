import QtQuick
import QtQuick.Controls.Basic
import Legato

// A row of the sidebar. Rows whose page is not in this build are dimmed and say so on hover. A right
// click asks for the row's menu (menuRequested, with the pointer in window coordinates); the sidebar
// decides what is in it.
Item {
    id: row

    property string glyph
    // A playlist row shows its cover instead of a glyph.
    property bool cover: false
    property string coverArt
    property int hue
    // An artist's photo is round.
    property bool round: false
    property string text
    property bool current: false
    property bool available: true

    signal clicked()
    signal menuRequested(point at)

    implicitHeight: 32

    ItemDelegate {
        id: delegate

        anchors.fill: parent
        enabled: row.available
        text: row.text
        leftPadding: 10
        font.pixelSize: Theme.fs
        font.weight: row.current ? Font.DemiBold : Font.Normal
        Accessible.name: row.text
        onClicked: row.clicked()

        contentItem: Row {
            spacing: 10

            Glyph {
                anchors.verticalCenter: parent.verticalCenter
                visible: !row.cover
                name: row.glyph || "music_note"
                size: 20
                color: row.current ? Theme.text : row.available ? Theme.text2 : Theme.text3
            }

            PlaylistCover {
                anchors.verticalCenter: parent.verticalCenter
                visible: row.cover
                width: 20
                height: 20
                radius: row.round ? 10 : 4
                coverArt: row.coverArt
                hue: row.hue
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: delegate.availableWidth - 30
                text: delegate.text
                font: delegate.font
                color: row.available ? Theme.text : Theme.text3
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: row.current ? Theme.fill2 : delegate.down ? Theme.fill2 : delegate.hovered ? Theme.fill : "transparent"
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => row.menuRequested(row.mapToItem(null, eventPoint.position))
    }

    HoverHandler {
        id: hover
        enabled: !row.available
    }

    ToolTip {
        visible: hover.hovered
        delay: 400
        text: qsTr("Not built yet")
    }
}

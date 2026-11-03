pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Legato

// The right panel (docs/design.md, "Up Next panel" and "Lyrics"): a Queue | Lyrics switch, then
// the queue or the current song's lyrics.
Rectangle {
    id: panel

    // "queue" or "lyrics".
    property string page: "queue"

    signal closeRequested()
    signal pageRequested(string page)

    // Opaque, as it may lie over the content, in the window's backdrop color.
    color: Theme.backdrop

    // While it slides in, the panel lies over the page: the wheel and hover stop here. (In place it
    // sits beside the page, so nothing is under it; see Main.)
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    Rectangle {
        width: 1
        height: parent.height
        color: Theme.line
    }

    // The panel covers the right end of the header, so it carries the window buttons and the drag
    // strip there.
    Item {
        id: strip

        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.headerHeight

        DragHandler {
            target: null
            onActiveChanged: if (active) panel.Window.window.startSystemMove()
        }

        WindowControls {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // The switch and the close button.
    Item {
        id: head

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: strip.bottom
        height: 44

        Rectangle {
            id: segments

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: close.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            height: 32
            radius: 9
            color: Theme.fill

            Row {
                anchors.fill: parent
                anchors.margins: 2

                Repeater {
                    model: [{ key: "queue", label: qsTr("Up Next") }, { key: "lyrics", label: qsTr("Lyrics") }]

                    delegate: T.Button {
                        id: segment

                        required property var modelData
                        readonly property bool selected: panel.page === modelData.key

                        width: (segments.width - 4) / 2
                        height: 28
                        focusPolicy: Qt.TabFocus
                        onClicked: panel.pageRequested(modelData.key)

                        background: Rectangle {
                            radius: 7
                            color: segment.selected ? Theme.raised : "transparent"
                            border.width: segment.visualFocus ? 2 : 0
                            border.color: Theme.accent
                        }

                        contentItem: Text {
                            text: segment.modelData.label
                            color: segment.selected ? Theme.text : Theme.text2
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }

        IconButton {
            id: close

            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            glyph: "close"
            tone: Theme.text2
            tip: qsTr("Close")
            onClicked: panel.closeRequested()
        }
    }

    // ---- Up Next

    UpNextList {
        id: queue

        objectName: "queueList"
        anchors.fill: parent
        anchors.topMargin: head.y + head.height + 4
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        visible: panel.page === "queue"
    }

    WheelScroll {
        flickable: queue
    }

    // ---- Lyrics

    LyricsView {
        id: lyrics

        objectName: "lyricsView"
        anchors.fill: parent
        anchors.topMargin: head.y + head.height
        anchors.leftMargin: 18
        anchors.rightMargin: 14
        visible: panel.page === "lyrics"
    }

    Column {
        anchors.centerIn: lyrics
        width: lyrics.width - 20
        spacing: 6
        visible: panel.page === "lyrics" && !Player.lyricsLoading && lyrics.empty

        Text {
            width: parent.width
            text: Player.hasSong ? qsTr("No lyrics for this song") : qsTr("Nothing playing")
            color: Theme.text
            font.pixelSize: Theme.fs
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width
            visible: Player.hasSong
            text: Prefs.lrclib ? qsTr("Neither your server nor LRCLIB has them.")
                               : qsTr("Legato shows the lyrics your server has: embedded in the file or in an .lrc file beside it. Settings can also find missing lyrics on LRCLIB.")
            color: Theme.text2
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
}

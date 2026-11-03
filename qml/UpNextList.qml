pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Legato

// The queue (docs/design.md, "Up Next panel"): where it plays from, Clear, then the songs after the
// current one. A click plays a song; × removes it; a right click opens the song menu. The side panel shows the current song above it;
// the full-screen player, which shows that song itself, does not. Colors are settable, as the
// full-screen player is dark in either theme. Put a WheelScroll beside it.
ListView {
    id: queue

    // "Now Playing" and the current song at the top.
    property bool nowPlaying: true
    property color textColor: Theme.text
    property color dimColor: Theme.text2
    property color hoverFill: Theme.fill
    property color clearColor: Theme.red
    property real titleSize: Theme.fs

    readonly property int first: Player.queueIndex + 1
    // Rows leave the top as songs change, and a view keeps its offset: one that was at its top
    // goes back there, with Now Playing in view. A click on a song shows the queue from the top.
    property bool keepTop: false

    function rowsChanging() {
        keepTop = keepTop || atYBeginning
    }

    function rowsChanged() {
        if (keepTop)
            positionViewAtBeginning()
        keepTop = false
    }

    Connections {
        target: Player.upNext

        function onRowsAboutToBeRemoved() { queue.rowsChanging() }
        function onRowsAboutToBeInserted() { queue.rowsChanging() }
        function onRowsRemoved() { queue.rowsChanged() }
        function onRowsInserted() { queue.rowsChanged() }
    }

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    bottomMargin: 16
    model: Player.upNext

    header: Column {
        width: queue.width

        Text {
            visible: queue.nowPlaying
            text: qsTr("Now Playing")
            color: queue.textColor
            font.pixelSize: queue.titleSize
            font.weight: Font.DemiBold
            leftPadding: 6
            topPadding: 8
            bottomPadding: 6
        }

        QueueRow {
            width: parent.width
            visible: queue.nowPlaying && Player.hasSong
            current: true
            title: Player.title
            artist: Player.artist
            coverArt: Player.coverArt
            duration: Player.duration
            textColor: queue.textColor
            dimColor: queue.dimColor
            hoverFill: queue.hoverFill
        }

        Text {
            width: parent.width
            visible: queue.nowPlaying && Player.playingFrom !== ""
            text: qsTr("Playing from %1").arg(Player.playingFrom)
            color: queue.dimColor
            font.pixelSize: Theme.fsSmall
            elide: Text.ElideRight
            leftPadding: 6
            topPadding: 4
        }

        Text {
            width: parent.width
            visible: !Player.hasSong
            text: qsTr("Nothing playing")
            color: queue.dimColor
            horizontalAlignment: Text.AlignHCenter
            topPadding: 40
        }

        Item {
            width: parent.width
            height: queue.nowPlaying ? 40 : upNext.implicitHeight + 14
            visible: Player.hasSong

            Text {
                id: upNext

                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 6
                text: qsTr("Up Next")
                color: queue.textColor
                font.pixelSize: queue.titleSize
                font.weight: Font.DemiBold
            }

            T.Button {
                id: clear

                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.baseline: upNext.baseline
                visible: queue.count > 0
                focusPolicy: Qt.TabFocus
                implicitWidth: clearText.implicitWidth
                implicitHeight: clearText.implicitHeight + 4
                onClicked: Player.clearUpNext()

                contentItem: Text {
                    id: clearText

                    text: qsTr("Clear")
                    color: queue.clearColor
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    font.underline: clear.hovered || clear.visualFocus
                }
            }
        }

        // Without Now Playing above, where the queue plays from goes under its title.
        Text {
            width: parent.width
            visible: !queue.nowPlaying && Player.playingFrom !== ""
            text: qsTr("Playing from %1").arg(Player.playingFrom)
            color: queue.dimColor
            font.pixelSize: Theme.fsSmall
            elide: Text.ElideRight
            leftPadding: 6
            bottomPadding: 8
        }

        Text {
            width: parent.width
            visible: Player.hasSong && queue.count === 0
            text: Player.repeat === 1 ? qsTr("The queue starts over after this song.")
                                      : qsTr("The queue ends after this song.")
            color: queue.dimColor
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            topPadding: 24
        }
    }

    delegate: QueueRow {
        id: queueRow

        required property int index
        required property string songId
        required property string albumId
        required property string artistId
        required property bool starred
        required title
        required artist
        required coverArt
        required duration

        width: queue.width
        textColor: queue.textColor
        dimColor: queue.dimColor
        hoverFill: queue.hoverFill
        onPlay: {
            queue.keepTop = true
            Player.playAt(queue.first + index)
        }
        onRemove: Player.removeAt(queue.first + index)
        onMenuRequested: (x, y) => {
            menu.active = true
            const popup = menu.item as SongMenu
            popup.openAt(queueRow, x, y)
        }

        Loader {
            id: menu

            active: false
            sourceComponent: SongMenu {
                songs: Player.upNext
                row: queueRow.index
                upNext: true
                songId: queueRow.songId
                songTitle: queueRow.title
                artist: queueRow.artist
                albumId: queueRow.albumId
                artistId: queueRow.artistId
                starred: queueRow.starred
                onClosed: menu.active = false
            }
        }
    }

    T.ScrollBar.vertical: Scroller { underHeader: false }
}

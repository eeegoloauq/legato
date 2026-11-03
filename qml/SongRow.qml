pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Legato

// A song in a list (docs/design.md, "Song lists"). A click selects it, a double click plays it; the
// number turns into a play button under the pointer, and the playing row shows a level meter.
// "…" under the pointer, a right click or Shift+F10 opens the song menu.
//
// `album` rows (an album's page) show the track number and the title, with the song's artists
// under it when they are not the album's. `list` rows show the cover with the artists under the
// title, and the album in a column when there is room.
Item {
    id: songRow

    required property int index
    required property string songId
    required property string title
    required property string artist
    required property var artists
    required property string album
    required property string albumId
    required property string artistId
    required property string coverArt
    required property int duration
    required property int track
    required property int discHeader
    required property bool starred
    required property int playCount

    property SongModel songs
    // The row in `songs`, which differs from `index` while a find filters the list.
    property int row: index
    property string mode: "list"
    // An album's artist: an album row names the song's artists only when they differ.
    property string albumArtist
    property string playlistId
    property bool selected: false
    readonly property bool playing: Player.songId === songId
    readonly property bool showsArtist: mode === "list" || (artist !== albumArtist && artist !== "")
    readonly property real rowHeight: mode === "album" ? (showsArtist ? 52 : 44) : 56

    signal selectRequested()
    signal playRequested()

    // Opens the song menu at (x, y) in `item`, or from "…" when `item` is null.
    function openMenu(item, x, y) {
        menu.active = true
        const popup = menu.item as SongMenu
        if (item)
            popup.openAt(item, x, y)
        else
            popup.openFrom(more, 2, true)
    }

    width: ListView.view ? ListView.view.width : 600
    height: rowHeight + (discHeader > 0 ? 44 : 0)

    Text {
        visible: songRow.discHeader > 0
        x: 12
        y: songRow.index === 0 ? 8 : 18
        text: qsTr("Disc %1").arg(songRow.discHeader)
        color: Theme.text2
        font.pixelSize: Theme.fsSmall
        font.weight: Font.DemiBold
    }

    Item {
        id: body

        anchors.bottom: parent.bottom
        width: parent.width
        height: songRow.rowHeight

        HoverHandler {
            id: hover
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: songRow.selectRequested()
            onDoubleTapped: songRow.playRequested()
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: eventPoint => {
                songRow.selectRequested()
                songRow.openMenu(body, eventPoint.position.x, eventPoint.position.y)
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusArt
            color: songRow.selected ? Theme.fill2 : hover.hovered ? Theme.fill : "transparent"
        }

        // The line between rows, hidden where a row is lit.
        Rectangle {
            x: 12
            width: parent.width - 24
            height: 1
            anchors.bottom: parent.bottom
            visible: !songRow.selected && !hover.hovered
            color: Theme.line
        }

        Item {
            id: number

            x: 12
            width: 28
            height: parent.height

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: !hover.hovered && !songRow.playing
                text: songRow.mode === "album" && songRow.track > 0 ? songRow.track : songRow.row + 1
                color: Theme.text3
                font.pixelSize: Theme.fsSmall
                font.features: { "tnum": 1 }
            }

            LevelMeter {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                visible: songRow.playing && !hover.hovered
            }

            ToolButton {
                anchors.right: parent.right
                anchors.rightMargin: -4
                anchors.verticalCenter: parent.verticalCenter
                width: 26
                height: 26
                padding: 0
                visible: hover.hovered
                focusPolicy: Qt.NoFocus
                display: AbstractButton.IconOnly
                icon.source: Theme.icon(songRow.playing && Player.playing ? "pause" : "play_arrow")
                icon.width: 18
                icon.height: 18
                icon.color: Theme.text
                Accessible.name: qsTr("Play %1").arg(songRow.title)
                onClicked: songRow.playing ? Player.togglePause() : songRow.playRequested()
                background: Item {}
            }
        }

        RoundedImage {
            id: cover

            anchors.left: number.right
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            visible: songRow.mode === "list"
            width: visible ? 40 : 0
            height: 40
            radius: 5
            coverArt: songRow.coverArt
        }

        Column {
            anchors.left: cover.right
            anchors.leftMargin: cover.visible ? 12 : 14
            anchors.right: albumColumn.visible ? albumColumn.left : plays.visible ? plays.left : time.left
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                width: parent.width
                text: songRow.title
                color: songRow.playing ? Theme.tint : Theme.text
                font.pixelSize: Theme.fs
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            ArtistLinks {
                width: parent.width
                height: 17
                clip: true
                visible: songRow.showsArtist
                artists: songRow.artists
                fallback: songRow.artist
            }
        }

        Text {
            id: albumColumn

            anchors.right: time.left
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            visible: songRow.mode === "list" && songRow.width > 640
            width: (songRow.width - 160) * 0.38
            text: songRow.album
            color: Theme.text2
            font.pixelSize: Theme.fsSmall
            font.underline: albumHover.hovered
            elide: Text.ElideRight

            HoverHandler {
                id: albumHover

                enabled: songRow.albumId !== ""
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                enabled: songRow.albumId !== ""
                onTapped: Navigation.album(songRow.albumId)
            }
        }

        // How often the listener played the song, from the server's count of scrobbles: on an
        // album's page, as streaming apps show plays there. Nothing for a song never played.
        Row {
            id: plays

            anchors.right: time.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            visible: songRow.mode === "album" && songRow.playCount > 0
            spacing: 3

            Glyph {
                anchors.verticalCenter: parent.verticalCenter
                name: "play_arrow"
                size: 12
                color: Theme.text3
            }

            Text {
                text: songRow.playCount.toLocaleString(Qt.locale(), "f", 0)
                color: Theme.text3
                font.pixelSize: Theme.fsSmall
                font.features: { "tnum": 1 }
            }

            HoverHandler {
                id: playsHover
            }

            ToolTip.text: qsTr("Played %n time(s)", "", songRow.playCount)
            ToolTip.visible: playsHover.hovered
            ToolTip.delay: 600
        }

        Text {
            id: time

            anchors.right: more.left
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            horizontalAlignment: Text.AlignRight
            text: Math.floor(songRow.duration / 60) + ":" + String(songRow.duration % 60).padStart(2, "0")
            color: Theme.text2
            font.pixelSize: Theme.fsSmall
            font.features: { "tnum": 1 }
        }

        IconButton {
            id: more

            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            diameter: 28
            glyph: "more_horiz"
            glyphSize: 18
            tone: Theme.text2
            tip: qsTr("More")
            opacity: hover.hovered || songRow.selected || menu.active ? 1 : 0
            onClicked: {
                songRow.selectRequested()
                songRow.openMenu(null)
            }
        }
    }

    Loader {
        id: menu

        active: false
        sourceComponent: SongMenu {
            songs: songRow.songs
            row: songRow.row
            playlistId: songRow.playlistId
            songId: songRow.songId
            songTitle: songRow.title
            artist: songRow.artist
            albumId: songRow.albumId
            artistId: songRow.artistId
            starred: songRow.starred
            onClosed: menu.active = false
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import Legato

// A song in the artist page's Top Songs grid (docs/design.md, "Artist page"): cover, title,
// "album · year" and "…". A click plays the top songs from this one.
Item {
    id: songRow

    required property int index
    required property string songId
    required property string title
    required property string artist
    required property string album
    required property string albumId
    required property string artistId
    required property string coverArt
    required property int year
    required property bool starred

    property SongModel songs
    property string from
    readonly property bool playing: Player.songId === songId

    function openMenu(item, x, y) {
        menu.active = true
        const popup = menu.item as SongMenu
        if (item)
            popup.openAt(item, x, y)
        else
            popup.openFrom(more, 2, true)
    }

    implicitHeight: 56

    HoverHandler {
        id: hover

        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: Library.playFrom(songRow.songs, songRow.index, songRow.from)
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => songRow.openMenu(songRow, eventPoint.position.x, eventPoint.position.y)
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusArt
        color: hover.hovered ? Theme.fill : "transparent"
    }

    RoundedImage {
        id: cover

        x: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 44
        height: 44
        radius: 5
        coverArt: songRow.coverArt
    }

    LevelMeter {
        anchors.centerIn: cover
        visible: songRow.playing
        color: "white"
    }

    Column {
        anchors.left: cover.right
        anchors.leftMargin: 12
        anchors.right: more.left
        anchors.rightMargin: 6
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

        Text {
            width: parent.width
            text: [songRow.album, songRow.year > 0 ? songRow.year : ""].filter(part => part !== "").join(" · ")
            color: Theme.text2
            font.pixelSize: Theme.fsSmall
            elide: Text.ElideRight
        }
    }

    IconButton {
        id: more

        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        diameter: 28
        glyph: "more_horiz"
        tone: Theme.text2
        tip: qsTr("More")
        opacity: hover.hovered || menu.active ? 1 : 0
        onClicked: songRow.openMenu(null)
    }

    Loader {
        id: menu

        active: false
        sourceComponent: SongMenu {
            songs: songRow.songs
            row: songRow.index
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

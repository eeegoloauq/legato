pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import Legato

// An album in a grid or a carousel (docs/design.md, "Covers and carousels"). The cover and the
// title open the album, the artist line the artist; under the pointer a play button shows bottom
// left and "…" bottom right. The cover does not move.
Item {
    id: card

    required property string albumId
    required property string name
    required property string artist
    required property string artistId
    required property string coverArt
    required property int hue
    required property bool starred
    // The line under the title: the artist, or the year on the artist's own page.
    property string subtitle: artist
    property bool subtitleLinks: subtitle === artist
    property real coverSize: 180
    // Chosen with the arrow keys: a ring around the cover, and Enter opens it.
    property bool keyFocus: false
    // The play and "…" buttons are made the first time they are wanted: a page of covers builds
    // without them, and Home opened with a pause of 100-200 ms while it made sixty.
    readonly property bool buttonsWanted: hover.hovered || keyFocus || menu.active
    property bool buttonsMade: false
    property Item moreButton: null

    onButtonsWantedChanged: if (buttonsWanted) buttonsMade = true

    width: GridView.view ? GridView.view.cellWidth : coverSize
    height: GridView.view ? GridView.view.cellHeight : coverSize + 66

    HoverHandler {
        id: hover
    }

    // A right click on the cover or the words opens the "…" menu at the pointer.
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => {
            menu.fromMore = false
            menu.at = eventPoint.position
            menu.active = true
        }
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

        RoundedImage {
            anchors.fill: parent
            coverArt: card.coverArt
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            onTapped: Navigation.album(card.albumId)
        }

        Loader {
            anchors.fill: parent
            active: card.buttonsMade

            sourceComponent: Item {
                CoverButton {
                    x: 8
                    y: parent.height - height - 8
                    glyph: "play_arrow"
                    shown: hover.hovered || card.keyFocus
                    Accessible.name: qsTr("Play %1").arg(card.name)
                    onClicked: Library.playAlbum(card.albumId)
                }

                CoverButton {
                    id: more

                    x: parent.width - width - 8
                    // Centered on the larger play button's line.
                    y: parent.height - 8 - 20 - height / 2
                    glyph: "more_horiz"
                    shown: hover.hovered || menu.active
                    Accessible.name: qsTr("More for %1").arg(card.name)
                    Component.onCompleted: card.moreButton = more
                    onClicked: {
                        menu.fromMore = true
                        menu.active = true
                    }
                }
            }
        }

        // Made when asked for: a grid of a thousand albums keeps no thousand menus.
        Loader {
            id: menu

            // From "…", or at the right click's point in the card.
            property bool fromMore
            property point at

            active: false
            onLoaded: fromMore ? (item as AlbumMenu).openFrom(card.moreButton) : (item as AlbumMenu).openAt(card, at.x, at.y)
            sourceComponent: AlbumMenu {
                albumId: card.albumId
                name: card.name
                artist: card.artist
                artistId: card.artistId
                starred: card.starred
                coverArt: card.coverArt
                hue: card.hue
                onClosed: menu.active = false
            }
        }
    }

    Text {
        id: title

        anchors.top: media.bottom
        anchors.topMargin: 8
        width: card.coverSize
        text: card.name
        color: Theme.text
        font.pixelSize: Theme.fs
        font.weight: Font.Medium
        font.underline: titleHover.hovered
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight

        HoverHandler {
            id: titleHover

            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            onTapped: Navigation.album(card.albumId)
        }
    }

    Text {
        anchors.top: title.bottom
        anchors.topMargin: 1
        width: Math.min(implicitWidth, card.coverSize)
        text: card.subtitle
        color: Theme.text2
        font.pixelSize: Theme.fsSmall
        font.underline: subtitleHover.hovered
        elide: Text.ElideRight

        HoverHandler {
            id: subtitleHover

            enabled: card.subtitleLinks && card.artistId !== ""
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            enabled: card.subtitleLinks && card.artistId !== ""
            onTapped: Navigation.artist(card.artistId)
        }
    }
}

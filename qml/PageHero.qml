import QtQuick
import Legato

// The top of an album or playlist page: the cover on the left; beside it,
// aligned to its bottom, the kind of page, the title, the artists, a line of details, a comment
// with More, and the buttons (children).
Item {
    id: hero

    // The cover goes here, filling it.
    property alias cover: coverSlot.data
    property string eyebrow
    property string title
    property var artists: []
    property string artist
    property string meta
    property string badge
    property string comment
    default property alias actions: actionRow.data

    readonly property real coverSize: width < 640 ? 180 : 240
    property bool commentOpen: false

    implicitHeight: Theme.headerHeight + 8 + Math.max(coverSize, info.implicitHeight) + 36

    Item {
        id: coverSlot

        y: Theme.headerHeight + 8
        width: hero.coverSize
        height: hero.coverSize
    }

    Column {
        id: info

        anchors.left: coverSlot.right
        anchors.leftMargin: 30
        anchors.right: parent.right
        anchors.bottom: coverSlot.bottom
        spacing: 6

        Text {
            text: hero.eyebrow.toUpperCase()
            color: Theme.text2
            font.pixelSize: Theme.fsTiny
            font.weight: Font.DemiBold
            font.letterSpacing: 0.06 * Theme.fsTiny
        }

        Text {
            width: parent.width
            text: hero.title
            color: Theme.text
            font.pixelSize: 36
            font.weight: Font.Bold
            font.letterSpacing: -0.025 * 36
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        ArtistLinks {
            width: parent.width
            visible: hero.artists.length > 0 || hero.artist !== ""
            artists: hero.artists
            fallback: hero.artist
            color: Theme.accent
            pixelSize: Theme.fsSection
            weight: Font.Medium
        }

        Row {
            spacing: 8
            visible: hero.meta !== "" || hero.badge !== ""

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: hero.meta !== ""
                text: hero.meta
                color: Theme.text2
                font.pixelSize: Theme.fs
            }

            QualityBadge {
                anchors.verticalCenter: parent.verticalCenter
                text: hero.badge
            }
        }

        Row {
            width: parent.width
            spacing: 6
            visible: hero.comment !== ""

            Text {
                id: comment

                width: Math.min(implicitWidth, parent.width - (more.visible ? more.width + 6 : 0), 560)
                text: hero.comment
                color: Theme.text2
                font.pixelSize: Theme.fsSmall
                wrapMode: hero.commentOpen ? Text.Wrap : Text.NoWrap
                elide: hero.commentOpen ? Text.ElideNone : Text.ElideRight
            }

            Text {
                id: more

                visible: comment.truncated || hero.commentOpen
                text: hero.commentOpen ? qsTr("Less") : qsTr("More")
                color: Theme.text
                font.pixelSize: Theme.fsSmall
                font.weight: Font.Medium

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: hero.commentOpen = !hero.commentOpen
                }
            }
        }

        Item {
            width: 1
            height: 8
        }

        Row {
            id: actionRow

            spacing: 8
        }
    }
}

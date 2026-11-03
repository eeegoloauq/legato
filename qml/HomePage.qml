pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Legato

// Home (docs/design.md, "Home"): the title with Shuffle beside it, then rows of covers. The three
// cards of the design come back once Continue and Instant Mix exist; until then Shuffle is a button.
ContentPage {
    id: page

    route: ({ page: "home" })
    title: qsTr("Home")
    flickable: view

    AlbumList {
        id: recent

        type: "newest"
        limit: 15
    }

    AlbumList {
        id: random

        type: "random"
        limit: 15
    }

    AlbumList {
        id: frequent

        type: "frequent"
        limit: 15
    }

    Component.onCompleted: Library.loadPlaylists()

    component AlbumRow: Carousel {
        id: row

        width: parent ? parent.width : 0
        delegate: AlbumCard {
            coverSize: row.coverSize
        }
    }

    Flickable {
        id: view

        anchors.fill: parent
        anchors.leftMargin: Theme.gutter
        anchors.rightMargin: Theme.gutter
        contentHeight: column.height
        bottomMargin: Theme.barHeight + 56
        boundsBehavior: Flickable.StopAtBounds
        pixelAligned: true

        Column {
            id: column

            width: view.width
            spacing: 36

            PageHeading {
                text: qsTr("Home")
                // The heading's own bottom gap and the column's spacing would add up.
                height: implicitHeight - 20

                PushButton {
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: "shuffle"
                    text: qsTr("Shuffle")
                    ToolTip.text: qsTr("Shuffle your whole library (Ctrl+Shift+S)")
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    onClicked: Library.shuffleLibrary()
                }
            }

            Text {
                width: Math.min(parent.width, 640)
                visible: recent.error !== ""
                text: qsTr("Could not load your library: %1").arg(recent.error)
                color: Theme.text2
                wrapMode: Text.Wrap
            }

            AlbumRow {
                visible: recent.model.count > 0
                title: qsTr("Recently added")
                opens: true
                model: recent.model
                onOpened: Navigation.open({ page: "recent" })
            }

            AlbumRow {
                visible: random.model.count > 0
                title: qsTr("Random albums")
                model: random.model

                IconButton {
                    glyph: "refresh"
                    tone: Theme.text2
                    tip: qsTr("New random albums")
                    onClicked: random.reload()
                }
            }

            AlbumRow {
                visible: frequent.model.count > 0
                title: qsTr("Most played")
                model: frequent.model
            }

            Carousel {
                id: playlists

                width: parent.width
                visible: Library.playlists.count > 0
                title: qsTr("Playlists")
                opens: true
                model: Library.playlists
                onOpened: Navigation.open({ page: "playlists" })
                delegate: PlaylistCard {
                    coverSize: playlists.coverSize
                }
            }
        }

        T.ScrollBar.vertical: Scroller {}
    }

    WheelScroll {
        flickable: view
    }
}

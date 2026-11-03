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

    // The rows are HomeFeed's, kept between visits.
    readonly property AlbumList played: HomeFeed.played
    readonly property AlbumList recent: HomeFeed.recent
    readonly property AlbumList random: HomeFeed.random
    readonly property AlbumList frequent: HomeFeed.frequent

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
        // Hidden until every row is in, as the pages with details.
        opacity: HomeFeed.ready ? 1 : 0

        Behavior on opacity {
            enabled: Theme.animationsOn
            NumberAnimation { duration: Theme.normalMs; easing.type: Easing.OutCubic }
        }

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
                visible: page.recent.error !== ""
                text: qsTr("Could not load your library: %1").arg(page.recent.error)
                color: Theme.text2
                wrapMode: Text.Wrap
            }

            AlbumRow {
                visible: page.played.model.count > 0
                title: qsTr("Recently played")
                model: page.played.model
            }

            AlbumRow {
                visible: page.recent.model.count > 0
                title: qsTr("Recently added")
                opens: true
                model: page.recent.model
                onOpened: Navigation.open({ page: "recent" })
            }

            AlbumRow {
                visible: page.random.model.count > 0
                title: qsTr("Random albums")
                model: page.random.model

                IconButton {
                    glyph: "refresh"
                    tone: Theme.text2
                    tip: qsTr("New random albums")
                    onClicked: page.random.reload()
                }
            }

            AlbumRow {
                visible: page.frequent.model.count > 0
                title: qsTr("Most played")
                model: page.frequent.model
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

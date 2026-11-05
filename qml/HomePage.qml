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

    // The rows as a ListView of shelves, as lumeo's Home: a row is built when it comes near the
    // screen, so opening Home builds only the rows that show, and the ones below are built after
    // it, in the background. A few screens of rows stay built, so a row scrolled sideways is where
    // it was on the way back up.
    ListView {
        id: view

        anchors.fill: parent
        anchors.leftMargin: Theme.gutter
        anchors.rightMargin: Theme.gutter
        bottomMargin: Theme.barHeight + 56
        boundsBehavior: Flickable.StopAtBounds
        pixelAligned: true
        keyNavigationEnabled: false
        // No current row, which would take the keyboard as the page opens.
        currentIndex: -1
        cacheBuffer: 3000
        model: ["played", "recent", "random", "frequent", "playlists"]
        // Hidden until every row is in, as the pages with details.
        opacity: HomeFeed.ready ? 1 : 0

        Behavior on opacity {
            enabled: Theme.animationsOn
            NumberAnimation { duration: Theme.normalMs; easing.type: Easing.OutCubic }
        }

        header: Column {
            width: view.width

            PageHeading {
                text: qsTr("Home")
                // The heading's own bottom gap and the gap above a row would add up.
                height: implicitHeight - 20

                PushButton {
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: "shuffle"
                    text: qsTr("Shuffle")
                    ToolTip.text: qsTr("Shuffle your whole library (Ctrl+Shift+S)")
                    ToolTip.visible: hovered
                    onClicked: Library.shuffleLibrary()
                }
            }

            Text {
                width: Math.min(parent.width, 640)
                topPadding: 36
                visible: page.recent.error !== ""
                text: qsTr("Could not load your library: %1").arg(page.recent.error)
                color: Theme.text2
                wrapMode: Text.Wrap
            }
        }

        delegate: Item {
            id: slot

            required property string modelData
            readonly property Item row: loader.item as Item
            readonly property real gap: 36

            width: view.width
            // An empty row takes no room.
            height: row && row.visible ? row.height + gap : 0

            Loader {
                id: loader

                y: slot.gap
                width: parent.width
                sourceComponent: slot.modelData === "playlists" ? playlistRow : albumRow

                Component {
                    id: albumRow

                    Carousel {
                        id: carousel

                        readonly property AlbumList albums: ({
                            played: page.played, recent: page.recent, random: page.random, frequent: page.frequent
                        })[slot.modelData]

                        visible: albums.model.count > 0
                        title: ({
                            played: qsTr("Recently played"), recent: qsTr("Recently added"),
                            random: qsTr("Random albums"), frequent: qsTr("Most played")
                        })[slot.modelData]
                        opens: slot.modelData === "recent"
                        model: albums.model
                        onOpened: Navigation.open({ page: "recent" })
                        delegate: AlbumCard {
                            coverSize: carousel.coverSize
                        }

                        IconButton {
                            visible: slot.modelData === "random"
                            glyph: "refresh"
                            tone: Theme.text2
                            tip: qsTr("New random albums")
                            onClicked: page.random.reload()
                        }
                    }
                }

                Component {
                    id: playlistRow

                    Carousel {
                        id: playlists

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
            }
        }

        T.ScrollBar.vertical: Scroller {}
    }

    WheelScroll {
        flickable: view
    }
}

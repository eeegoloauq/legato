pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import Legato

// An artist (docs/design.md, "Artist page"): a centered header with the round photo, the name, a
// large Play, Shuffle and a star; the latest release beside the top songs; then the albums, and the
// singles and EPs, as carousels. "Appears On" waits for an API that lists it (docs/roadmap.md).
ContentPage {
    id: page

    route: ({ page: "artist", id: "" })
    title: details.name
    flickable: view

    ArtistDetails {
        id: details

        artistId: page.route.id ?? ""
    }

    PageTint {
        width: parent.width
        y: view.originY - view.contentY
        coverArt: details.latest.coverArt ?? ""
    }

    Flickable {
        id: view

        // Shown whole once the page's details are in, never filled in piece by piece.
        opacity: details.name !== "" || details.error !== "" ? 1 : 0

        Behavior on opacity {
            enabled: Theme.animationsOn
            NumberAnimation { duration: Theme.normalMs; easing.type: Easing.OutCubic }
        }

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
            topPadding: Theme.headerHeight + 8
            spacing: 40

            Column {
                width: parent.width
                spacing: 14

                RoundedImage {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 180
                    height: 180
                    radius: 90
                    coverArt: details.coverArt
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: details.name
                    color: Theme.text
                    font.pixelSize: 36
                    font.weight: Font.Bold
                    font.letterSpacing: -0.025 * 36
                    wrapMode: Text.Wrap
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    visible: details.error !== ""
                    text: qsTr("Could not load this artist: %1").arg(details.error)
                    color: Theme.text2
                    wrapMode: Text.Wrap
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10

                    PushButton {
                        anchors.verticalCenter: parent.verticalCenter
                        primary: true
                        implicitHeight: 40
                        leftPadding: 26
                        rightPadding: 28
                        glyph: "play_arrow"
                        text: qsTr("Play")
                        enabled: details.songsReady
                        onClicked: details.play()
                    }
                    PushButton {
                        anchors.verticalCenter: parent.verticalCenter
                        implicitHeight: 40
                        glyph: "shuffle"
                        text: qsTr("Shuffle")
                        enabled: details.songsReady
                        onClicked: details.play(true)
                    }
                    IconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        diameter: 40
                        glyph: details.starred ? "star_filled" : "star"
                        glyphSize: 22
                        tone: Theme.text
                        restFill: Theme.fill2
                        tip: details.starred ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
                        enabled: details.name !== ""
                        onClicked: Library.setArtistStarred(details.artistId, !details.starred)
                    }
                    IconButton {
                        id: more

                        anchors.verticalCenter: parent.verticalCenter
                        diameter: 40
                        glyph: "more_horiz"
                        glyphSize: 22
                        tone: Theme.text
                        restFill: Theme.fill2
                        tip: qsTr("More")
                        enabled: details.name !== ""
                        onClicked: artistMenu.openFrom(more)

                        ArtistMenu {
                            id: artistMenu

                            artistId: details.artistId
                            name: details.name
                            coverArt: details.coverArt
                        }
                    }
                }
            }

            // The latest release beside the top songs; one over the other when narrow.
            Grid {
                id: highlights

                readonly property bool wide: width >= 820
                readonly property real latestWidth: wide ? 260 : width

                width: parent.width
                visible: details.latest.albumId !== undefined || details.topSongs.count > 0
                columns: wide ? 2 : 1
                columnSpacing: 36
                rowSpacing: 32

                Column {
                    width: highlights.latestWidth
                    visible: details.latest.albumId !== undefined
                    spacing: 14

                    SectionTitle {
                        text: qsTr("Latest Release")
                    }

                    Row {
                        spacing: 16

                        RoundedImage {
                            width: 140
                            height: 140
                            coverArt: details.latest.coverArt ?? ""

                            HoverHandler {
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler {
                                onTapped: Navigation.album(details.latest.albumId)
                            }
                        }

                        Column {
                            anchors.bottom: parent.bottom
                            width: highlights.latestWidth - 156
                            spacing: 2

                            Text {
                                visible: (details.latest.year ?? 0) > 0
                                text: details.latest.year ?? ""
                                color: Theme.text2
                                font.pixelSize: Theme.fsSmall
                            }

                            Text {
                                width: parent.width
                                text: details.latest.name ?? ""
                                color: Theme.text
                                font.pixelSize: Theme.fs
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }

                            Text {
                                readonly property int songs: details.latest.songCount ?? 0

                                visible: songs > 0
                                text: songs === 1 ? qsTr("1 song") : qsTr("%1 songs").arg(songs)
                                color: Theme.text2
                                font.pixelSize: Theme.fsSmall
                            }
                        }
                    }
                }

                Column {
                    id: topColumn

                    readonly property int columns: Math.max(1, Math.min(3, Math.floor(width / 250)))

                    width: highlights.wide ? highlights.width - highlights.latestWidth - highlights.columnSpacing : highlights.width
                    visible: details.topSongs.count > 0
                    spacing: 10

                    SectionTitle {
                        text: qsTr("Top Songs")
                    }

                    Grid {
                        width: parent.width
                        columns: topColumn.columns
                        columnSpacing: 12

                        Repeater {
                            model: details.topSongs

                            // Three rows, as many columns as fit (up to three).
                            delegate: CompactSongRow {
                                visible: index < topColumn.columns * 3
                                width: (topColumn.width - (topColumn.columns - 1) * 12) / topColumn.columns
                                songs: details.topSongs
                                from: details.name
                            }
                        }
                    }
                }
            }

            Carousel {
                id: albums

                width: parent.width
                visible: details.albums.count > 0
                title: qsTr("Albums")
                model: details.albums
                delegate: AlbumCard {
                    required property int year

                    coverSize: albums.coverSize
                    subtitle: year > 0 ? String(year) : ""
                    subtitleLinks: false
                }
            }

            Carousel {
                id: singles

                width: parent.width
                visible: details.singles.count > 0
                title: qsTr("Singles & EPs")
                model: details.singles
                delegate: AlbumCard {
                    required property int year

                    coverSize: singles.coverSize
                    subtitle: year > 0 ? String(year) : ""
                    subtitleLinks: false
                }
            }

            // Other artists' albums the artist sings or plays on, under their album artist.
            Carousel {
                id: appearsOn

                width: parent.width
                visible: details.appearsOn.count > 0
                title: qsTr("Appears On")
                model: details.appearsOn
                delegate: AlbumCard {
                    coverSize: appearsOn.coverSize
                }
            }
        }

        T.ScrollBar.vertical: Scroller {}
    }

    WheelScroll {
        flickable: view
    }
}

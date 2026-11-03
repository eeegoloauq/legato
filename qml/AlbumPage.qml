pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Legato

// An album (docs/design.md, "Album page"): the top tinted with the cover's colors, the cover with
// the album's details and buttons beside it, the songs by disc, and a footer with the totals.
ContentPage {
    id: page

    route: ({ page: "album", id: "" })
    title: details.name
    flickable: list
    // The whole window takes the cover's color, not only the top of the page.
    backdrop: tint.coverColors.length > 0 ? Theme.backdropFrom(tint.coverColors[0]) : "transparent"

    AlbumDetails {
        id: details

        albumId: page.route.id ?? ""
    }

    PageTint {
        id: tint

        width: parent.width
        y: list.originY - list.contentY
        coverArt: details.coverArt
    }

    SongListView {
        id: list

        // Shown whole once the page's details are in, never filled in piece by piece.
        opacity: details.name !== "" || details.error !== "" ? 1 : 0

        Behavior on opacity {
            enabled: Theme.animationsOn
            NumberAnimation { duration: Theme.normalMs; easing.type: Easing.OutCubic }
        }

        songs: details.songs
        from: details.name
        findPage: page
        mode: "album"
        albumArtist: details.artist

        header: PageHero {
            x: 12
            width: list.width - 24
            eyebrow: qsTr("Album")
            title: details.name
            artists: details.artists
            artist: details.artist
            meta: [details.genre, details.year > 0 ? details.year : ""].filter(part => part !== "").join(" · ")
            badge: details.quality
            comment: details.comment
            cover: RoundedImage {
                anchors.fill: parent
                coverArt: details.coverArt

                // A right click on the cover opens the album's "…" menu at the pointer.
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    enabled: details.name !== ""
                    onTapped: eventPoint => albumMenu.openAt(parent, eventPoint.position.x, eventPoint.position.y)
                }
            }

            PushButton {
                primary: true
                glyph: "play_arrow"
                text: qsTr("Play")
                enabled: details.songs.count > 0
                onClicked: Library.playFrom(details.songs, -1, details.name)
            }
            PushButton {
                glyph: "shuffle"
                text: qsTr("Shuffle")
                enabled: details.songs.count > 0
                onClicked: Library.playFrom(details.songs, -1, details.name, true)
            }
            IconButton {
                diameter: 34
                glyph: details.starred ? "star_filled" : "star"
                glyphSize: 21
                tone: Theme.text
                restFill: Theme.fill2
                tip: details.starred ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
                enabled: details.name !== ""
                onClicked: Library.setAlbumStarred(details.albumId, !details.starred)
            }
            IconButton {
                id: more

                diameter: 34
                glyph: "more_horiz"
                glyphSize: 21
                restFill: Theme.fill2
                tip: qsTr("More")
                enabled: details.name !== ""
                onClicked: albumMenu.openFrom(more)

                AlbumMenu {
                    id: albumMenu

                    albumId: details.albumId
                    name: details.name
                    artist: details.artist
                    artistId: details.artists.length > 0 ? details.artists[0].id : ""
                    starred: details.starred
                    coverArt: details.coverArt
                }
            }
        }

        footer: Text {
            x: 12
            width: list.width - 24
            topPadding: 20
            text: details.error !== "" ? qsTr("Could not load this album: %1").arg(details.error)
                : list.filter.active ? (list.count === 0 ? qsTr("No songs match.") : "")
                : details.summary
            color: Theme.text2
            font.pixelSize: Theme.fsSmall
            font.features: { "tnum": 1 }
            wrapMode: Text.Wrap
        }
    }

    WheelScroll {
        flickable: list
        onMoved: list.pointerMoved()
    }
}

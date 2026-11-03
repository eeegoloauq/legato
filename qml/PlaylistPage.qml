pragma ComponentBehavior: Bound

import QtQuick
import Legato

// A playlist (docs/design.md, "Playlist page"): its cover (a drifting generated one when it has
// none), name, description or when it changed, Play and Shuffle, the songs with their covers, and
// the totals.
ContentPage {
    id: page

    route: ({ page: "playlist", id: "" })
    title: playlist.name
    flickable: list

    PlaylistDetails {
        id: playlist

        playlistId: page.route.id ?? ""
    }

    PageTint {
        width: parent.width
        y: list.originY - list.contentY
        coverArt: playlist.coverArt
    }

    SongListView {
        id: list

        // Shown whole once the page's details are in, never filled in piece by piece.
        opacity: playlist.name !== "" || playlist.error !== "" ? 1 : 0

        Behavior on opacity {
            enabled: Theme.animationsOn
            NumberAnimation { duration: Theme.normalMs; easing.type: Easing.OutCubic }
        }

        songs: playlist.songs
        from: playlist.name
        playlistId: playlist.editable ? playlist.playlistId : ""
        searchPage: page

        header: PageHero {
            x: 12
            width: list.width - 24
            eyebrow: qsTr("Playlist")
            title: playlist.name
            meta: playlist.comment !== "" ? playlist.comment : playlist.updated
            cover: PlaylistCover {
                anchors.fill: parent
                coverArt: playlist.coverArt
                name: playlist.name
                hue: playlist.hue
                radius: Theme.radiusArt
                live: true
                active: list.contentY - list.originY < height
            }

            PushButton {
                primary: true
                glyph: "play_arrow"
                text: qsTr("Play")
                enabled: playlist.songs.count > 0
                onClicked: Library.playFrom(playlist.songs, -1, playlist.name)
            }
            PushButton {
                glyph: "shuffle"
                text: qsTr("Shuffle")
                enabled: playlist.songs.count > 0
                onClicked: Library.playFrom(playlist.songs, -1, playlist.name, true)
            }
            IconButton {
                id: more

                diameter: 34
                glyph: "more_horiz"
                glyphSize: 21
                tone: Theme.text
                restFill: Theme.fill2
                tip: qsTr("More")
                enabled: playlist.name !== ""
                onClicked: playlistMenu.openFrom(more)

                PlaylistMenu {
                    id: playlistMenu

                    playlistId: playlist.playlistId
                    name: playlist.name
                    coverArt: playlist.coverArt
                    hue: playlist.hue
                    editable: playlist.editable
                }
            }
        }

        footer: Text {
            x: 12
            width: list.width - 24
            topPadding: 20
            text: playlist.error !== "" ? qsTr("Could not load this playlist: %1").arg(playlist.error)
                : !playlist.loading && playlist.songs.count === 0 ? qsTr("No songs yet.")
                : playlist.summary
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

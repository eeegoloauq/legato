pragma ComponentBehavior: Bound

import QtQuick
import Legato

// Favorites: the songs marked with a star, then the albums and artists.
// Unstarring here leaves the row until the page opens again, so a slip is undone in place.
ContentPage {
    id: page

    route: ({ page: "favorites" })
    title: qsTr("Favorites")
    flickable: list

    Favorites {
        id: favorites
    }

    SongListView {
        id: list

        songs: favorites.songs
        from: qsTr("Favorite songs")
        searchPage: page

        header: Column {
            x: 12
            width: list.width - 24

            PageHeading {
                text: page.title

                PushButton {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: favorites.songs.count > 0
                    primary: true
                    glyph: "play_arrow"
                    text: qsTr("Play")
                    onClicked: Library.playFrom(favorites.songs, -1, qsTr("Favorite songs"))
                }
                PushButton {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: favorites.songs.count > 0
                    glyph: "shuffle"
                    text: qsTr("Shuffle")
                    onClicked: Library.playFrom(favorites.songs, -1, qsTr("Favorite songs"), true)
                }
            }

            Text {
                width: Math.min(parent.width, 640)
                visible: favorites.error !== "" || (!favorites.loading && favorites.songs.count === 0)
                text: favorites.error !== "" ? qsTr("Could not load favorites: %1").arg(favorites.error)
                    : qsTr("Songs you mark with a star appear here and sync to your server.")
                color: Theme.text2
                wrapMode: Text.Wrap
                bottomPadding: 16
            }
        }

        footer: Column {
            x: 12
            width: list.width - 24
            topPadding: 36
            spacing: 36

            Carousel {
                id: albums

                width: parent.width
                visible: favorites.albums.count > 0
                title: qsTr("Albums")
                model: favorites.albums
                delegate: AlbumCard {
                    coverSize: albums.coverSize
                }
            }

            Carousel {
                id: artists

                width: parent.width
                visible: favorites.artists.count > 0
                title: qsTr("Artists")
                model: favorites.artists
                textHeight: 50
                delegate: ArtistCard {
                    coverSize: artists.coverSize
                }
            }
        }
    }

    WheelScroll {
        flickable: list
        onMoved: list.pointerMoved()
    }
}

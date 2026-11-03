pragma ComponentBehavior: Bound

import QtQuick
import Legato

// All Playlists: every playlist of the server in a grid. Loaded again each time the page opens, so
// one made in another app shows up.
ContentPage {
    id: page

    route: ({ page: "playlists" })
    title: qsTr("Playlists")
    flickable: grid

    Component.onCompleted: Library.loadPlaylists()

    CoverGrid {
        id: grid

        model: Library.playlists
        onActivated: index => Navigation.playlist(Library.playlists.idAt(index))

        header: Column {
            width: grid.width - Theme.columnGap

            PageHeading {
                text: page.title
            }

            Text {
                visible: grid.count === 0
                text: qsTr("Playlists you make on your server or in another app appear here.")
                color: Theme.text2
                bottomPadding: 16
            }
        }

        delegate: PlaylistCard {
            required property int index

            coverSize: grid.coverSize
            keyFocus: grid.keyFocusOn(index)
        }
    }

    WheelScroll {
        flickable: grid
        onMoved: grid.pointerMoved()
    }
}

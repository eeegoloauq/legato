pragma ComponentBehavior: Bound

import QtQuick
import Legato

// Artists: every album artist in a grid of round photos, by name.
ContentPage {
    id: page

    route: ({ page: "artists" })
    title: qsTr("Artists")
    flickable: grid

    ArtistList {
        id: artists
    }

    CoverGrid {
        id: grid

        model: artists.model
        textHeight: 10 + 20 + 18
        onActivated: index => Navigation.artist(artists.model.idAt(index))

        header: Column {
            width: grid.width - Theme.columnGap

            PageHeading {
                text: page.title
            }

            Text {
                width: Math.min(parent.width, 640)
                visible: artists.error !== ""
                text: qsTr("Could not load artists: %1").arg(artists.error)
                color: Theme.text2
                wrapMode: Text.Wrap
                bottomPadding: 16
            }
        }

        delegate: ArtistCard {
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

pragma ComponentBehavior: Bound

import QtQuick
import Legato

// Albums: every album in a grid, sorted by Recently added,
// Title, Artist or Year; the choice is remembered. With `recent`, the Recently Added page: newest
// first and no sort.
ContentPage {
    id: page

    property bool recent: false
    // The server's orders for the four sorts.
    readonly property list<string> types: ["newest", "alphabeticalByName", "alphabeticalByArtist", "byYear"]
    property int sort: 1

    route: ({ page: recent ? "recent" : "albums" })
    title: recent ? qsTr("Recently Added") : qsTr("Albums")
    flickable: grid

    StoredSetting {
        category: "albums"
        property alias sort: page.sort
    }

    AlbumList {
        id: albums

        type: page.recent ? "newest" : page.types[page.sort] ?? "alphabeticalByName"
    }

    CoverGrid {
        id: grid

        objectName: "albumGrid"
        model: albums.model
        onActivated: index => Navigation.album(albums.model.idAt(index))

        header: Column {
            width: grid.width - Theme.columnGap

            PageHeading {
                text: page.title

                Dropdown {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !page.recent
                    prefix: qsTr("Sort by")
                    labels: [qsTr("Recently Added"), qsTr("Title"), qsTr("Artist"), qsTr("Year")]
                    current: page.sort
                    Accessible.name: qsTr("Sort")
                    onChosen: index => {
                        page.sort = index
                        grid.keyIndex = -1
                    }
                }
            }

            Text {
                width: Math.min(parent.width, 640)
                visible: albums.error !== ""
                text: qsTr("Could not load albums: %1").arg(albums.error)
                color: Theme.text2
                wrapMode: Text.Wrap
                bottomPadding: 16
            }
        }

        delegate: AlbumCard {
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

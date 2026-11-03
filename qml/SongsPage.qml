pragma ComponentBehavior: Bound

import QtQuick
import Legato

// Songs: every song of the library, loaded a page at a time as the list
// nears its end. Shuffle All plays random songs from the whole library.
ContentPage {
    id: page

    route: ({ page: "songs" })
    title: qsTr("Songs")
    flickable: list

    AllSongs {
        id: allSongs
    }

    SongListView {
        id: list

        songs: allSongs.model
        from: qsTr("Songs")
        // A screen and a half before the end, the next page is on its way.
        onContentYChanged: {
            if (!allSongs.complete && contentY + height * 2.5 > originY + contentHeight)
                allSongs.fetchMore()
        }

        header: Column {
            x: 12
            width: list.width - 24

            PageHeading {
                text: page.title

                PushButton {
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: "shuffle"
                    text: qsTr("Shuffle All")
                    onClicked: Library.shuffleLibrary()
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: allSongs.total > 0
                    text: allSongs.total === 1 ? qsTr("1 song") : qsTr("%1 songs").arg(allSongs.total.toLocaleString(Qt.locale(), "f", 0))
                    color: Theme.text2
                }
            }

            Text {
                width: Math.min(parent.width, 640)
                visible: allSongs.error !== ""
                text: qsTr("Could not load songs: %1").arg(allSongs.error)
                color: Theme.text2
                wrapMode: Text.Wrap
                bottomPadding: 16
            }
        }
    }

    WheelScroll {
        flickable: list
        onMoved: list.pointerMoved()
    }
}

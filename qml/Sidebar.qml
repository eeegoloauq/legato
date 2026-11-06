pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Legato

// The sidebar (docs/design.md, "Shell"): the pages, the pins, each playlist with its cover, and
// Settings at the foot. It has no background of its own: the window's backdrop shows through, split
// from the content by a hairline. Library rows and the playlists can be taken off (a right click,
// or Settings); a right click on a pin unpins it. In a narrow window it lies over the content
// (`floating`), on the backdrop's color with a shadow.
Item {
    id: sidebar

    // The current page's route, for the highlight.
    property var route: ({ page: "" })
    property bool floating: false

    signal navigate(var route)

    Shadow {
        anchors.fill: parent
        visible: sidebar.floating && Theme.shadowsOn
        offset.x: 8
        blur: 32
        color: Theme.shadow
    }

    Rectangle {
        anchors.fill: parent
        visible: sidebar.floating
        color: Theme.backdrop
    }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.line
    }

    // A strip for dragging the window. Main lays the sidebar button over its left end, so the
    // button stays where it is while the sidebar slides away.
    Item {
        id: strip

        width: parent.width
        height: Theme.headerHeight

        WindowDragArea {
            anchors.fill: parent
        }
    }

    component SectionLabel: Text {
        leftPadding: 10
        topPadding: 14
        bottomPadding: 4
        color: Theme.text3
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }

    // A row that opens a page, lit while that page shows.
    component PageItem: SidebarItem {
        property var target: ({ page: "" })

        width: ListView.view ? ListView.view.width : parent.width
        current: sidebar.route.page === target.page && (target.id === undefined || sidebar.route.id === target.id)
        onClicked: sidebar.navigate(target)
    }

    // A Library row or All Playlists: there unless taken off, and a right click offers to.
    component LibraryItem: PageItem {
        visible: Prefs.rowShown(target.page)
        onMenuRequested: at => {
            rowMenu.page = target.page
            rowMenu.openAt(null, at.x, at.y)
        }
    }

    PopupMenu {
        id: rowMenu

        property string page

        Action {
            text: rowMenu.page === "playlists" ? qsTr("Hide Playlists from Sidebar") : qsTr("Hide from Sidebar")
            onTriggered: Prefs.setRowShown(rowMenu.page, false)
        }

        MenuDivider {}

        Action {
            text: qsTr("Sidebar Settings…")
            onTriggered: sidebar.navigate({ page: "settings" })
        }
    }

    PopupMenu {
        id: pinMenu

        property string kind
        property string itemId

        Action {
            text: qsTr("Unpin")
            onTriggered: Prefs.unpin(pinMenu.kind, pinMenu.itemId)
        }
    }

    PlaylistMenu {
        id: playlistMenu
    }

    // The pages, then every playlist; it scrolls when the playlists do not fit.
    ListView {
        id: list

        anchors.top: strip.bottom
        anchors.bottom: foot.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: 1
        model: Prefs.rowShown("playlists") ? Library.playlists : null

        header: Column {
            width: list.width
            spacing: 1
            bottomPadding: 1

            PageItem { glyph: "home"; text: qsTr("Home"); target: ({ page: "home" }) }

            // Pinned albums, artists and playlists, in the order they were pinned.
            SectionLabel { visible: Prefs.pins.length > 0; text: qsTr("Pinned") }
            Repeater {
                model: Prefs.pins

                delegate: PageItem {
                    required property var modelData

                    cover: true
                    coverArt: modelData.coverArt
                    hue: modelData.hue
                    round: modelData.kind === "artist"
                    text: modelData.name
                    target: ({ page: modelData.kind, id: modelData.id })
                    onMenuRequested: at => {
                        pinMenu.kind = modelData.kind
                        pinMenu.itemId = modelData.id
                        pinMenu.openAt(null, at.x, at.y)
                    }
                }
            }

            SectionLabel {
                visible: ["recent", "artists", "albums", "songs", "favorites"].some(page => Prefs.rowShown(page))
                text: qsTr("Library")
            }
            LibraryItem { glyph: "schedule"; text: qsTr("Recently Added"); target: ({ page: "recent" }) }
            LibraryItem { glyph: "artist"; text: qsTr("Artists"); target: ({ page: "artists" }) }
            LibraryItem { glyph: "library_music"; text: qsTr("Albums"); target: ({ page: "albums" }) }
            LibraryItem { glyph: "music_note"; text: qsTr("Songs"); target: ({ page: "songs" }) }
            LibraryItem { glyph: "star"; text: qsTr("Favorites"); target: ({ page: "favorites" }) }

            SectionLabel { visible: Prefs.rowShown("playlists"); text: qsTr("Playlists") }
            LibraryItem { glyph: "grid_view"; text: qsTr("All Playlists"); target: ({ page: "playlists" }) }
        }

        delegate: PageItem {
            id: playlistRow

            required property string playlistId
            required property string name
            required coverArt
            required hue
            required property bool editable

            cover: true
            text: name
            target: ({ page: "playlist", id: playlistId })
            onMenuRequested: at => {
                playlistMenu.playlistId = playlistRow.playlistId
                playlistMenu.name = playlistRow.name
                playlistMenu.coverArt = playlistRow.coverArt
                playlistMenu.hue = playlistRow.hue
                playlistMenu.editable = playlistRow.editable
                playlistMenu.openAt(null, at.x, at.y)
            }
        }
    }

    // Settings, a row like the others at the foot. Servers are switched there, not here.
    PageItem {
        id: foot

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        glyph: "settings"
        text: qsTr("Settings")
        target: ({ page: "settings" })
    }
}

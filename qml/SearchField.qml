pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Legato

// Search (docs/design.md, "Search"): a field at the right end of the header on every page, with
// what it finds in a dropdown under it. On an album, a playlist or Favorites the page's own songs
// come first; then artists, albums, songs and playlists from the server, then the actions whose
// name matches. Ctrl+K, / or Ctrl+F go to it. Arrows choose, Enter opens or plays, Esc clears the
// field and then leaves it.
Item {
    id: search

    required property ContentPage page
    // An action the window carries out: "lyrics", "queue", "fullScreen" or "settings".
    signal actionRequested(string name)

    // How many of the page's own songs come first.
    readonly property int localCount: 5
    // The chosen row.
    property int current: 0
    // Where the pointer was last seen; rows moving under a still pointer choose nothing.
    property point pointer: Qt.point(-1, -1)
    readonly property string query: input.text.trim()
    readonly property bool active: input.activeFocus
    readonly property Item windowItem: Window.contentItem
    readonly property bool wanted: active && query !== ""
    // Shown once there is something to show: matches, or the server's word that there are none.
    readonly property bool shown: wanted && (rows.length > 0 || !results.searching)

    readonly property var actions: [
        { id: "shuffle", title: qsTr("Shuffle Library"), glyph: "shuffle", keys: "Ctrl+Shift+S" },
        { id: "mix", title: qsTr("Instant Mix"), glyph: "radio", keys: "" },
        { id: "lyrics", title: qsTr("Show Lyrics"), glyph: "lyrics", keys: "Ctrl+L" },
        { id: "queue", title: qsTr("Show Queue"), glyph: "format_list_bulleted", keys: "Ctrl+J" },
        { id: "fullScreen", title: qsTr("Open Full-Screen Player"), glyph: "open_in_full", keys: "" },
        { id: "info", title: qsTr("Get Info"), glyph: "info", keys: "Ctrl+I" },
        { id: "theme", title: qsTr("Change Theme"), glyph: "contrast", keys: "" },
        { id: "settings", title: qsTr("Settings"), glyph: "settings", keys: "Ctrl+," }
    ]
    readonly property var appearanceNames: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
    readonly property string localGroup: {
        switch (page ? page.route.page : "") {
        case "album": return qsTr("In This Album")
        case "playlist": return qsTr("In This Playlist")
        default: return qsTr("In Favorites")
        }
    }

    // The page's matches, the server's, then the actions, each marked when it opens a group.
    readonly property var rows: {
        const out = []
        if (query === "")
            return out
        void [local.count, local.text, local.songs]
        for (const match of local.firstMatches(localCount)) {
            out.push({ kind: "local", group: localGroup, id: match.songId, row: match.row, title: match.title,
                       detail: match.artist, coverArt: match.coverArt })
        }
        // Copies: a property added to a map from C++ does not stick.
        for (const item of results.items)
            out.push(Object.assign({}, item))
        const q = query.toLowerCase()
        for (const action of actions) {
            if (!action.title.toLowerCase().includes(q))
                continue
            // Made from the playing song, so only while something plays.
            if ((action.id === "mix" || action.id === "info") && !Player.hasSong)
                continue
            out.push({
                kind: "action", group: qsTr("Actions"), id: action.id, title: action.title, glyph: action.glyph,
                keys: action.keys,
                detail: action.id === "theme" ? appearanceNames[Theme.savedAppearance]
                        : action.id === "mix" ? Player.artist
                        : action.id === "info" ? Player.title : ""
            })
        }
        for (let i = 0; i < out.length; ++i)
            out[i].opensGroup = i === 0 || out[i].group !== out[i - 1].group
        return out
    }

    // Ctrl+K, / or Ctrl+F.
    function start() {
        input.forceActiveFocus()
        input.selectAll()
    }

    // Gives the keyboard back to the page.
    function leave() {
        input.text = ""
        if (page)
            page.forceActiveFocus()
    }

    function move(by) {
        if (rows.length > 0)
            current = Math.max(0, Math.min(rows.length - 1, current + by))
    }

    function run(index) {
        const row = rows[index]
        if (!row)
            return
        // The theme changes in place, so another Enter goes on to the next one.
        if (row.kind === "action" && row.id === "theme") {
            Theme.savedAppearance = (Theme.savedAppearance + 1) % 3
            return
        }
        // Taken before leave(): emptying the field empties the page's matches and the server's.
        const songs = page ? page.localSongs : null
        const from = page ? page.localFrom : ""
        if (row.kind === "song")
            results.playSong(row.id)
        leave()
        switch (row.kind) {
        case "local":
            // The whole album or playlist, from that song.
            Library.playFrom(songs, row.row, from)
            break
        case "artist":
            Navigation.artist(row.id)
            break
        case "album":
            Navigation.album(row.id)
            break
        case "playlist":
            Navigation.playlist(row.id)
            break
        case "action":
            if (row.id === "shuffle")
                Library.shuffleLibrary()
            else if (row.id === "mix")
                Library.instantMixFromPlaying()
            else if (row.id === "info")
                Navigation.info("song", Player.songId)
            else
                actionRequested(row.id)
            break
        }
    }

    onRowsChanged: current = Math.min(current, Math.max(0, rows.length - 1))
    onCurrentChanged: list.positionViewAtIndex(current, ListView.Contain)
    onQueryChanged: current = 0
    onShownChanged: {
        if (shown) {
            pointer = Qt.point(-1, -1)
            dropdown.open()
        } else {
            dropdown.close()
        }
    }
    // Another page: its own songs, and nothing left over from the last one.
    onPageChanged: input.text = ""

    // Wider while in use, and the dropdown takes the field's width: one column, as a menu under
    // its button.
    width: active ? 360 : 200
    height: 30

    Behavior on width {
        enabled: Theme.animationsOn
        NumberAnimation { duration: Theme.fastMs; easing.type: Easing.OutCubic }
    }

    SearchResults {
        id: results

        query: input.text
    }

    SongFilter {
        id: local

        songs: search.page ? search.page.localSongs : null
        text: input.text
    }

    HoverHandler {
        id: hover

        cursorShape: Qt.IBeamCursor
    }

    TapHandler {
        onTapped: input.forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: search.active || hover.hovered ? Theme.fill2 : Theme.fill
        border.width: search.active ? 1 : 0
        border.color: Theme.line
    }

    Glyph {
        x: 9
        anchors.verticalCenter: parent.verticalCenter
        name: "search"
        size: 16
        color: Theme.text2
    }

    TextInput {
        id: input

        anchors.left: parent.left
        anchors.leftMargin: 32
        anchors.right: clear.visible ? clear.left : parent.right
        anchors.rightMargin: clear.visible ? 2 : 12
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.text
        selectionColor: Theme.accent
        selectedTextColor: Theme.textOnAccent
        selectByMouse: true
        clip: true
        font.pixelSize: Theme.fsSmall
        Accessible.name: qsTr("Search")
        // Esc belongs to the field here, not to the window's Esc (which closes the side panel).
        Keys.onShortcutOverride: event => event.accepted = event.key === Qt.Key_Escape
        Keys.onEscapePressed: text !== "" ? text = "" : search.leave()
        Keys.onUpPressed: search.move(-1)
        Keys.onDownPressed: search.move(1)
        Keys.onPressed: event => {
            if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                search.move(event.key === Qt.Key_PageUp ? -8 : 8)
                event.accepted = true
            }
        }
        onAccepted: search.run(search.current)

        Text {
            anchors.fill: parent
            visible: input.text === "" && input.preeditText === ""
            text: search.active ? qsTr("Artists, albums, songs or actions") : qsTr("Search")
            color: Theme.text3
            font: input.font
            elide: Text.ElideRight
        }
    }

    IconButton {
        id: clear

        anchors.right: parent.right
        anchors.rightMargin: 3
        anchors.verticalCenter: parent.verticalCenter
        visible: input.text !== ""
        diameter: 24
        glyph: "close"
        glyphSize: 14
        tone: Theme.text2
        tip: qsTr("Clear")
        focusPolicy: Qt.NoFocus
        onClicked: {
            input.text = ""
            input.forceActiveFocus()
        }
    }

    // Under the field, as wide as it. It never takes the keyboard: typing goes on in the field. A
    // press elsewhere closes it and leaves the field.
    Popup {
        id: dropdown

        parent: search
        y: search.height + 6
        width: search.width
        padding: 0
        modal: false
        focus: false
        closePolicy: Popup.CloseOnPressOutsideParent
        // Closed by a press elsewhere: the field goes too.
        onClosed: if (search.shown) search.leave()

        enter: Transition {
            enabled: Theme.animationsOn
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.fastMs; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            enabled: Theme.animationsOn
            NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverMs }
        }

        background: Item {
            Shadow {
                anchors.fill: parent
                visible: Theme.shadowsOn
                radius: Theme.radiusCard
                offset.y: 12
                blur: 36
                color: Theme.shadow
            }

            GlassPanel {
                anchors.fill: parent
                radius: Theme.radiusCard
                rim: true
                thin: true
                source: search.windowItem
            }
        }

        contentItem: Column {
            ListView {
                id: list

                readonly property real maxHeight: Math.min(480, search.windowItem ? search.windowItem.height * 0.6 : 480)

                width: parent.width
                height: Math.min(contentHeight + topMargin + bottomMargin, maxHeight)
                visible: search.rows.length > 0
                topMargin: 6
                bottomMargin: 6
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: search.rows

                delegate: Item {
                    id: row

                    required property var modelData
                    required property int index
                    readonly property bool chosen: index === search.current
                    readonly property real headerHeight: modelData.opensGroup ? 28 : 0

                    width: list.width
                    height: headerHeight + 44

                    Text {
                        x: 16
                        y: 10
                        visible: row.modelData.opensGroup
                        text: row.modelData.group
                        color: Theme.text3
                        font.pixelSize: Theme.fsTiny
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        x: 6
                        y: row.headerHeight
                        width: parent.width - 12
                        height: 44
                        radius: Theme.radius
                        color: row.chosen ? Theme.fill2 : "transparent"

                        // Moving the mouse chooses. Qt also reports a list scrolling, or the dropdown
                        // opening, under a still pointer as a move: those leave the choice alone.
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onPositionChanged: mouse => {
                                const at = mapToItem(null, mouse.x, mouse.y)
                                const moved = search.pointer.x >= 0 && (at.x !== search.pointer.x || at.y !== search.pointer.y)
                                search.pointer = at
                                if (moved)
                                    search.current = row.index
                            }
                            onClicked: search.run(row.index)
                        }

                        Item {
                            id: lead

                            x: 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: 32
                            height: 32

                            RoundedImage {
                                anchors.fill: parent
                                visible: row.modelData.kind !== "playlist" && row.modelData.kind !== "action"
                                radius: row.modelData.kind === "artist" ? width / 2 : 5
                                coverArt: visible ? row.modelData.coverArt : ""
                            }

                            PlaylistCover {
                                anchors.fill: parent
                                visible: row.modelData.kind === "playlist"
                                active: visible
                                radius: 5
                                coverArt: visible ? row.modelData.coverArt : ""
                                name: row.modelData.title
                                hue: row.modelData.hue ?? 0
                            }

                            Glyph {
                                anchors.centerIn: parent
                                visible: row.modelData.kind === "action"
                                name: row.modelData.glyph ?? "search"
                                size: 20
                                color: Theme.text2
                            }
                        }

                        Column {
                            anchors.left: lead.right
                            anchors.leftMargin: 12
                            anchors.right: keys.left
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                width: parent.width
                                text: row.modelData.title
                                color: Theme.text
                                font.pixelSize: Theme.fs
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }

                            Text {
                                width: parent.width
                                visible: text !== ""
                                text: row.modelData.detail
                                color: Theme.text2
                                font.pixelSize: Theme.fsSmall - 1
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            id: keys

                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.modelData.keys ?? ""
                            color: Theme.text3
                            font.pixelSize: Theme.fsSmall - 1
                        }
                    }
                }
            }

            // Only once the server has answered: never while the matches are on their way.
            Text {
                width: parent.width
                visible: search.rows.length === 0 && !results.searching
                topPadding: 18
                bottomPadding: 20
                leftPadding: 16
                rightPadding: 16
                horizontalAlignment: Text.AlignHCenter
                text: results.error !== "" ? results.error : qsTr("Nothing matches “%1”").arg(search.query)
                color: Theme.text2
                font.pixelSize: Theme.fs
                elide: Text.ElideRight
            }
        }
    }
}

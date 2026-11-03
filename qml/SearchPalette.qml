pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import Legato

// The search palette (docs/design.md, "Search"), opened with Ctrl+K, / or the sidebar's Search:
// artists, albums, songs and playlists that match, then the actions. Arrows choose, Enter opens or
// plays, Esc closes. With nothing typed it lists the actions.
Popup {
    id: finder

    // An action the window carries out: "lyrics", "queue", "fullScreen" or "settings".
    signal actionRequested(string name)

    // The chosen row.
    property int current: 0
    // Where the pointer was last seen in the window; rows moving under a still pointer choose nothing.
    property point pointer: Qt.point(-1, -1)

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
    readonly property string query: input.text.trim()

    // The matches, then the actions whose name matches, each with the group it opens if it is the
    // first of one.
    readonly property var rows: {
        // Copies: a property added to a map from C++ does not stick.
        const out = query === "" ? [] : results.items.map(item => Object.assign({}, item))
        const q = query.toLowerCase()
        for (const action of actions) {
            if (q !== "" && !action.title.toLowerCase().includes(q))
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
        close()
        switch (row.kind) {
        case "artist":
            Navigation.artist(row.id)
            break
        case "album":
            Navigation.album(row.id)
            break
        case "playlist":
            Navigation.playlist(row.id)
            break
        case "song":
            results.playSong(row.id)
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
    onAboutToShow: {
        input.text = ""
        current = 0
        pointer = Qt.point(-1, -1)
    }
    onOpened: input.forceActiveFocus()

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round(parent.height * 0.12)
    width: Math.min(620, parent.width - 32)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle {
        color: Theme.dark ? Qt.rgba(0, 0, 0, 0.4) : Qt.rgba(0, 0, 0, 0.18)
    }

    enter: Transition {
        enabled: Theme.animationsOn
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.fastMs; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.98; to: 1; duration: Theme.fastMs; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        enabled: Theme.animationsOn
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverMs }
    }

    SearchResults {
        id: results

        query: input.text
    }

    background: Item {
        RectangularShadow {
            anchors.fill: parent
            visible: Theme.shadowsOn
            radius: Theme.radiusCard
            offset.y: 16
            blur: 48
            color: Theme.shadow
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusCard
            color: Theme.raised
            border.width: Theme.dark ? 1 : 0
            border.color: Theme.line
        }
    }

    contentItem: Column {
        Item {
            width: parent.width
            height: 54

            Glyph {
                x: 18
                anchors.verticalCenter: parent.verticalCenter
                name: "search"
                size: 20
                color: Theme.text3
            }

            TextInput {
                id: input

                x: 50
                width: parent.width - x - 18
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: Theme.textOnAccent
                selectByMouse: true
                clip: true
                font.pixelSize: 18
                Accessible.name: qsTr("Search")
                Keys.onUpPressed: finder.move(-1)
                Keys.onDownPressed: finder.move(1)
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                        finder.move(event.key === Qt.Key_PageUp ? -8 : 8)
                        event.accepted = true
                    }
                }
                onAccepted: finder.run(finder.current)

                Text {
                    anchors.fill: parent
                    visible: input.text === "" && input.preeditText === ""
                    text: qsTr("Artists, albums, songs or actions")
                    color: Theme.text3
                    font: input.font
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.line
        }

        ListView {
            id: list

            readonly property real maxHeight: Math.min(440, finder.parent.height * 0.55)

            width: parent.width
            height: Math.min(contentHeight + topMargin + bottomMargin, maxHeight)
            visible: finder.rows.length > 0
            topMargin: 6
            bottomMargin: 6
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: finder.rows

            delegate: Item {
                id: row

                required property var modelData
                required property int index
                readonly property bool chosen: index === finder.current
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
                    id: body

                    x: 6
                    y: row.headerHeight
                    width: parent.width - 12
                    height: 44
                    radius: Theme.radius
                    color: row.chosen ? Theme.fill2 : "transparent"

                    // Moving the mouse chooses. Qt also reports a list scrolling, or the palette
                    // opening, under a still pointer as a move: those leave the choice alone.
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onPositionChanged: mouse => {
                            const at = mapToItem(null, mouse.x, mouse.y)
                            const moved = finder.pointer.x >= 0 && (at.x !== finder.pointer.x || at.y !== finder.pointer.y)
                            finder.pointer = at
                            if (moved)
                                finder.current = row.index
                        }
                        onClicked: finder.run(row.index)
                    }

                    Item {
                        id: lead

                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: 32
                        height: 32

                        RoundedImage {
                            anchors.fill: parent
                            visible: row.modelData.kind === "artist" || row.modelData.kind === "album"
                                || row.modelData.kind === "song"
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
            visible: finder.rows.length === 0 && finder.query !== "" && !results.searching
            topPadding: 22
            bottomPadding: 24
            horizontalAlignment: Text.AlignHCenter
            text: results.error !== "" ? results.error : qsTr("Nothing matches “%1”").arg(finder.query)
            color: Theme.text2
            font.pixelSize: Theme.fs
            elide: Text.ElideRight
        }
    }
}

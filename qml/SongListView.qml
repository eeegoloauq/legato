pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import Legato

// A page's list of songs (docs/design.md, "Song lists"), with the page's top as its header. A click
// selects a row; ↑/↓ move, Enter plays, Shift+F10 or the Menu key opens the song menu, Ctrl+I gets
// info. Declare a WheelScroll after it that calls pointerMoved().
//
// With `searchPage` set, the header's search finds in these songs first (ContentPage.localSongs).
ListView {
    id: list

    required property SongModel songs
    // "Playing from" in Up Next.
    property string from
    property string mode: "list"
    property string albumArtist
    // A playlist of the user's: its songs' menus offer Remove from Playlist.
    property string playlistId
    property int selected: -1
    property ContentPage searchPage: null

    function play(row) {
        Library.playFrom(songs, row, from)
    }

    function pointerMoved() {
        revealGlide.stop()
    }

    // Scrolls the selected row into the part of the view the bars leave free.
    function reveal() {
        const item = itemAtIndex(selected)
        if (!item) {
            positionViewAtIndex(selected, ListView.Contain)
            return
        }
        const top = item.y - Theme.headerHeight - 8
        const bottom = item.y + item.height - (height - Theme.barHeight - 32)
        const first = originY
        const last = Math.max(first, originY + contentHeight + bottomMargin - height)
        let y = contentY
        if (y > top)
            y = top
        else if (y < bottom)
            y = bottom
        y = Math.max(first, Math.min(y, last))
        if (Math.abs(y - contentY) < 1)
            return
        revealGlide.to = y
        revealGlide.restart()
    }

    anchors.fill: parent
    anchors.leftMargin: Theme.gutter - 12
    anchors.rightMargin: Theme.gutter - 12
    bottomMargin: Theme.barHeight + 56
    model: songs
    reuseItems: true
    cacheBuffer: 600
    boundsBehavior: Flickable.StopAtBounds
    pixelAligned: true
    focus: true
    keyNavigationEnabled: false
    onCountChanged: if (selected >= count) selected = -1
    Binding {
        when: list.searchPage !== null
        target: list.searchPage
        property: "localSongs"
        value: list.songs
    }
    Binding {
        when: list.searchPage !== null
        target: list.searchPage
        property: "localFrom"
        value: list.from
    }

    // Ctrl+I is the window's Get info for the playing song; with a row chosen, it is that row's.
    Keys.onShortcutOverride: event => {
        if (event.key === Qt.Key_I && event.modifiers === Qt.ControlModifier && selected >= 0)
            event.accepted = true
    }
    Keys.onPressed: event => {
        if (count === 0)
            return
        const plain = (event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier
        if (event.key === Qt.Key_I && event.modifiers === Qt.ControlModifier) {
            if (selected >= 0)
                Navigation.info("song", songs.idAt(selected))
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
            if (selected < 0)
                return
            const item = itemAtIndex(selected) as SongRow
            if (item)
                item.openMenu(item, 60, item.height)
            event.accepted = true
            return
        }
        if (!plain)
            return
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (selected >= 0)
                play(selected)
            event.accepted = true
            return
        }
        const rows = Math.max(1, Math.floor((height - Theme.headerHeight - Theme.barHeight) / 52))
        const steps = { [Qt.Key_Down]: 1, [Qt.Key_Up]: -1, [Qt.Key_PageDown]: rows, [Qt.Key_PageUp]: -rows }
        let next
        if (event.key === Qt.Key_Home)
            next = 0
        else if (event.key === Qt.Key_End)
            next = count - 1
        else if (steps[event.key] !== undefined)
            // The first key starts at the first row in view.
            next = selected < 0 ? Math.max(0, indexAt(10, contentY + Theme.headerHeight + 20)) : selected + steps[event.key]
        else
            return
        event.accepted = true
        selected = Math.max(0, Math.min(next, count - 1))
        reveal()
    }

    delegate: SongRow {
        songs: list.songs
        mode: list.mode
        albumArtist: list.albumArtist
        playlistId: list.playlistId
        selected: index === list.selected
        onSelectRequested: {
            list.selected = index
            list.forceActiveFocus()
        }
        onPlayRequested: list.play(index)
    }

    NumberAnimation {
        id: revealGlide

        target: list
        property: "contentY"
        duration: Theme.normalMs
        easing.type: Easing.OutCubic
    }

    T.ScrollBar.vertical: Scroller {}
}

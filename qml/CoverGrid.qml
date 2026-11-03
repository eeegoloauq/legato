import QtQuick
import QtQuick.Templates as T
import Legato

// A page's grid of covers (docs/design.md, "Covers and carousels"): as many columns of at least
// Theme.coverMin as fit, never more than Theme.maxColumns. Arrows, Home, End and Page Up/Down choose
// a cover (delegates show a ring when `keyFocus`), Enter activates it. Declare a WheelScroll after it
// that calls pointerMoved().
GridView {
    id: grid

    // The view is a gap wider than the covers (see the anchors below).
    readonly property int columns: Theme.columnsFor(width - Theme.columnGap)
    readonly property real coverSize: cellWidth - Theme.columnGap
    // What is under each cover: two title lines and the subtitle line.
    property real textHeight: 8 + 40 + 18
    // The cover chosen with the arrow keys, -1 before any key. Kept apart from GridView's
    // currentIndex, whose own scrolling ignores the header and the player bar over the view.
    property int keyIndex: -1
    property bool keyed: false

    signal activated(int index)

    function keyFocusOn(index) {
        return keyed && activeFocus && index === keyIndex
    }

    // The pointer took over; the ring comes back with the next arrow key.
    function pointerMoved() {
        revealGlide.stop()
        keyed = false
    }

    // Scrolls the chosen cover into the part of the view the bars leave free.
    function reveal() {
        const item = itemAtIndex(keyIndex)
        if (!item) {
            positionViewAtIndex(keyIndex, GridView.Contain)
            return
        }
        const top = item.y - Theme.headerHeight - 16
        const bottom = item.y + cellHeight - Theme.rowGap - (height - Theme.barHeight - 32)
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

    // GridView fits whole cells into its width and ignores margins for that, so the gutters are
    // anchor margins; the right one leaves room for the last column's gap.
    anchors.fill: parent
    anchors.leftMargin: Theme.gutter
    anchors.rightMargin: Theme.gutter - Theme.columnGap
    cellWidth: Math.floor(width / columns)
    cellHeight: coverSize + textHeight + Theme.rowGap
    bottomMargin: Theme.barHeight + 56
    // Covers a little beyond the view load before they scroll in.
    cacheBuffer: cellHeight * 2
    reuseItems: true
    boundsBehavior: Flickable.StopAtBounds
    pixelAligned: true
    focus: true
    keyNavigationEnabled: false

    Keys.onPressed: event => {
        if (count === 0 || (event.modifiers & ~Qt.KeypadModifier) !== Qt.NoModifier)
            return
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (keyIndex >= 0)
                activated(keyIndex)
            event.accepted = true
            return
        }
        const rows = Math.max(1, Math.floor((height - Theme.headerHeight - Theme.barHeight) / cellHeight))
        const steps = {
            [Qt.Key_Right]: 1, [Qt.Key_Left]: -1, [Qt.Key_Down]: columns, [Qt.Key_Up]: -columns,
            [Qt.Key_PageDown]: columns * rows, [Qt.Key_PageUp]: -columns * rows
        }
        let next
        if (event.key === Qt.Key_Home)
            next = 0
        else if (event.key === Qt.Key_End)
            next = count - 1
        else if (steps[event.key] !== undefined)
            // The first key shows where the choice starts: the first cover in view.
            next = keyIndex < 0 || !keyed
                ? Math.max(0, indexAt(10, contentY + Theme.headerHeight + 20))
                : keyIndex + steps[event.key]
        else
            return
        event.accepted = true
        keyed = true
        keyIndex = Math.max(0, Math.min(next, count - 1))
        reveal()
    }

    NumberAnimation {
        id: revealGlide

        target: grid
        property: "contentY"
        duration: Theme.normalMs
        easing.type: Easing.OutCubic
    }

    T.ScrollBar.vertical: Scroller {}
}

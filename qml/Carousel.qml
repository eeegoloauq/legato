pragma ComponentBehavior: Bound

import QtQuick
import Legato

// A titled row of covers that scrolls sideways (docs/design.md, "Covers and carousels"): as many
// covers as a grid of the same width has columns, with part of the next one showing. No scroll bar: arrow buttons show
// at the ends under the pointer, and a touchpad scrolls it sideways. Delegates take
// `coverSize: carousel.coverSize`.
Column {
    id: carousel

    property alias title: heading.text
    property alias opens: heading.opens
    property alias model: list.model
    property alias delegate: list.delegate
    // Beside the title, such as Random albums' refresh button.
    default property alias extra: extras.data
    // The grid's columns and 0.4 of the next cover, which shows there is more.
    readonly property int columns: Theme.columnsFor(width)
    readonly property real coverSize: Math.floor((width + Theme.columnGap) / (columns + 0.4) - Theme.columnGap)
    // What is under each cover.
    property real textHeight: 66

    signal opened()

    spacing: 14

    Row {
        spacing: 10

        SectionTitle {
            id: heading

            onClicked: carousel.opened()
        }

        Row {
            id: extras

            anchors.verticalCenter: heading.verticalCenter
            spacing: 4
        }
    }

    // To the window's edges, so covers slide out of view there instead of at the gutter. The arrows
    // sit in the gutters inside this item, so the pointer on an arrow still counts as over the
    // carousel (when they were outside it, an arrow hid under the pointer). A Column sets only y.
    Item {
        x: -Theme.gutter
        width: parent.width + 2 * Theme.gutter
        height: carousel.coverSize + carousel.textHeight

        HoverHandler {
            id: hover
        }

        ListView {
            id: list

            anchors.fill: parent
            leftMargin: Theme.gutter
            rightMargin: Theme.gutter
            orientation: ListView.Horizontal
            spacing: Theme.columnGap
            boundsBehavior: Flickable.StopAtBounds
            pixelAligned: true
            reuseItems: true
            // Covers a page beyond the end load before they slide in.
            cacheBuffer: Math.max(0, width)

            // One view's width at a time, less a cover, so the one at the edge stays in view.
            function page(direction) {
                const step = Math.max(carousel.coverSize, width - 2 * Theme.gutter - carousel.coverSize)
                const first = originX - leftMargin
                const last = Math.max(first, originX + contentWidth + rightMargin - width)
                glide.to = Math.max(first, Math.min(contentX + direction * step, last))
                glide.restart()
            }

            NumberAnimation {
                id: glide

                target: list
                property: "contentX"
                duration: Theme.normalMs * 1.5
                easing.type: Easing.OutCubic
            }
        }

        IconButton {
            x: Theme.gutter / 2 - width / 2 + 2
            y: carousel.coverSize / 2 - height / 2
            diameter: 32
            glyph: "chevron_left"
            tone: Theme.text
            restFill: Theme.raised
            tip: qsTr("Back")
            visible: hover.hovered && !list.atXBeginning
            onClicked: list.page(-1)
        }

        IconButton {
            x: parent.width - Theme.gutter / 2 - width / 2 - 2
            y: carousel.coverSize / 2 - height / 2
            diameter: 32
            glyph: "chevron_right"
            tone: Theme.text
            restFill: Theme.raised
            tip: qsTr("More")
            visible: hover.hovered && !list.atXEnd
            onClicked: list.page(1)
        }
    }
}

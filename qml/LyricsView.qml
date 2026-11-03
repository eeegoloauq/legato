pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Legato

// The current song's lyrics (docs/design.md, "Lyrics"), in the side panel and the full-screen
// player. Synced lyrics follow the song: the sung line is bright, earlier lines dimmer than later
// ones, a click on a line seeks there, and "• • •" stands for an instrumental gap. A wheel or drag
// stops the following for a few seconds so the viewer can read ahead.
Item {
    id: root

    property color textColor: Theme.text
    property int lineSize: 20
    property int gap: 14
    // Where the sung line rests, as a share of the height from the top.
    property real anchor: 1 / 3

    readonly property bool empty: Player.lyrics.length === 0
    // The line being sung: the last one that has started. -1 before the first, or unsynced.
    readonly property int current: {
        const lines = Player.lyrics
        if (!Player.lyricsSynced || lines.length === 0)
            return -1
        const t = Player.position + 0.15
        let lo = 0, hi = lines.length - 1, found = -1
        while (lo <= hi) {
            const mid = (lo + hi) >> 1
            if (lines[mid].time <= t) {
                found = mid
                lo = mid + 1
            } else {
                hi = mid - 1
            }
        }
        return found
    }
    property double heldUntil: 0

    function follow(animated) {
        if (!visible || current < 0 || Date.now() < heldUntil)
            return
        const item = list.itemAtIndex(current)
        if (!item) {
            list.positionViewAtIndex(current, ListView.Center)
            return
        }
        const top = list.originY - list.topMargin
        const bottom = Math.max(top, list.originY + list.contentHeight + list.bottomMargin - list.height)
        const target = Math.max(top, Math.min(item.y - list.height * anchor, bottom))
        if (animated && Theme.animationsOn) {
            follower.to = target
            follower.restart()
        } else {
            list.contentY = target
        }
    }

    onCurrentChanged: follow(true)
    onVisibleChanged: if (visible) Qt.callLater(() => follow(false))
    onEmptyChanged: if (!empty) Qt.callLater(() => follow(false))

    ListView {
        id: list

        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        topMargin: 18
        bottomMargin: height * 0.5
        spacing: root.gap
        model: Player.lyrics
        onMovementStarted: {
            follower.stop()
            root.heldUntil = Date.now() + 4000
        }

        delegate: T.Button {
            id: line

            required property var modelData
            required property int index
            readonly property bool synced: modelData.time >= 0

            width: list.width
            // Templates controls have no implicit size of their own.
            implicitHeight: implicitContentHeight + topPadding + bottomPadding
            enabled: synced
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            onClicked: {
                root.heldUntil = 0
                Player.seek(line.modelData.time)
            }

            contentItem: Text {
                text: line.modelData.text !== "" ? line.modelData.text : "• • •"
                color: root.textColor
                font.pixelSize: line.synced ? root.lineSize : Math.round(root.lineSize * 0.85)
                font.weight: line.synced ? Font.Bold : Font.Medium
                font.letterSpacing: line.modelData.text !== "" ? -0.01 * root.lineSize : 4
                wrapMode: Text.Wrap
                opacity: !line.synced ? 0.85
                    : line.index === root.current ? 1
                    : line.hovered ? 0.55
                    : line.index < root.current ? 0.38 : 0.28

                Behavior on opacity {
                    NumberAnimation { duration: Theme.normalMs }
                }
            }

            background: Item {}

            HoverHandler {
                cursorShape: line.synced ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        T.ScrollBar.vertical: Scroller { underHeader: false }
    }

    WheelScroll {
        flickable: list
        onMoved: {
            follower.stop()
            root.heldUntil = Date.now() + 4000
        }
    }

    NumberAnimation {
        id: follower

        target: list
        property: "contentY"
        duration: 420
        easing.type: Easing.OutCubic
    }
}

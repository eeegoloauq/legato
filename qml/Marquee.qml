pragma ComponentBehavior: Bound

import QtQuick
import Legato

// One line of text that, when it does not fit, glides left until its end shows, rests, and glides
// back: back and forth rather than a loop that wraps around, as asked.
//
// What makes it calm rather than busy, from players that do this well:
// - it rests 2.5 s before the first move, so a new song's title reads first;
// - it moves at a steady reading speed (30 px/s) with soft starts and stops, so a long title takes
//   longer instead of racing;
// - the edge it scrolls past fades out instead of cutting letters in half;
// - text that fits never moves, and with animations off the line is elided instead.
Item {
    id: root

    property alias text: label.text
    property alias color: label.color
    property alias font: label.font
    // Rich text for links (<a href>); a click on one says linkActivated.
    property alias textFormat: label.textFormat
    readonly property alias hoveredLink: label.hoveredLink

    signal linkActivated(string link)
    // Pixels per second.
    property real speed: 30
    property int rest: 2500

    readonly property real overflow: Math.max(0, Math.ceil(label.implicitWidth - width))
    readonly property bool scrolls: overflow > 0 && Theme.animationsOn && visible
    readonly property int travel: Math.max(500, overflow / speed * 1000)

    implicitWidth: label.implicitWidth
    implicitHeight: label.implicitHeight
    clip: true

    function reset() {
        run.stop()
        label.x = 0
        if (scrolls)
            run.start()
    }

    onTextChanged: reset()
    onOverflowChanged: reset()
    onScrollsChanged: reset()

    // Always its full width, so it never has to be laid out again when it starts or stops
    // scrolling (toggling elide and width with the overflow left it drawn at a stale width); it
    // is elided only when animations are off.
    Text {
        id: label

        width: Theme.animationsOn ? implicitWidth : root.width
        elide: Theme.animationsOn ? Text.ElideNone : Text.ElideRight
        maximumLineCount: 1
        onLinkActivated: link => root.linkActivated(link)
    }

    HoverHandler {
        cursorShape: label.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    SequentialAnimation {
        id: run

        loops: Animation.Infinite

        PauseAnimation { duration: root.rest }
        NumberAnimation {
            target: label
            property: "x"
            from: 0
            to: -root.overflow
            duration: root.travel
            easing.type: Easing.InOutSine
        }
        PauseAnimation { duration: root.rest - 500 }
        NumberAnimation {
            target: label
            property: "x"
            from: -root.overflow
            to: 0
            duration: root.travel
            easing.type: Easing.InOutSine
        }
    }

    layer.enabled: root.scrolls
    layer.effect: ShaderEffect {
        // 0 to 1 over the first and last few pixels of travel, so the fade comes in with the move.
        readonly property real fadeLeft: Math.min(1, -label.x / 8)
        readonly property real fadeRight: Math.min(1, (root.overflow + label.x) / 8)
        readonly property real edge: Math.min(0.25, 14 / Math.max(1, root.width))

        fragmentShader: "qrc:/qt/qml/Legato/shaders/fade.frag.qsb"
    }
}

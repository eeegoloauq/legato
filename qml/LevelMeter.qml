pragma ComponentBehavior: Bound

import QtQuick
import Legato

// The playing row's small level meter (docs/design.md, "Song lists"): three bars that bounce while
// the song plays and rest while it is paused.
Row {
    id: meter

    property color color: Theme.tint
    property bool running: Player.playing

    spacing: 2
    height: 12

    Repeater {
        model: [430, 560, 370]

        delegate: Rectangle {
            id: bar

            required property int modelData
            required property int index

            anchors.bottom: parent.bottom
            width: 3
            radius: 1
            color: meter.color
            height: meter.height * [0.55, 0.9, 0.4][index]

            SequentialAnimation on height {
                running: meter.running && Theme.animationsOn
                loops: Animation.Infinite
                alwaysRunToEnd: false
                NumberAnimation { to: meter.height; duration: bar.modelData / 2; easing.type: Easing.InOutSine }
                NumberAnimation { to: meter.height * 0.25; duration: bar.modelData / 2; easing.type: Easing.InOutSine }
            }
        }
    }
}

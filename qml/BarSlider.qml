import QtQuick
import QtQuick.Controls.Basic
import Legato

// A thin slider for the song position and the volume: a rounded
// track filled up to the value, and a white knob that shows under the pointer or while dragging.
// The knob is centered on the value and may overhang the ends of the track.
Slider {
    id: slider

    property color fillColor: Theme.tint
    property color trackColor: Theme.fill2
    property real trackHeight: 3
    readonly property bool active: hovered || pressed || visualFocus

    implicitWidth: 120
    implicitHeight: 16
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    background: Item {
        x: slider.leftPadding
        y: slider.topPadding + (slider.availableHeight - height) / 2
        width: slider.availableWidth
        height: slider.trackHeight

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: slider.trackColor
        }

        Rectangle {
            width: slider.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: slider.fillColor
        }
    }

    handle: Rectangle {
        x: slider.leftPadding + slider.visualPosition * slider.availableWidth - width / 2
        y: slider.topPadding + (slider.availableHeight - height) / 2
        width: 12
        height: 12
        radius: 6
        color: "white"
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.18)
        opacity: slider.active ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.fastMs }
        }
    }
}

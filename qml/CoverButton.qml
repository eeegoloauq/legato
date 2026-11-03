import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import Legato

// The round play or "…" button over a cover under the pointer.
Button {
    id: button

    property string glyph
    property bool shown: false
    // False until made, so a button made under the pointer fades in like the others.
    property bool made: false
    // Play is the cover's main action: larger than "…", and dark enough to show on a pale cover.
    readonly property bool primary: glyph === "play_arrow"

    width: primary ? 40 : 32
    height: width
    padding: 0
    visible: opacity > 0
    opacity: made && (shown || hovered || visualFocus) ? 1 : 0
    focusPolicy: Qt.TabFocus
    display: AbstractButton.IconOnly
    icon.source: Theme.icon(glyph)
    icon.width: primary ? 22 : 18
    icon.height: primary ? 22 : 18
    icon.color: "white"

    Behavior on opacity {
        NumberAnimation { duration: Theme.hoverMs }
    }

    Component.onCompleted: made = true

    background: Item {
        RectangularShadow {
            anchors.fill: parent
            visible: Theme.shadowsOn
            radius: height / 2
            offset.y: 2
            blur: 10
            color: Qt.rgba(0, 0, 0, 0.3)
        }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Qt.rgba(24 / 255, 24 / 255, 28 / 255, button.down ? 0.85 : button.hovered ? 0.78 : 0.62)
            border.width: button.visualFocus ? 2 : 0
            border.color: "white"
        }
    }
}

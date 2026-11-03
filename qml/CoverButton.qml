import QtQuick
import QtQuick.Controls.Basic
import Legato

// The round play or "…" button over a cover under the pointer.
Button {
    id: button

    property string glyph
    property bool shown: false

    width: 32
    height: 32
    padding: 0
    visible: opacity > 0
    opacity: shown || hovered || visualFocus ? 1 : 0
    focusPolicy: Qt.TabFocus
    display: AbstractButton.IconOnly
    icon.source: Theme.icon(glyph)
    icon.width: 18
    icon.height: 18
    icon.color: "white"

    Behavior on opacity {
        NumberAnimation { duration: Theme.hoverMs }
    }

    background: Rectangle {
        radius: height / 2
        color: Qt.rgba(30 / 255, 30 / 255, 36 / 255, button.down ? 0.72 : button.hovered ? 0.62 : 0.42)
        border.width: button.visualFocus ? 2 : 0
        border.color: "white"
    }
}

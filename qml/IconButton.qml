import QtQuick
import QtQuick.Controls.Basic
import Legato

// An icon in a circle that shows under the pointer, with its words in the tooltip (as in lumeo).
// The icon is one of icons/, tinted [tone] by the button. Hover and press lay the icon's own color,
// faint, over the resting fill, so the change shows on any fill and any background.
ToolButton {
    id: button

    property string glyph
    property real glyphSize: 18
    property real diameter: 30
    property color tone: Theme.text
    // The circle at rest; most buttons have none.
    property color restFill: "transparent"
    property string tip
    // A press puts the tooltip away until the pointer leaves (it would sit beside the menu it opened).
    property bool tipDismissed: false

    implicitWidth: diameter
    implicitHeight: diameter
    padding: 0
    focusPolicy: Qt.TabFocus
    display: AbstractButton.IconOnly
    icon.source: Theme.icon(glyph)
    icon.width: glyphSize
    icon.height: glyphSize
    icon.color: tone
    opacity: enabled ? 1 : 0.4
    Accessible.name: tip
    ToolTip.text: tip
    ToolTip.visible: hovered && tip !== "" && !tipDismissed
    onPressed: tipDismissed = true
    onHoveredChanged: if (!hovered) tipDismissed = false

    background: Rectangle {
        radius: height / 2
        color: button.restFill
        border.width: button.visualFocus ? 2 : 0
        border.color: Theme.accent

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Qt.alpha(button.tone, button.down ? 0.24 : 0.14)
            opacity: button.hovered || button.down ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: Theme.hoverMs }
            }
        }
    }
}

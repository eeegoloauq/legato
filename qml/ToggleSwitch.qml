import QtQuick
import QtQuick.Templates as T
import Legato

// On or off (docs/design.md, "Controls"): a pill in the accent color with the knob at the right when
// on, a faint pill with a white knob at the left when off. Named so it does not hide Controls' Switch.
T.Switch {
    id: control

    implicitWidth: 40
    implicitHeight: 24
    padding: 0
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.CheckBox

    indicator: Rectangle {
        width: control.width
        height: control.height
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.fill2
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.text2

        Behavior on color {
            enabled: Theme.animationsOn
            ColorAnimation { duration: Theme.fastMs }
        }

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: parent.height - 6
            height: width
            radius: width / 2
            color: control.checked ? Theme.textOnAccent : "white"
            border.width: control.checked ? 0 : 1
            border.color: Qt.rgba(0, 0, 0, 0.12)

            Behavior on x {
                enabled: Theme.animationsOn
                NumberAnimation { duration: Theme.fastMs; easing.type: Easing.OutCubic }
            }
        }
    }

    contentItem: Item {}
    // Pressed, it dims a little, as a button does.
    opacity: enabled ? (control.down ? 0.85 : 1) : 0.4
}

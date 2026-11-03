pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Legato

// One choice of several: a button with the current choice and a chevron; a click opens the choices
// under it as a menu, the current one checked (docs/design.md, "Controls").
Button {
    id: control

    required property list<string> labels
    property int current: 0
    // Before the choice on the button ("Sort by"), where the row around it does not say it.
    property string prefix

    signal chosen(int index)

    implicitHeight: 30
    leftPadding: 12
    rightPadding: 8
    font.pixelSize: Theme.fs
    text: (prefix !== "" ? prefix + " " : "") + (labels[current] ?? "")
    Accessible.role: Accessible.ComboBox
    onClicked: menu.visible ? menu.close() : menu.open()

    contentItem: Row {
        spacing: 4

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: Theme.text
        }

        Glyph {
            anchors.verticalCenter: parent.verticalCenter
            name: "expand_more"
            size: 18
            color: Theme.text2
        }
    }

    background: Rectangle {
        radius: 9
        color: Theme.fill2
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.text

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.text
            opacity: control.down || menu.visible ? 0.12 : control.hovered ? 0.06 : 0
        }
    }

    PopupMenu {
        id: menu

        x: control.width - width
        y: control.height + 4
        width: Math.max(control.width, 180)

        Instantiator {
            model: control.labels

            delegate: Action {
                id: choice

                required property int index
                required property string modelData

                text: modelData
                checkable: true
                checked: index === control.current
                onTriggered: {
                    control.chosen(index)
                    // Triggering toggled it, which broke the binding.
                    choice.checked = Qt.binding(() => choice.index === control.current)
                }
            }

            onObjectAdded: (index, object) => menu.insertAction(index, object as Action)
            onObjectRemoved: (index, object) => menu.removeAction(object as Action)
        }
    }
}

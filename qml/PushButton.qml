import QtQuick
import QtQuick.Controls.Basic
import Legato

// A text button: on the accent color when `primary`, on a faint fill otherwise.
// With a `glyph`, the icon goes before the text.
Button {
    id: button

    property bool primary: false
    property string glyph
    // The text's color; red for an action that cannot be undone.
    property color tone: primary ? Theme.textOnAccent : Theme.text

    implicitHeight: 34
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: Theme.fs
    font.weight: Font.Medium
    opacity: enabled ? 1 : 0.5

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: label.implicitHeight

        Row {
            id: row

            anchors.centerIn: parent
            spacing: 7

            Glyph {
                anchors.verticalCenter: parent.verticalCenter
                visible: button.glyph !== ""
                name: button.glyph || "play_arrow"
                size: 18
                color: label.color
            }

            Text {
                id: label

                anchors.verticalCenter: parent.verticalCenter
                text: button.text
                font: button.font
                color: button.tone
            }
        }
    }

    background: Rectangle {
        radius: 9
        color: button.primary ? Theme.accent : Theme.fill2
        border.width: button.visualFocus ? 2 : 0
        border.color: Theme.text

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: button.primary ? "black" : Theme.text
            opacity: button.down ? 0.16 : button.hovered ? 0.08 : 0
        }
    }
}

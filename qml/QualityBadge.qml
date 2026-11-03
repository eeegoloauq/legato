import QtQuick
import Legato

// "Lossless", "Hi-Res Lossless", "MP3 320" in a thin outline.
Rectangle {
    property alias text: label.text

    visible: label.text !== ""
    implicitWidth: label.implicitWidth + 8
    implicitHeight: 16
    radius: 4
    color: "transparent"
    border.width: 1
    border.color: Theme.line

    Text {
        id: label

        anchors.centerIn: parent
        color: Theme.text2
        font.pixelSize: 10
        font.weight: Font.DemiBold
        font.letterSpacing: 0.3
    }
}

import QtQuick
import Legato

// A short confirmation over the player bar ("Added to Road Trip"), from Library.notice. It shows
// for a moment and fades; a new one replaces it.
Item {
    id: notice

    required property Item source
    property string text

    function show(text) {
        notice.text = text
        hold.restart()
    }

    width: label.implicitWidth + 32
    height: 36
    opacity: hold.running ? 1 : 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: Theme.normalMs; easing.type: Easing.OutCubic }
    }

    Timer {
        id: hold

        interval: 2400
    }

    GlassPanel {
        anchors.fill: parent
        source: notice.source
        radius: height / 2
        rim: true
        thin: true
    }

    Text {
        id: label

        anchors.centerIn: parent
        text: notice.text
        color: Theme.text
        font.pixelSize: Theme.fs
        font.weight: Font.Medium
    }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: text
}

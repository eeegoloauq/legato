import QtQuick
import QtQuick.Controls.Basic
import Legato

// The content area while the active server does not answer (docs/design.md, "Sign-in and
// servers"): the name, the address, Try Again, and a way to another server only when one answers.
Rectangle {
    color: Theme.bg

    // Nothing under it reacts; the wheel has nothing to scroll.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Theme.barHeight / 2
        width: Math.min(parent.width - 2 * Theme.gutter, 440)
        spacing: 10

        Text {
            width: parent.width
            text: qsTr("Can't reach %1").arg(Session.activeName)
            color: Theme.text
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: 24
            font.weight: Font.Bold
        }

        Text {
            width: parent.width
            text: qsTr("%1 is not answering.").arg(Session.activeHost)
            color: Theme.text2
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fs
            bottomPadding: 8
        }

        PushButton {
            anchors.horizontalCenter: parent.horizontalCenter
            primary: true
            text: Session.busy ? qsTr("Trying…") : qsTr("Try Again")
            enabled: !Session.busy
            onClicked: Session.retry()
        }

        AbstractButton {
            id: other

            anchors.horizontalCenter: parent.horizontalCenter
            visible: Session.fallbackId !== ""
            topPadding: 6
            bottomPadding: 6
            onClicked: Session.switchTo(Session.fallbackId)

            contentItem: Row {
                spacing: 7

                StatusDot {
                    anchors.verticalCenter: parent.verticalCenter
                    status: "up"
                }

                Text {
                    text: qsTr("%1 is answering. Switch to %1").arg(Session.fallbackName)
                    color: Theme.accent
                    font.pixelSize: Theme.fsSmall
                    font.underline: other.hovered
                }
            }
        }
    }
}

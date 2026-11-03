import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import Legato

// A small card over the dimmed window that asks for a name (New Playlist, Rename) or confirms
// something that cannot be undone (Delete Playlist). Enter confirms, Esc or a click outside cancels.
// Navigation.askName and Navigation.confirm open it from anywhere.
Popup {
    id: dialog

    property string title
    property string detail
    property string action
    // Asks for a name: a text field, and the action needs some text.
    property bool asksName: false
    // The action cannot be undone: it is in red.
    property bool destructive: false
    property var done: null

    function askName(title, action, initial, done) {
        dialog.title = title
        dialog.detail = ""
        dialog.action = action
        dialog.asksName = true
        dialog.destructive = false
        dialog.done = done
        field.text = initial
        open()
    }

    function confirm(title, detail, action, done) {
        dialog.title = title
        dialog.detail = detail
        dialog.action = action
        dialog.asksName = false
        dialog.destructive = true
        dialog.done = done
        open()
    }

    function accept() {
        const name = field.text.trim()
        if (asksName && name === "")
            return
        const callback = done
        done = null
        close()
        if (callback)
            callback(name)
    }

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(380, parent.width - 32)
    padding: 20
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: {
        if (asksName) {
            field.forceActiveFocus()
            field.selectAll()
        } else {
            confirmButton.forceActiveFocus()
        }
    }
    onClosed: done = null

    Overlay.modal: Rectangle {
        color: Theme.dark ? Qt.rgba(0, 0, 0, 0.4) : Qt.rgba(0, 0, 0, 0.18)
    }

    enter: Transition {
        enabled: Theme.animationsOn
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.fastMs; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.98; to: 1; duration: Theme.fastMs; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        enabled: Theme.animationsOn
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverMs }
    }

    background: Item {
        RectangularShadow {
            anchors.fill: parent
            visible: Theme.shadowsOn
            radius: Theme.radiusCard
            offset.y: 16
            blur: 48
            color: Theme.shadow
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusCard
            color: Theme.raised
            border.width: Theme.dark ? 1 : 0
            border.color: Theme.line
        }
    }

    contentItem: Column {
        spacing: 16

        Text {
            width: parent.width
            text: dialog.title
            color: Theme.text
            font.pixelSize: Theme.fs + 2
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }

        Text {
            width: parent.width
            visible: dialog.detail !== ""
            text: dialog.detail
            color: Theme.text2
            font.pixelSize: Theme.fs
            wrapMode: Text.Wrap
        }

        TextField {
            id: field

            width: parent.width
            visible: dialog.asksName
            implicitHeight: 34
            leftPadding: 10
            rightPadding: 10
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.textOnAccent
            font.pixelSize: Theme.fs
            Accessible.name: dialog.title
            onAccepted: dialog.accept()

            background: Rectangle {
                radius: 8
                color: Theme.fill
                border.width: field.activeFocus ? 2 : 1
                border.color: field.activeFocus ? Theme.accent : Theme.line
            }
        }

        Row {
            anchors.right: parent.right
            spacing: 8

            PushButton {
                text: qsTr("Cancel")
                onClicked: dialog.close()
            }

            PushButton {
                id: confirmButton

                primary: !dialog.destructive
                text: dialog.action
                enabled: !dialog.asksName || field.text.trim() !== ""
                tone: dialog.destructive ? Theme.red : Theme.textOnAccent
                onClicked: dialog.accept()
                Keys.onReturnPressed: dialog.accept()
                Keys.onEnterPressed: dialog.accept()
            }
        }
    }
}

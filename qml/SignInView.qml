pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Legato

// The sign-in card (docs/design.md, "Sign-in and servers"): the whole window, without the sidebar
// or the player bar. Saved servers, if any, are listed under the card to switch to.
Rectangle {
    id: view

    function submit() {
        if (!Session.busy)
            Session.signIn(address.text, username.text, password.text, keep.checked)
    }

    function focusFirstField() {
        (address.text === "" ? address : username.text === "" ? username : password).input.forceActiveFocus()
    }

    color: Theme.bg
    Keys.onEscapePressed: Session.cancelSignIn()

    // The window has no title bar here either: the background moves it, a double click maximizes it.
    WindowDragArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    WindowControls {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 16
        anchors.topMargin: (Theme.headerHeight - height) / 2
    }

    component Field: Column {
        id: field

        property alias label: caption.text
        property alias text: input.text
        property alias echoMode: input.echoMode
        property alias input: input

        width: parent ? parent.width : 0
        spacing: 5

        Text {
            id: caption

            color: Theme.text2
            font.pixelSize: Theme.fsTiny + 1
            font.weight: Font.DemiBold
        }

        TextField {
            id: input

            width: parent.width
            implicitHeight: 34
            leftPadding: 10
            rightPadding: 10
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.textOnAccent
            font.pixelSize: Theme.fs
            Accessible.name: caption.text
            onAccepted: view.submit()

            background: Rectangle {
                radius: 8
                color: Theme.fill
                border.width: input.activeFocus ? 2 : 1
                border.color: input.activeFocus ? Theme.accent : Theme.line
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 22

        Item {
            width: 380
            height: form.implicitHeight + 52

            Shadow {
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
                border.width: 1
                border.color: Theme.line
            }

            Column {
                id: form

                x: 28
                y: 28
                width: parent.width - 56
                spacing: 16

                Text {
                    width: parent.width
                    text: qsTr("Sign in to your server")
                    color: Theme.text
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 22
                    font.weight: Font.Bold
                }

                Text {
                    width: parent.width
                    visible: Session.error !== ""
                    text: Session.error
                    color: Theme.red
                    font.pixelSize: Theme.fsSmall
                    wrapMode: Text.Wrap
                }

                Column {
                    width: parent.width
                    spacing: 10

                    Field {
                        id: address

                        label: qsTr("Server address")
                        text: Session.address
                        input.placeholderText: "music.example.org"
                        input.placeholderTextColor: Theme.text3
                        input.inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                    }

                    Field {
                        id: username

                        label: qsTr("Username")
                        text: Session.username
                        input.inputMethodHints: Qt.ImhNoAutoUppercase
                    }

                    Field {
                        id: password

                        label: qsTr("Password")
                        echoMode: TextInput.Password
                    }
                }

                CheckBox {
                    id: keep

                    text: qsTr("Keep me signed in")
                    checked: Session.keychainAvailable
                    enabled: Session.keychainAvailable && !Session.busy
                    padding: 0
                    spacing: 8
                    font.pixelSize: Theme.fsSmall
                    palette.text: Theme.text
                    palette.windowText: Theme.text
                    palette.base: Theme.fill2
                    palette.highlight: Theme.accent
                }

                Text {
                    width: parent.width
                    visible: !Session.keychainAvailable
                    text: qsTr("No keychain service is running, so Legato can't keep your password. You'll sign in at each start.")
                    color: Theme.text2
                    font.pixelSize: Theme.fsSmall
                    wrapMode: Text.Wrap
                }

                PushButton {
                    width: parent.width
                    primary: true
                    text: Session.busy ? qsTr("Signing In…") : qsTr("Sign In")
                    enabled: !Session.busy
                    onClicked: view.submit()
                }

                PushButton {
                    width: parent.width
                    visible: Session.canCancel
                    text: qsTr("Cancel")
                    onClicked: Session.cancelSignIn()
                }
            }
        }

        // The other saved servers, to switch to instead.
        Column {
            id: saved

            readonly property var others: Session.servers.filter(server => server.address !== Session.address
                                                                 || server.username !== Session.username)

            width: 380
            visible: others.length > 0
            spacing: 2

            Text {
                leftPadding: 10
                bottomPadding: 2
                text: qsTr("Saved servers")
                color: Theme.text3
                font.pixelSize: Theme.fsTiny
                font.weight: Font.DemiBold
            }

            Repeater {
                model: saved.others

                delegate: ItemDelegate {
                    id: entry

                    required property var modelData

                    width: saved.width
                    implicitHeight: 40
                    leftPadding: 10
                    rightPadding: 10
                    enabled: !Session.busy
                    onClicked: Session.switchTo(entry.modelData.id)

                    contentItem: Row {
                        spacing: 10

                        StatusDot {
                            anchors.verticalCenter: parent.verticalCenter
                            status: entry.modelData.status
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: entry.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fs
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: entry.modelData.username
                            color: Theme.text2
                            font.pixelSize: Theme.fsSmall
                        }
                    }

                    background: Rectangle {
                        radius: 8
                        color: entry.down ? Theme.fill2 : entry.hovered ? Theme.fill : "transparent"
                    }
                }
            }
        }
    }
}

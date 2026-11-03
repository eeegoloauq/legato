pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import Legato

// Get info (docs/design.md, "Get info"): a card over the dimmed window with a song's or an album's
// details, asked from the server when it opens, so it opens whole once they are in. Read only: a
// Subsonic server cannot change tags. The text can be selected and copied. Esc or a click outside
// closes it.
Popup {
    id: card

    // Opened once the answer is in.
    property bool pending: false

    function show(kind, id) {
        pending = true
        info.load(kind, id)
    }

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(460, parent.width - 32)
    padding: 20
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    ItemInfo {
        id: info

        onLoadingChanged: {
            if (!loading && card.pending) {
                card.pending = false
                card.open()
            }
        }
    }

    // Opened from a menu, the card is often up before the menu has finished closing, and the menu
    // then hands the focus back to the page under it, where Esc does nothing. A modal card keeps it.
    property bool holdFocus: false

    onAboutToShow: holdFocus = true
    onAboutToHide: holdFocus = false

    Connections {
        target: card.parent ? card.parent.Window.window : null
        enabled: card.holdFocus

        function onActiveFocusItemChanged() {
            if (!card.activeFocus)
                card.forceActiveFocus()
        }
    }

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

        Row {
            width: parent.width
            spacing: 14
            visible: info.error === ""

            RoundedImage {
                id: cover

                width: 64
                height: 64
                coverArt: info.coverArt
            }

            Column {
                width: parent.width - cover.width - parent.spacing
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                TextEdit {
                    width: parent.width
                    text: info.title
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: Theme.text
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.textOnAccent
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }

                TextEdit {
                    width: parent.width
                    visible: text !== ""
                    text: info.artist
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: Theme.text2
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.textOnAccent
                    font.pixelSize: Theme.fs
                }
            }
        }

        Text {
            width: parent.width
            visible: info.error !== ""
            text: info.kind === "album" ? qsTr("Could not load this album: %1").arg(info.error)
                                         : qsTr("Could not load this song: %1").arg(info.error)
            color: Theme.text2
            wrapMode: Text.Wrap
        }

        Rectangle {
            width: parent.width
            height: 1
            visible: info.error === ""
            color: Theme.line
        }

        // A long comment scrolls rather than pushing the card off the window.
        Flickable {
            width: parent.width
            height: Math.min(rows.height, card.parent.height * 0.7 - 120)
            visible: info.error === ""
            contentHeight: rows.height
            boundsBehavior: Flickable.StopAtBounds
            clip: contentHeight > height

            Column {
                id: rows

                width: parent.width
                spacing: 7

                Repeater {
                    model: info.rows

                    Row {
                        id: infoRow

                        required property var modelData

                        width: rows.width
                        spacing: 12

                        // Labels end at a common edge, as in a macOS form.
                        Text {
                            width: 96
                            horizontalAlignment: Text.AlignRight
                            text: infoRow.modelData.label
                            color: Theme.text2
                            font.pixelSize: Theme.fsSmall
                        }

                        TextEdit {
                            width: infoRow.width - 96 - infoRow.spacing
                            text: infoRow.modelData.value
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                            color: Theme.text
                            selectionColor: Theme.accent
                            selectedTextColor: Theme.textOnAccent
                            font.pixelSize: Theme.fsSmall
                            font.features: { "tnum": 1 }
                        }
                    }
                }
            }
        }
    }
}

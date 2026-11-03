import QtQuick
import QtQuick.Controls.Basic
import Legato

// Find in page (docs/design.md, "Song lists"): a magnifier in the header that opens into a field
// on a click or Ctrl+F. Typing filters the page's songs; ↓ goes to the first match, Enter plays it,
// Esc clears the field and gives the keyboard back to the page. It stays open while it holds text.
Item {
    id: find

    required property ContentPage page
    readonly property bool open: input.activeFocus || (page !== null && page.findText !== "")
    property real openWidth: 220

    function start() {
        input.forceActiveFocus()
        input.selectAll()
    }

    function leave() {
        if (page) {
            page.findText = ""
            page.forceActiveFocus()
        }
    }

    width: open ? openWidth : 30
    height: 30

    Behavior on width {
        enabled: Theme.animationsOn
        NumberAnimation { duration: Theme.fastMs; easing.type: Easing.OutCubic }
    }

    HoverHandler {
        id: hover

        cursorShape: find.open ? Qt.IBeamCursor : Qt.ArrowCursor
    }

    // Closed only: once open, a click in the text places the cursor.
    TapHandler {
        enabled: !find.open
        onTapped: find.start()
    }

    ToolTip.text: qsTr("Find (Ctrl+F)")
    ToolTip.visible: hover.hovered && !find.open
    ToolTip.delay: 600

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: find.open || hover.hovered ? Theme.fill2 : Theme.fill
    }

    Glyph {
        x: 7
        anchors.verticalCenter: parent.verticalCenter
        name: "search"
        size: 16
        color: Theme.text2
    }

    TextInput {
        id: input

        anchors.left: parent.left
        anchors.leftMargin: 30
        anchors.right: clear.visible ? clear.left : parent.right
        anchors.rightMargin: clear.visible ? 2 : 12
        anchors.verticalCenter: parent.verticalCenter
        opacity: find.open ? 1 : 0
        text: find.page ? find.page.findText : ""
        color: Theme.text
        selectionColor: Theme.accent
        selectedTextColor: Theme.textOnAccent
        selectByMouse: true
        clip: true
        font.pixelSize: Theme.fsSmall
        Accessible.name: placeholder.text
        onTextEdited: if (find.page) find.page.findText = text
        // Esc belongs to the field here, not to the window's Esc (which closes the side panel).
        Keys.onShortcutOverride: event => event.accepted = event.key === Qt.Key_Escape
        Keys.onEscapePressed: find.leave()
        Keys.onDownPressed: if (find.page) find.page.findCommitted(false)
        onAccepted: if (find.page) find.page.findCommitted(true)

        Text {
            id: placeholder

            anchors.fill: parent
            visible: input.text === "" && input.preeditText === ""
            text: {
                switch (find.page ? find.page.route.page : "") {
                case "album": return qsTr("Find in album")
                case "playlist": return qsTr("Find in playlist")
                case "favorites": return qsTr("Find in favorites")
                default: return qsTr("Find")
                }
            }
            color: Theme.text3
            font: input.font
            elide: Text.ElideRight
        }
    }

    IconButton {
        id: clear

        anchors.right: parent.right
        anchors.rightMargin: 3
        anchors.verticalCenter: parent.verticalCenter
        visible: find.open && input.text !== ""
        diameter: 24
        glyph: "close"
        glyphSize: 14
        tone: Theme.text2
        tip: qsTr("Clear")
        focusPolicy: Qt.NoFocus
        onClicked: {
            if (find.page)
                find.page.findText = ""
            input.forceActiveFocus()
        }
    }
}

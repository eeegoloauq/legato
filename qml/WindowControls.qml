import QtQuick
import Legato

// Window buttons for the undecorated window (docs/architecture.md, "Desktop integration").
Row {
    id: controls

    // White on the full-screen player's picture.
    property color tone: Theme.text
    property color restFill: Theme.fill2

    spacing: 10

    IconButton {
        glyph: "remove"
        tip: qsTr("Minimize")
        diameter: 24
        glyphSize: 16
        tone: controls.tone
        restFill: controls.restFill
        onClicked: Window.window.showMinimized()
    }

    IconButton {
        glyph: "crop_square"
        tip: Window.window && Window.window.visibility === Window.Maximized ? qsTr("Restore") : qsTr("Maximize")
        diameter: 24
        glyphSize: 14
        tone: controls.tone
        restFill: controls.restFill
        onClicked: Window.window.visibility === Window.Maximized ? Window.window.showNormal() : Window.window.showMaximized()
    }

    IconButton {
        glyph: "close"
        tip: qsTr("Close")
        diameter: 24
        glyphSize: 16
        tone: controls.tone
        restFill: controls.restFill
        onClicked: Window.window.close()
    }
}

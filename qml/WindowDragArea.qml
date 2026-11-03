import QtQuick

// Empty background that works as a title bar (the window has none): a drag moves the window, a
// double click maximizes it or brings it back. A MouseArea, not a DragHandler: Qt shows a press to
// the pointer handlers under an item that took it, so a DragHandler moved the window from a text
// field's selection or a slider's knob. A MouseArea gets only the presses nothing above it took.
MouseArea {
    id: area

    property point pressAt
    // Asked once per press: the window manager has the pointer from then on.
    property bool moving: false

    onPressed: mouse => {
        pressAt = Qt.point(mouse.x, mouse.y)
        moving = false
    }
    onPositionChanged: mouse => {
        if (moving || !(mouse.buttons & Qt.LeftButton))
            return
        if (Math.hypot(mouse.x - pressAt.x, mouse.y - pressAt.y) < Application.styleHints.startDragDistance)
            return
        moving = true
        area.Window.window.startSystemMove()
    }
    onDoubleClicked: mouse => {
        if (mouse.button !== Qt.LeftButton)
            return
        const window = area.Window.window
        window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
    }
}

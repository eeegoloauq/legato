import QtQuick
import Legato

// The sticky header over the content (docs/design.md, "Shell"). It turns to glass and shows the page
// title once the page title has scrolled away. Dragging it moves the window.
Item {
    id: bar

    required property Item source
    property bool scrolled: false
    property string title
    // Off while the side panel covers the right end and carries them.
    property bool windowControls: true
    property bool canGoBack: false
    property bool canGoForward: false
    // Where back and forward start: further right while the sidebar button sits at the left end.
    property real leftInset: 20
    // The page under the header: a findable one gets the find field at the right end.
    property ContentPage page: null

    signal backRequested()
    signal forwardRequested()

    // Ctrl+F: opens the find field of a findable page.
    function find() {
        findField.start()
    }

    height: Theme.headerHeight

    // Solid to the pointer, so a click on the header never reaches the album under it; the wheel
    // still scrolls the page. Its handlers see the press first: drag moves the window, a double
    // click maximizes it.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = false

        DragHandler {
            target: null
            onActiveChanged: if (active) bar.Window.window.startSystemMove()
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: {
                const window = bar.Window.window
                window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
            }
        }
    }

    Item {
        anchors.fill: parent
        opacity: bar.scrolled ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.normalMs; easing.type: Easing.OutCubic }
        }

        GlassPanel {
            anchors.fill: parent
            source: bar.source
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.line
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: bar.leftInset
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        IconButton {
            glyph: "chevron_left"
            tip: qsTr("Back (Alt+←)")
            restFill: Theme.fill
            tone: Theme.text2
            enabled: bar.canGoBack
            onClicked: bar.backRequested()
        }

        IconButton {
            glyph: "chevron_right"
            tip: qsTr("Forward (Alt+→)")
            restFill: Theme.fill
            tone: Theme.text2
            enabled: bar.canGoForward
            onClicked: bar.forwardRequested()
        }
    }

    Text {
        anchors.centerIn: parent
        text: bar.title
        color: Theme.text
        font.pixelSize: Theme.fs
        font.weight: Font.DemiBold
        opacity: bar.scrolled ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.normalMs }
        }
    }

    FindField {
        id: findField

        anchors.right: controls.visible ? controls.left : parent.right
        anchors.rightMargin: controls.visible ? 14 : 16
        anchors.verticalCenter: parent.verticalCenter
        visible: bar.page !== null && bar.page.findable
        page: bar.page
        openWidth: Math.min(220, bar.width * 0.3)
    }

    WindowControls {
        id: controls

        visible: bar.windowControls
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import Legato

// The M0 frame meter (docs/roadmap.md, "M0"). F2 hides it. The live number counts frames the window
// presented in the last second; it drops to zero while nothing on screen changes.
Rectangle {
    id: panel

    // The album grid on Home; the scroll test needs it.
    property GridView view

    signal fullScreenRequested(bool open)
    signal testFinished()

    function runTest(name) {
        if (name === "scroll")
            scrollTest.start()
        else if (name === "full-screen")
            fullScreenTest.start()
    }

    readonly property bool testing: scrollTest.running || fullScreenTest.running

    function effects() {
        const on = []
        if (Theme.blurOn) on.push("blur")
        if (Theme.shadowsOn) on.push("shadows")
        if (Theme.animationsOn) on.push("animations")
        return on.join(", ")
    }

    width: 300
    height: content.implicitHeight + 24
    radius: 12
    color: Theme.raised
    border.width: 1
    border.color: Theme.line

    FrameMeter {
        id: meter
        window: panel.Window.window
    }

    // Scrolls the grid at a fixed 1500 px/s for 12 s from the top.
    SequentialAnimation {
        id: scrollTest

        ScriptAction { script: panel.view.contentY = panel.view.originY }
        PauseAnimation { duration: 600 }
        ScriptAction { script: meter.start(qsTr("scroll test"), panel.effects()) }
        NumberAnimation {
            target: panel.view
            property: "contentY"
            from: panel.view.originY
            to: panel.view.originY + Math.min(18000, Math.max(0, panel.view.contentHeight - panel.view.height))
            duration: 12000
        }
        ScriptAction { script: meter.stop() }
        ScriptAction { script: panel.testFinished() }
    }

    // Opens the full-screen player with its drifting background for 8 s.
    SequentialAnimation {
        id: fullScreenTest

        ScriptAction { script: panel.fullScreenRequested(true) }
        PauseAnimation { duration: 800 }
        ScriptAction { script: meter.start(qsTr("full-screen test"), panel.effects()) }
        PauseAnimation { duration: 8000 }
        ScriptAction { script: meter.stop() }
        ScriptAction { script: panel.fullScreenRequested(false) }
        ScriptAction { script: panel.testFinished() }
    }

    component Toggle: CheckBox {
        id: toggle

        font.pixelSize: Theme.fsSmall
        palette.windowText: Theme.text
        palette.text: Theme.text
        palette.base: Theme.raised
        palette.mid: Theme.text3
        palette.dark: Theme.text2
        enabled: !panel.testing
    }

    component Action: Button {
        id: action

        font.pixelSize: Theme.fsSmall
        enabled: !panel.testing
        palette.buttonText: Theme.text
        background: Rectangle {
            radius: Theme.radiusSmall
            color: action.down ? Theme.fill2 : action.hovered ? Theme.fill2 : Theme.fill
        }
    }

    Column {
        id: content

        x: 14
        y: 12
        width: parent.width - 28
        spacing: 8

        Row {
            spacing: 8

            Text {
                id: fps

                text: meter.fps.toFixed(0)
                color: Theme.text
                font.pixelSize: 26
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
            }

            Text {
                anchors.baseline: fps.baseline
                text: qsTr("fps · display %1 Hz").arg(Math.round(meter.refreshRate))
                color: Theme.text2
                font.pixelSize: Theme.fsSmall
            }
        }

        Text {
            width: parent.width
            text: qsTr("Frames presented in the last second. F2 hides this panel.")
            color: Theme.text3
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        Row {
            spacing: 8

            Action {
                text: scrollTest.running ? qsTr("Scrolling…") : qsTr("Scroll test")
                onClicked: scrollTest.start()
            }

            Action {
                text: fullScreenTest.running ? qsTr("Running…") : qsTr("Full-screen test")
                onClicked: fullScreenTest.start()
            }
        }

        Flow {
            width: parent.width
            spacing: 4

            Toggle { text: qsTr("Blur"); checked: Theme.blurOn; onToggled: Theme.blurOn = !Theme.blurOn }
            Toggle { text: qsTr("Shadows"); checked: Theme.shadowsOn; onToggled: Theme.shadowsOn = !Theme.shadowsOn }
            Toggle { text: qsTr("Animations"); checked: Theme.animationsOn; onToggled: Theme.animationsOn = !Theme.animationsOn }
        }

        TextEdit {
            width: parent.width
            visible: text !== ""
            text: meter.report
            readOnly: true
            selectByMouse: true
            wrapMode: Text.Wrap
            color: Theme.text
            selectionColor: Theme.accent
            font.pixelSize: 11
            font.family: "monospace"
        }
    }
}

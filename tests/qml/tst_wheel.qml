import QtQuick
import QtTest
import "../../qml" as App

// The page wheel (qml/WheelScroll.qml): which delta it takes, and that real wheel events reach it
// over the whole view, the header included.
Item {
    width: 400
    height: 500

    GridView {
        id: grid

        anchors.fill: parent
        cellWidth: 200
        cellHeight: 200
        model: 500
        // A header sits above the content item's bounds, where a handler inside the content item
        // never saw the wheel.
        header: Rectangle { width: 400; height: 300; color: "steelblue" }
        delegate: Rectangle { width: 200; height: 200; color: "gray" }
    }

    App.WheelScroll {
        id: wheel

        flickable: grid
        duration: 0
        property int events: 0
        onMoved: events++
    }

    TestCase {
        name: "WheelScroll"
        when: windowShown

        function init() {
            wheel.glide.stop()
            wheel.duration = 0
            wheel.events = 0
            wheel.direction = 0
            grid.contentY = 1000
        }

        function event(angle, pixels, phase, modifiers) {
            return { angleDelta: Qt.point(0, angle), pixelDelta: Qt.point(0, pixels),
                     phase: phase === undefined ? Qt.NoScrollPhase : phase,
                     modifiers: modifiers === undefined ? Qt.NoModifier : modifiers }
        }

        // GTK 4's step: the view's height (500) to the power 2/3.
        function test_theStepIsGtks() {
            compare(wheel.step, 63)
        }

        // What a mouse wheel on GNOME Wayland sends for one notch.
        function test_aWaylandNotchMovesAStepNotTheCompositorsPixels() {
            wheel.scroll(event(-120, -10))
            tryCompare(grid, "contentY", 1000 + wheel.step)
        }

        function test_aHighResolutionWheelMovesInProportion() {
            wheel.scroll(event(-30, -2))
            tryCompare(grid, "contentY", 1000 + wheel.step / 4)
        }

        function test_aTouchpadGestureFollowsTheFingers() {
            wheel.scroll(event(-24, -17, Qt.ScrollUpdate))
            compare(grid.contentY, 1017)
            verify(!wheel.glide.running)
        }

        function test_pixelOnlyInputMovesByPixels() {
            wheel.scroll(event(0, -17))
            compare(grid.contentY, 1017)
        }

        // Quick notches add up without speeding up: three notches go three steps.
        function test_quickNotchesAddUpWithoutAcceleration() {
            wheel.duration = 1000
            for (let i = 0; i < 3; ++i)
                wheel.scroll(event(-120, -10))
            compare(wheel.goal, 1000 + 3 * wheel.step)
        }

        function test_reversingGoesBackFromWhereThePageIs() {
            wheel.duration = 1000
            wheel.scroll(event(-120, -10))
            wheel.scroll(event(-120, -10))
            wait(50)
            const here = grid.contentY
            wheel.scroll(event(120, 10))
            verify(wheel.goal < here, "the page turns back at once")
            fuzzyCompare(wheel.goal, here - wheel.step, 1)
        }

        // Left to what is under the page, such as a horizontal strip.
        function test_shiftAndSidewaysPassThrough() {
            verify(!wheel.scroll(event(-120, -10, Qt.NoScrollPhase, Qt.ShiftModifier)))
            verify(!wheel.scroll({ angleDelta: Qt.point(-120, 0), pixelDelta: Qt.point(-10, 0),
                                   phase: Qt.NoScrollPhase, modifiers: Qt.NoModifier }))
            compare(grid.contentY, 1000)
        }

        function test_theWheelReachesTheHandlerOverTheContent() {
            mouseWheel(grid, 100, 400, 0, -120)
            tryCompare(wheel, "events", 1)
            tryCompare(grid, "contentY", 1000 + wheel.step)
        }

        function test_theWheelReachesTheHandlerOverTheHeader() {
            grid.contentY = grid.originY
            const start = grid.contentY
            mouseWheel(grid, 100, 100, 0, -120)
            tryCompare(wheel, "events", 1)
            tryCompare(grid, "contentY", start + wheel.step)
        }
    }
}

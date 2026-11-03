import QtQuick
import Legato

// The top of an album or artist page tinted with the cover's colors, fading into the background
// (docs/design.md, "Album page"). Put it behind the page's view and move it with the content.
Item {
    id: tint

    property alias coverArt: colors.coverArt
    // The cover's colors, strongest first; empty until read.
    readonly property var coverColors: colors.colors

    height: 560
    opacity: Theme.coverColors && colors.colors.length > 0 ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: Theme.normalMs }
    }

    CoverColors {
        id: colors
    }

    ShaderEffect {
        readonly property color color1: colors.colors.length > 0 ? colors.colors[0] : "transparent"
        readonly property color color2: colors.colors.length > 1 ? colors.colors[1] : color1

        anchors.fill: parent
        visible: tint.opacity > 0
        fragmentShader: "qrc:/qt/qml/Legato/shaders/tint.frag.qsb"
    }
}

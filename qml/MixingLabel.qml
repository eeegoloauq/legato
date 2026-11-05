pragma ComponentBehavior: Bound

import QtQuick
import Legato

// "Mixing", while the old song is still heard under the new one (docs/design.md, "Player bar"): a
// highlight sweeps across it from left to right, a sign of the change rather than a control. With
// animations off it is plain text.
Text {
    id: root

    // The highlight moves while the music plays.
    property bool running: true
    property real phase: 0

    text: qsTr("Mixing")
    color: Theme.text
    font.weight: Font.DemiBold
    font.letterSpacing: 0.3

    // Hidden, it keeps no texture.
    layer.enabled: Theme.animationsOn && visible
    layer.effect: ShaderEffect {
        readonly property real phase: root.phase
        readonly property real base: 0.45
        readonly property real band: 0.35

        fragmentShader: "qrc:/qt/qml/Legato/shaders/shimmer.frag.qsb"
    }

    SequentialAnimation on phase {
        running: root.running && root.visible && Theme.animationsOn
        loops: Animation.Infinite

        NumberAnimation { from: 0; to: 1; duration: 1400; easing.type: Easing.InOutSine }
        PauseAnimation { duration: 500 }
    }
}

import QtQuick
import QtQuick.Effects
import Legato

// Generated cover: soft color blobs with the name set large in white (docs/design.md, "Generated
// covers"). `live` covers drift while `active` (on screen) and visible; small covers stay still. A stopped
// drift starts again from the beginning, unseen.
Item {
    id: root

    property string name
    property list<color> colors: ["#5a52d6", "#ff7eb3", "#ffb86b"]
    property bool live: false
    property bool active: true
    property real radius: Theme.radiusCard
    property bool shadow: true

    readonly property int labelSize: Math.round(Math.max(18, Math.min(30, width * 0.12)))

    RectangularShadow {
        anchors.fill: blobs
        visible: root.shadow && Theme.shadowsOn
        radius: root.radius
        offset.y: 8
        blur: 26
        color: Theme.shadow
    }

    ShaderEffect {
        id: blobs

        readonly property size itemSize: Qt.size(width, height)
        readonly property real radius: root.radius
        readonly property real dpr: Screen.devicePixelRatio
        property real phase: 0
        readonly property color color1: root.colors[0]
        readonly property color color2: root.colors[1]
        readonly property color color3: root.colors[2]

        anchors.fill: parent
        fragmentShader: "qrc:/qt/qml/Legato/shaders/blobs.frag.qsb"

        NumberAnimation on phase {
            from: 0
            to: 2 * Math.PI
            duration: Theme.driftMs
            loops: Animation.Infinite
            // Stopped rather than paused: a paused animation still ticks.
            running: root.live && root.active && root.visible && Theme.animationsOn
        }
    }

    Text {
        anchors.centerIn: parent
        width: parent.width * 0.8
        visible: root.name !== ""
        text: root.name
        color: "white"
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font.pixelSize: root.labelSize
        font.weight: Font.Bold
        font.letterSpacing: -0.03 * root.labelSize
        lineHeight: 1.02
    }
}

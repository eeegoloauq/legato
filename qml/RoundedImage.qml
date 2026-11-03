pragma ComponentBehavior: Bound

import QtQuick
import Legato

// A cover with rounded corners over a placeholder color. The image is decoded off the GUI thread at
// the size shown, in size steps; a new size keeps the old picture until the new one is decoded, so
// resizing the window never blanks a cover.
//
// A new cover fades in over a neutral placeholder, unless it was decoded already (a page shown
// again, a row scrolled back), which shows at once. With `crossfade` (the player bar, the full-screen
// player) the old cover stays until the new one is ready and the new one fades in over it, so a
// change of song never shows the placeholder, and the same picture under another id (songs of one
// album on Navidrome) does not visibly change.
Item {
    id: root

    property string coverArt
    property real radius: Theme.radiusArt
    property color placeholder: Theme.fill2
    property bool crossfade: false
    // How long a crossfade takes; read when it starts.
    property int crossfadeMs: Theme.normalMs

    readonly property string wanted: coverArt !== "" ? "image://cover/" + coverArt : ""
    readonly property bool ready: front.ready && front.holds(wanted)
    readonly property int step: 50
    // When the front layer was given its cover, to tell a cover in memory from one being decoded.
    property double asked: 0
    // The layer shown; the other one loads the next cover in crossfade mode.
    property Layer front: first
    readonly property Layer back: front === first ? second : first

    // Runs after a change of cover has started loading, so a cover that was already decoded (Ready
    // at once) is seen as ready.
    function settle() {
        if (!crossfade || front.opacity === 0 || wanted === "") {
            if (!back.holds("")) {
                back.opacity = 0
                back.source = ""
            }
            if (!front.holds(wanted)) {
                fade.stop()
                front.opacity = 0
                asked = Date.now()
                front.source = wanted
            }
            if (front.ready && front.opacity === 0) {
                // Decoded within a frame or two: it was in memory, so no fade.
                if (Date.now() - asked < 40)
                    front.opacity = 1
                else
                    show(front)
            }
            return
        }
        if (front.holds(wanted)) {
            if (fade.target === back)
                fade.stop()
            back.opacity = 0
            back.source = ""
            return
        }
        if (!back.holds(wanted)) {
            if (fade.target === back)
                fade.stop()
            back.opacity = 0
            back.source = wanted
        }
        if (back.ready && back.opacity === 0) {
            back.z = 1
            front.z = 0
            show(back)
        }
    }

    function show(layer) {
        fade.stop()
        fade.target = layer
        fade.duration = crossfade ? crossfadeMs : Theme.fastMs
        if (Theme.animationsOn) {
            fade.restart()
        } else {
            layer.opacity = 1
            fade.finished()
        }
    }

    onWantedChanged: Qt.callLater(settle)
    Component.onCompleted: settle()

    component Layer: Item {
        id: layer

        property alias source: image.source
        readonly property bool ready: image.status === Image.Ready

        function holds(url: string): bool {
            return String(image.source) === url
        }

        anchors.fill: parent
        opacity: 0
        visible: opacity > 0

        onReadyChanged: if (ready) Qt.callLater(root.settle)

        Image {
            id: image

            anchors.fill: parent
            visible: false
            asynchronous: true
            retainWhileLoading: true
            sourceSize: {
                const px = Math.ceil(Math.max(width, height) * Screen.devicePixelRatio / root.step) * root.step
                return Qt.size(px, px)
            }
        }

        ShaderEffect {
            readonly property Image source: image
            readonly property size itemSize: Qt.size(width, height)
            readonly property real radius: root.radius
            readonly property real dpr: Screen.devicePixelRatio

            anchors.fill: parent
            fragmentShader: "qrc:/qt/qml/Legato/shaders/rounded.frag.qsb"
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.placeholder
        visible: root.front.opacity < 1 && root.back.opacity < 1
    }

    Layer {
        id: first
    }

    Layer {
        id: second
    }

    NumberAnimation {
        id: fade

        property: "opacity"
        from: 0
        to: 1

        // The cover that faded in becomes the front; the old one lets go of its image.
        onFinished: {
            const layer = target as Layer
            if (layer && layer !== root.front) {
                const old = root.front
                root.front = layer
                old.opacity = 0
                old.source = ""
            }
        }
    }
}

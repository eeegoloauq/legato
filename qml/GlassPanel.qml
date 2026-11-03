import QtQuick
import QtQuick.Effects
import Legato

// Frosted glass over `source`: the part of the source behind the panel, blurred, saturated and
// tinted. The capture reaches `padding` beyond the panel, so the blur at the edge mixes in real
// content instead of fading to transparent; a mask cuts it back to the panel's shape.
Item {
    id: panel

    required property Item source
    property real radius: 0
    // A light rim around the panel, as on Apple's floating bars.
    property bool rim: false

    readonly property real padding: Theme.glassBlur * (1 + Theme.glassBlurMultiplier) / 2
    readonly property bool blurring: Theme.blurOn && visible
    // The panel's position in the source. Re-read when either moves or resizes.
    readonly property point origin: {
        void [x, y, width, height, source.x, source.y, source.width, source.height]
        return mapToItem(source, 0, 0)
    }

    ShaderEffectSource {
        id: capture

        sourceItem: panel.source
        sourceRect: Qt.rect(panel.origin.x - panel.padding, panel.origin.y - panel.padding,
                            panel.width + 2 * panel.padding, panel.height + 2 * panel.padding)
        width: sourceRect.width
        height: sourceRect.height
        // Half resolution: the blur hides it and the capture costs a quarter.
        textureSize: Qt.size(Math.ceil(width / 2), Math.ceil(height / 2))
        live: panel.blurring
        recursive: false
        visible: false
    }

    // The panel's shape inside the padded capture: a hidden layer, as Qt documents for masks.
    Item {
        id: mask

        x: -panel.padding
        y: -panel.padding
        width: capture.width
        height: capture.height
        visible: false
        // Always on: MultiEffect reads whether its mask is a texture only when the mask is set,
        // so a layer switched on later (a panel that starts hidden) would leave it masking all.
        layer.enabled: true

        Rectangle {
            x: panel.padding
            y: panel.padding
            width: panel.width
            height: panel.height
            radius: panel.radius
            color: "black"
        }
    }

    // The page under the panel, hidden. The blurred copy alone is thin wherever the page is
    // transparent (text, the gaps between covers), and the sharp page showed through it.
    Rectangle {
        anchors.fill: parent
        visible: panel.blurring
        radius: panel.radius
        color: Theme.backdrop
    }

    MultiEffect {
        x: -panel.padding
        y: -panel.padding
        width: capture.width
        height: capture.height
        visible: panel.blurring
        source: capture
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1.0
        blurMax: Theme.glassBlur
        blurMultiplier: Theme.glassBlurMultiplier
        saturation: Theme.glassSaturation
        maskEnabled: true
        maskSource: mask
    }

    Rectangle {
        anchors.fill: parent
        radius: panel.radius
        // Without blur the tint alone has to separate the panel from what is behind it.
        color: panel.blurring ? Theme.glass : Qt.alpha(Theme.glass, 0.96)
        border.width: panel.rim ? 1 : 0
        border.color: Theme.glassEdge
    }
}

import QtQuick
import Legato

// A playlist's cover: the server's, or a generated one in colors from its id when it has none
// (docs/design.md, "Generated covers").
Item {
    id: root

    property string coverArt
    property string name
    property int hue
    property real radius: Theme.radiusArt
    property bool live: false
    property bool active: true

    RoundedImage {
        anchors.fill: parent
        visible: root.coverArt !== ""
        radius: root.radius
        coverArt: root.coverArt
    }

    GenCover {
        anchors.fill: parent
        visible: root.coverArt === ""
        radius: root.radius
        shadow: false
        // Small covers carry no name: it would not be readable.
        name: root.width >= 120 ? root.name : ""
        live: root.live
        active: root.active
        colors: [Qt.hsla(root.hue / 360, 0.62, 0.48, 1), Qt.hsla(((root.hue + 40) % 360) / 360, 0.75, 0.62, 1),
                 Qt.hsla(((root.hue + 300) % 360) / 360, 0.7, 0.66, 1)]
    }
}

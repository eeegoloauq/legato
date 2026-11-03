pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import Legato

// An icon of icons/ outside a button, in [color] (as lumeo's Glyph): the SVGs are drawn black,
// lifted to white and tinted.
Image {
    id: glyph

    required property string name
    property real size: 18
    property color color: Theme.text

    source: Theme.icon(name)
    sourceSize: Qt.size(size, size)
    width: size
    height: size
    layer.enabled: true
    layer.effect: MultiEffect {
        brightness: 1
        colorization: 1
        colorizationColor: glyph.color
    }
}

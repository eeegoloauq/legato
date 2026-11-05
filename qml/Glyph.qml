pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.impl
import Legato

// An icon of icons/ outside a button, in [color]. The SVGs are drawn black and tinted once, when the
// image loads (as the Basic style tints its indicators), so an icon is a plain image that batches
// with the others: no layer and no effect each frame (docs/decisions/0002-performance.md).
ColorImage {
    required property string name
    property real size: 18

    color: Theme.text
    source: Theme.icon(name)
    sourceSize: Qt.size(size, size)
    width: size
    height: size
}

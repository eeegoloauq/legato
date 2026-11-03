import QtQuick
import Legato

// A section's title (21 px). With `opens`, it ends in › and opens the full page (docs/design.md,
// "Home": "Section titles with › open the full page").
Row {
    id: section

    property alias text: label.text
    property bool opens: false

    signal clicked()

    spacing: 2

    Text {
        id: label

        color: Theme.text
        font.pixelSize: Theme.fsSection
        font.weight: Font.DemiBold
        font.letterSpacing: -0.015 * Theme.fsSection
    }

    Glyph {
        anchors.verticalCenter: label.verticalCenter
        anchors.verticalCenterOffset: 1
        visible: section.opens
        name: "chevron_right"
        size: 22
        color: hover.hovered ? Theme.text2 : Theme.text3
        // A Row owns x; setting it here put the chevron over the first letter.
        transform: Translate { x: hover.hovered ? 2 : 0 }
    }

    HoverHandler {
        id: hover

        enabled: section.opens
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        enabled: section.opens
        onTapped: section.clicked()
    }
}

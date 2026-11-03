import QtQuick
import Legato

// The top of a page under the header: its large title, with controls after it centered on the title.
// Children go after the title; give them
// `anchors.verticalCenter: parent.verticalCenter`.
Item {
    id: heading

    property alias text: title.text
    default property alias content: row.data

    implicitWidth: row.implicitWidth
    implicitHeight: Theme.headerHeight + 8 + row.height + 20

    Row {
        id: row

        y: Theme.headerHeight + 8
        spacing: 14

        Text {
            id: title

            color: Theme.text
            font.pixelSize: Theme.fsTitle
            font.weight: Font.Bold
            font.letterSpacing: -0.02 * Theme.fsTitle
        }
    }
}

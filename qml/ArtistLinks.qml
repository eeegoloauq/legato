pragma ComponentBehavior: Bound

import QtQuick
import Legato

// Each artist as its own link, underlined under the pointer (docs/design.md, "Player bar"). An
// artist without an id is plain text.
Flow {
    id: links

    // [{id, name}]
    property var artists: []
    // Shown when the list is empty (a server without OpenSubsonic's artists).
    property string fallback
    property color color: Theme.text2
    property int pixelSize: Theme.fsSmall
    property int weight: Font.Normal

    Repeater {
        model: links.artists.length > 0 ? links.artists : [{ id: "", name: links.fallback }]

        delegate: Row {
            id: link

            required property var modelData
            required property int index

            Text {
                text: link.modelData.name
                color: links.color
                font.pixelSize: links.pixelSize
                font.weight: links.weight
                font.underline: hover.hovered && link.modelData.id !== ""

                HoverHandler {
                    id: hover

                    enabled: link.modelData.id !== ""
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    enabled: link.modelData.id !== ""
                    onTapped: Navigation.artist(link.modelData.id)
                }
            }

            Text {
                visible: link.index < links.artists.length - 1
                text: link.index === links.artists.length - 2 ? " & " : ", "
                color: links.color
                font.pixelSize: links.pixelSize
                font.weight: links.weight
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import Legato

// Resize handles along the edges of the undecorated window. The compositor does the resizing.
Item {
    id: edges

    readonly property int thickness: 6

    anchors.fill: parent
    enabled: Window.window && Window.window.visibility === Window.Windowed

    component Edge: Item {
        id: edge

        required property int sides
        required property int cursor

        HoverHandler {
            cursorShape: edge.cursor
        }

        DragHandler {
            target: null
            onActiveChanged: if (active) edge.Window.window.startSystemResize(edge.sides)
        }
    }

    Edge { sides: Qt.LeftEdge; cursor: Qt.SizeHorCursor; x: 0; y: edges.thickness; width: edges.thickness; height: parent.height - 2 * edges.thickness }
    Edge { sides: Qt.RightEdge; cursor: Qt.SizeHorCursor; x: parent.width - edges.thickness; y: edges.thickness; width: edges.thickness; height: parent.height - 2 * edges.thickness }
    Edge { sides: Qt.TopEdge; cursor: Qt.SizeVerCursor; x: edges.thickness; y: 0; width: parent.width - 2 * edges.thickness; height: edges.thickness }
    Edge { sides: Qt.BottomEdge; cursor: Qt.SizeVerCursor; x: edges.thickness; y: parent.height - edges.thickness; width: parent.width - 2 * edges.thickness; height: edges.thickness }
    Edge { sides: Qt.TopEdge | Qt.LeftEdge; cursor: Qt.SizeFDiagCursor; width: edges.thickness; height: edges.thickness }
    Edge { sides: Qt.TopEdge | Qt.RightEdge; cursor: Qt.SizeBDiagCursor; x: parent.width - edges.thickness; width: edges.thickness; height: edges.thickness }
    Edge { sides: Qt.BottomEdge | Qt.LeftEdge; cursor: Qt.SizeBDiagCursor; y: parent.height - edges.thickness; width: edges.thickness; height: edges.thickness }
    Edge { sides: Qt.BottomEdge | Qt.RightEdge; cursor: Qt.SizeFDiagCursor; x: parent.width - edges.thickness; y: parent.height - edges.thickness; width: edges.thickness; height: edges.thickness }
}

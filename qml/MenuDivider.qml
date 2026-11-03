import QtQuick
import QtQuick.Controls.Basic
import Legato

// A thin line between groups of a PopupMenu.
MenuSeparator {
    topPadding: 4
    bottomPadding: 4
    contentItem: Rectangle {
        implicitHeight: 1
        color: Theme.line
    }
}

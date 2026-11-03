import QtQuick
import QtQuick.Templates as T
import Legato

// A thin scroll bar over the content that shows while scrolling or under the pointer, as on
// macOS. The mechanics are Qt's template; only the look is legato's. On a page it starts under the
// header, beside the window buttons, and stops short of the bottom edge (`underHeader: false` in the
// side panel and the lyrics, which begin below their own heads).
//
// A page's bar sits at the page's right edge, not at its flickable's, which stops a gutter short:
// carousels run into that gutter and the bar was drawn over their last cover. Qt's documented way:
// a bar whose parent is not the flickable is still driven by it but placed by us.
T.ScrollBar {
    id: bar

    property bool underHeader: true
    // Placed at the edge of the flickable's parent (the page) rather than inside the flickable.
    property bool pageEdge: underHeader

    Component.onCompleted: {
        // The attachment has made the flickable the parent by now.
        const flickable = parent
        if (!pageEdge || !flickable || !flickable.parent)
            return
        parent = flickable.parent
        anchors.top = flickable.top
        anchors.bottom = flickable.bottom
        anchors.right = flickable.parent.right
    }

    implicitWidth: 14
    padding: 3
    topPadding: underHeader ? Theme.headerHeight + 4 : 6
    bottomPadding: 10
    leftPadding: bar.hovered || bar.pressed ? 3 : 6
    minimumSize: 0.05
    policy: T.ScrollBar.AsNeeded
    visible: size < 1

    contentItem: Rectangle {
        radius: width / 2
        color: bar.pressed ? Theme.text2 : Theme.text3
        opacity: bar.active || bar.hovered ? 0.85 : 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.normalMs }
        }
    }
}

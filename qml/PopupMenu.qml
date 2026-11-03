import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import Legato

// A menu in the app's look: a rounded glass card with a soft shadow. Its entries are Actions. An
// action with an icon color gets a dot in that color before its text (server status); a checked one
// gets a check mark after it.
//
// Open it with openAt (a right click: at the pointer) or openFrom (a button), never popup(): Qt's
// popup() pushes a menu that does not fit back over the pointer, and it overwrites x and y, so a
// later open() lands where the last right click was.
Menu {
    id: menu

    // Opens at a point in `item` (null: the window), down and to the right of it, or up and to the
    // left where the window ends there, as desktop context menus do.
    function openAt(item: Item, px: real, py: real) {
        place(item, px, py, 0, 0, 0, false)
    }

    // Opens under `button`, or over it when there is no room below; its left edges line up, or its
    // right edges with `alignRight`.
    function openFrom(button: Item, gap: real, alignRight: bool) {
        place(button, 0, 0, button.width, button.height, gap ?? 4, alignRight ?? false)
    }

    // Beside the rectangle (rx, ry, rw, rh) in `item`.
    function place(item: Item, rx: real, ry: real, rw: real, rh: real, gap: real, alignRight: bool) {
        if (item)
            parent = item
        // The size is read before the menu shows: lay its list out first.
        const list = contentItem as ListView
        list?.forceLayout()
        const w = implicitWidth
        const h = implicitHeight
        const area = parent.Window.contentItem
        const r = item ? item.mapToItem(null, rx, ry) : Qt.point(rx, ry)
        const edge = 8

        let x = alignRight ? r.x + rw - w : r.x
        if (x + w > area.width - edge)
            x = r.x + rw - w
        if (x < edge)
            x = r.x
        x = Math.max(edge, Math.min(x, area.width - edge - w))

        let y = r.y + rh + gap
        if (y + h > area.height - edge)
            y = r.y - gap - h
        y = Math.max(edge, Math.min(y, area.height - edge - h))

        const local = parent.mapFromItem(null, x, y)
        menu.x = local.x
        menu.y = local.y
        open()
    }

    padding: 5
    modal: false
    margins: 8

    delegate: MenuItem {
        id: item

        implicitHeight: 30
        leftPadding: 10
        rightPadding: 10
        indicator: Item {}
        // A submenu's row ends in a chevron.
        arrow: Text {
            x: item.width - width - item.rightPadding
            y: (item.height - height) / 2
            visible: item.subMenu !== null
            text: "›"
            color: Theme.text2
            font.pixelSize: Theme.fs + 2
        }

        contentItem: Item {
            readonly property real dotSpace: dot.visible ? 17 : 0
            readonly property real checkSpace: item.checked || item.subMenu ? 24 : 0

            implicitWidth: dotSpace + label.implicitWidth + checkSpace
            implicitHeight: label.implicitHeight

            Rectangle {
                id: dot

                anchors.verticalCenter: parent.verticalCenter
                visible: item.icon.color.a > 0
                width: 8
                height: 8
                radius: 4
                color: item.icon.color
            }

            Text {
                id: label

                x: parent.dotSpace
                width: parent.width - parent.dotSpace - parent.checkSpace
                height: parent.height
                text: item.text
                color: item.enabled ? Theme.text : Theme.text3
                font.pixelSize: Theme.fs
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: item.checked
                text: "✓"
                color: Theme.text2
                font.pixelSize: Theme.fs
            }
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: item.highlighted ? Theme.fill2 : "transparent"
        }
    }

    background: Item {
        implicitWidth: 220

        RectangularShadow {
            anchors.fill: parent
            visible: Theme.shadowsOn
            radius: Theme.radius
            offset.y: 8
            blur: 24
            color: Theme.shadow
        }

        // Glass over the whole window, as the player bar: a menu over an album takes its color.
        GlassPanel {
            anchors.fill: parent
            radius: Theme.radius
            rim: true
            thin: true
            source: menu.parent ? menu.parent.Window.contentItem : null
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.fastMs }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.fastMs; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: 100 }
    }
}

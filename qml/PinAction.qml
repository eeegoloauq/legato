import QtQuick
import QtQuick.Controls.Basic
import Legato

// "Pin to Sidebar" or "Unpin" in a menu of an album, artist or playlist (docs/design.md, "Pins").
Action {
    id: action

    // "album", "artist" or "playlist".
    required property string kind
    required property string itemId
    property string name
    property string coverArt
    property int hue

    readonly property bool pinned: Prefs.isPinned(kind, itemId)

    text: pinned ? qsTr("Unpin") : qsTr("Pin to Sidebar")
    enabled: itemId !== "" && Library.serverKey !== ""
    onTriggered: pinned ? Prefs.unpin(kind, itemId) : Prefs.pin(kind, itemId, name, coverArt, hue)
}

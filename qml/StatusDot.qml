import QtQuick
import Legato

// Whether a server answers: green yes, red no, grey not known (not signed in, or not asked yet).
Rectangle {
    property string status: "unknown"

    width: 8
    height: 8
    radius: 4
    color: Theme.statusColor(status)
}

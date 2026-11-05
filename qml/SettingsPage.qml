pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Legato

// Settings (docs/design.md, "Settings"): a page, so back returns to where you were. Sections appear
// as their features are built.
ContentPage {
    id: page

    route: ({ page: "settings" })
    title: qsTr("Settings")
    flickable: view

    // A section title over a group of rows on a faint fill.
    component Section: Column {
        id: section

        property string title
        default property alias rows: group.data

        width: parent ? parent.width : 0
        spacing: 12

        Text {
            text: section.title
            color: Theme.text
            font.pixelSize: Theme.fsSection
            font.weight: Font.DemiBold
        }

        Rectangle {
            width: parent.width
            height: group.implicitHeight
            radius: Theme.radiusCard
            color: Theme.fill

            Column {
                id: group

                width: parent.width
            }
        }
    }

    // A label (and an optional line under it) on the left, a control on the right. A hairline
    // divides it from the row above, as in the system's settings.
    component SettingRow: Item {
        id: settingRow

        property string label
        property string detail
        // A problem with this row, in red under the detail.
        property string warning
        // A server's status ("up", "down", "unknown") as a dot before the label; none when empty.
        property string status
        // The label as a text field (renaming a server); `edited` brings the text back.
        property bool editing: false
        property string editText
        default property alias control: holder.data

        signal edited(string text)

        width: parent ? parent.width : 0
        implicitHeight: Math.max(52, texts.implicitHeight + 28)

        Rectangle {
            x: 16
            width: parent.width - 32
            height: 1
            visible: settingRow.y > 0
            color: Theme.line
        }

        StatusDot {
            id: dot

            x: 16
            anchors.verticalCenter: parent.verticalCenter
            visible: settingRow.status !== ""
            status: settingRow.status
        }

        Column {
            id: texts

            anchors.left: parent.left
            anchors.right: holder.left
            anchors.leftMargin: dot.visible ? 34 : 16
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                visible: !settingRow.editing
                text: settingRow.label
                color: Theme.text
                font.pixelSize: Theme.fs
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            // Enter or a click elsewhere keeps the text, Esc gives up, as in a file manager.
            Loader {
                width: parent.width
                active: settingRow.editing
                visible: active
                onLoaded: {
                    const field = item as TextField
                    field.forceActiveFocus()
                    field.selectAll()
                }
                sourceComponent: TextField {
                    id: field

                    property bool done: false

                    function finish(keep) {
                        if (done)
                            return
                        done = true
                        if (keep)
                            settingRow.edited(text)
                        settingRow.editing = false
                    }

                    text: settingRow.editText
                    implicitHeight: 28
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 0
                    bottomPadding: 0
                    color: Theme.text
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.textOnAccent
                    font.pixelSize: Theme.fs
                    font.weight: Font.Medium
                    Accessible.name: qsTr("Name")
                    onAccepted: finish(true)
                    onActiveFocusChanged: if (!activeFocus) finish(true)
                    Keys.onEscapePressed: finish(false)

                    background: Rectangle {
                        radius: 6
                        color: Theme.raised
                        border.width: 2
                        border.color: Theme.accent
                    }
                }
            }

            Text {
                width: parent.width
                visible: text !== ""
                text: settingRow.detail
                color: Theme.text2
                font.pixelSize: Theme.fsSmall
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                visible: text !== ""
                text: settingRow.warning
                color: Theme.red
                font.pixelSize: Theme.fsSmall
                wrapMode: Text.Wrap
            }
        }

        Item {
            id: holder

            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            width: childrenRect.width
            height: childrenRect.height
        }
    }

    Flickable {
        id: view

        anchors.fill: parent
        contentHeight: body.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        pixelAligned: true

        Column {
            id: body

            x: Theme.gutter
            width: Math.min(740, view.width - 2 * Theme.gutter)
            topPadding: Theme.headerHeight + 8
            bottomPadding: Theme.barHeight + 64
            spacing: 32

            Text {
                text: qsTr("Settings")
                color: Theme.text
                font.pixelSize: Theme.fsTitle
                font.weight: Font.Bold
                font.letterSpacing: -0.02 * Theme.fsTitle
            }

            Section {
                title: qsTr("Appearance")

                SettingRow {
                    label: qsTr("Theme")
                    detail: Theme.forced >= 0 ? qsTr("--appearance on the command line overrides this choice.") : ""

                    Dropdown {
                        labels: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                        current: Theme.savedAppearance
                        Accessible.name: qsTr("Theme")
                        onChosen: index => Theme.savedAppearance = index
                    }
                }

                SettingRow {
                    label: qsTr("Colors from covers")
                    detail: qsTr("Album, artist and playlist pages take their colors from the cover.")

                    ToggleSwitch {
                        checked: Theme.coverColors
                        Accessible.name: qsTr("Colors from covers")
                        onToggled: Theme.coverColors = checked
                    }
                }

                SettingRow {
                    label: qsTr("Transparency")
                    detail: qsTr("The player bar, the header and menus blur what is under them. Off, they are solid, which is lighter on older graphics.")

                    ToggleSwitch {
                        checked: Theme.transparency
                        Accessible.name: qsTr("Transparency")
                        onToggled: Theme.transparency = checked
                    }
                }
            }

            Section {
                title: qsTr("Sidebar")

                Repeater {
                    model: [
                        { page: "recent", label: qsTr("Recently Added") },
                        { page: "artists", label: qsTr("Artists") },
                        { page: "albums", label: qsTr("Albums") },
                        { page: "songs", label: qsTr("Songs") },
                        { page: "favorites", label: qsTr("Favorites") },
                        { page: "playlists", label: qsTr("Playlists"), detail: qsTr("All Playlists and each playlist") }
                    ]

                    delegate: SettingRow {
                        id: sidebarRow

                        required property var modelData

                        label: modelData.label
                        detail: modelData.detail ?? ""

                        ToggleSwitch {
                            checked: Prefs.rowShown(sidebarRow.modelData.page)
                            Accessible.name: sidebarRow.modelData.label
                            onToggled: Prefs.setRowShown(sidebarRow.modelData.page, checked)
                        }
                    }
                }
            }

            Section {
                title: qsTr("Playback")

                SettingRow {
                    label: qsTr("Scrobbling")
                    detail: qsTr("Tells the server what you play, for play counts and Most Played, and for Last.fm or ListenBrainz when the server is set up for them.")

                    ToggleSwitch {
                        checked: Prefs.scrobbling
                        Accessible.name: qsTr("Scrobbling")
                        onToggled: Prefs.scrobbling = checked
                    }
                }

                SettingRow {
                    label: qsTr("Crossfade")
                    detail: [qsTr("A song that ends by itself fades into the next one. Skipping cuts at once."),
                             qsTr("Each change follows how one song ends and the next begins: long outros overlap, hard endings hardly, silence is skipped."),
                             qsTr("Every change fades over the same length.")][Prefs.crossfadeMode]

                    Dropdown {
                        labels: [qsTr("Off"), qsTr("Automatic"), qsTr("Fixed")]
                        current: Prefs.crossfadeMode
                        Accessible.name: qsTr("Crossfade")
                        onChosen: index => Prefs.setCrossfadeMode(index)
                    }
                }

                SettingRow {
                    visible: Prefs.crossfadeMode === 2
                    label: qsTr("Crossfade length")

                    Row {
                        spacing: 12

                        BarSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 160
                            fillColor: Theme.accent
                            from: 1
                            to: 12
                            stepSize: 1
                            snapMode: Slider.SnapAlways
                            value: Prefs.crossfadeSeconds
                            Accessible.name: qsTr("Crossfade length")
                            onMoved: Prefs.crossfadeSeconds = Math.round(value)
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1 s").arg(Prefs.crossfadeSeconds)
                            color: Theme.text2
                            font.pixelSize: Theme.fsSmall
                            font.features: { "tnum": 1 }
                        }
                    }
                }

                SettingRow {
                    visible: Prefs.crossfade
                    label: qsTr("Keep albums gapless")
                    detail: qsTr("Tracks that follow each other on an album never fade, so live albums and continuous mixes stay seamless.")

                    ToggleSwitch {
                        checked: Prefs.keepAlbumsGapless
                        Accessible.name: qsTr("Keep albums gapless")
                        onToggled: Prefs.keepAlbumsGapless = checked
                    }
                }

                SettingRow {
                    label: qsTr("Find missing lyrics on LRCLIB")
                    detail: qsTr("When your server has no lyrics for a song, Legato asks lrclib.net, sending the song's title, artist, album and length.")

                    ToggleSwitch {
                        checked: Prefs.lrclib
                        Accessible.name: qsTr("Find missing lyrics on LRCLIB")
                        onToggled: Prefs.lrclib = checked
                    }
                }
            }

            Section {
                title: qsTr("Servers")

                Repeater {
                    model: Session.servers

                    delegate: SettingRow {
                        id: serverRow

                        required property var modelData

                        label: modelData.name + (modelData.active ? "  ·  " + qsTr("Active") : "")
                        detail: modelData.address + "  ·  " + modelData.username
                        editText: modelData.name
                        onEdited: text => Session.rename(serverRow.modelData.id, text)
                        status: modelData.status
                        warning: modelData.keychainError

                        IconButton {
                            id: more

                            glyph: "more_horiz"
                            tip: qsTr("More")
                            tone: Theme.text2
                            onClicked: serverMenu.open()

                            PopupMenu {
                                id: serverMenu

                                x: more.width - width
                                y: more.height + 4

                                Action {
                                    text: qsTr("Switch to %1").arg(serverRow.modelData.name)
                                    enabled: !serverRow.modelData.active || Session.state !== Session.Ready
                                    onTriggered: Session.switchTo(serverRow.modelData.id)
                                }
                                Action {
                                    text: qsTr("Rename")
                                    onTriggered: serverRow.editing = true
                                }
                                Action {
                                    text: qsTr("Sign Out")
                                    enabled: serverRow.modelData.signedIn
                                    onTriggered: Session.signOut(serverRow.modelData.id)
                                }
                                Action {
                                    text: qsTr("Remove")
                                    onTriggered: Session.remove(serverRow.modelData.id)
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 56

                    Rectangle {
                        x: 16
                        width: parent.width - 32
                        height: 1
                        visible: parent.y > 0
                        color: Theme.line
                    }

                    PushButton {
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Add Server")
                        onClicked: Session.addServer()
                    }
                }
            }

            // What a bug report needs: the version and the log.
            Section {
                title: qsTr("About")

                SettingRow {
                    label: qsTr("Legato %1").arg(Qt.application.version)
                    detail: Log.folder !== "" ? qsTr("Logs are kept in %1.").arg(Log.folder) : ""

                    PushButton {
                        visible: Log.folder !== ""
                        text: qsTr("Show Logs")
                        onClicked: Log.showFolder()
                    }
                }
            }
        }

        T.ScrollBar.vertical: Scroller {}
    }

    WheelScroll {
        flickable: view
    }
}

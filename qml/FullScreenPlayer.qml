import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts
import Legato

// The full-screen player (docs/design.md, "Full-screen player"): drifting blobs in the cover's
// colors. The cover and controls sit on the left and the lyrics or Up Next on the right, as the two
// buttons by the volume choose; with neither, or lyrics chosen for a song without them, everything
// is centered. Esc or the X closes it.
Rectangle {
    id: root

    signal closed()

    readonly property list<color> colors: Player.palette.length === 3 ? Player.palette : ["#5a52d6", "#ff7eb3", "#3fc1c9"]
    // What the right side shows: "lyrics", "queue" or "" (nothing), remembered across runs.
    property string side: "lyrics"
    readonly property bool lyricsShown: side === "lyrics" && Player.lyrics.length > 0
    readonly property bool sideShown: lyricsShown || side === "queue"
    readonly property color dim: Qt.rgba(1, 1, 1, 0.62)
    // How long the picture takes to turn to a new song: as long as the change when two songs mix.
    // Set as the song changes, so it holds while the colors turn (they come a moment later).
    property int turnMs: Theme.normalMs * 2
    readonly property real dimming: {
        const linear = v => v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4)
        const luminance = c => 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b)
        const brightest = Math.max(luminance(colors[0]), luminance(colors[1]), luminance(colors[2]))
        // White reads at 4.5:1 on a luminance up to 0.183. Black at alpha a scales the channels by
        // 1 - a, so the luminance by about (1 - a)^2.2.
        const needed = brightest > 0.183 ? 1 - Math.pow(0.183 / brightest, 1 / 2.2) : 0
        return Math.max(0.22, Math.min(0.62, needed))
    }
    property color color1: colors[0]
    property color color2: colors[1]
    property color color3: colors[2]

    Connections {
        target: Player

        function onSongChanged() {
            root.turnMs = Player.mixing ? Math.round(Player.mixSeconds * 1000) : Theme.normalMs * 2
        }
    }

    Behavior on color1 {
        ColorAnimation { duration: root.turnMs }
    }
    Behavior on color2 {
        ColorAnimation { duration: root.turnMs }
    }
    Behavior on color3 {
        ColorAnimation { duration: root.turnMs }
    }

    StoredSetting {
        category: "fullScreen"
        property alias side: root.side
    }

    function clock(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
    }

    color: "#0d0d11"
    focus: true
    Keys.onEscapePressed: root.closed()

    // The window has no title bar here: the background moves it, a double click maximizes it. (Main
    // hides what is under the player, so nothing there can be clicked.)
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons

        DragHandler {
            target: null
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: {
                const window = root.Window.window
                window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
            }
        }
    }

    GenCover {
        anchors.fill: parent
        radius: 0
        shadow: false
        colors: [root.color1, root.color2, root.color3]
        live: true
        opacity: 0.85
    }

    // Darkens the picture so white text reads on it: a fifth for a dark cover, more for a bright one,
    // as much as WCAG's 4.5:1 for white needs on the brightest of the colors (the blobs drift, so
    // any of them can be under the text). It turns with the colors.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, root.dimming)

        Behavior on color {
            ColorAnimation { duration: root.turnMs }
        }
    }

    IconButton {
        x: 20
        y: 14
        glyph: "close"
        tip: qsTr("Close (Esc)")
        tone: "white"
        restFill: Qt.rgba(1, 1, 1, 0.12)
        onClicked: root.closed()
    }

    WindowControls {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 16
        anchors.topMargin: (Theme.headerHeight - height) / 2
        tone: "white"
        restFill: Qt.rgba(1, 1, 1, 0.12)
    }

    // The left half with lyrics, all of it without.
    Item {
        id: stage

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.sideShown ? parent.width * 0.48 : parent.width

        Behavior on width {
            enabled: Theme.animationsOn
            NumberAnimation { duration: Theme.normalMs * 2; easing.type: Easing.InOutCubic }
        }

        ColumnLayout {
            id: column

            readonly property real coverSize: Math.min(440, root.height - 360, stage.width - 120)

            anchors.centerIn: parent
            width: coverSize
            spacing: 0

            Item {
                Layout.preferredWidth: column.coverSize
                Layout.preferredHeight: column.coverSize

                RectangularShadow {
                    anchors.fill: parent
                    radius: 12
                    offset.y: 30
                    blur: 80
                    color: Qt.rgba(0, 0, 0, 0.5)
                    visible: Theme.shadowsOn
                }

                RoundedImage {
                    anchors.fill: parent
                    visible: Player.hasSong
                    radius: 12
                    crossfade: true
                    crossfadeMs: root.turnMs
                    coverArt: Player.coverArt
                }

                // A right click on the cover opens the "…" menu at the pointer.
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    enabled: Player.hasSong
                    onTapped: eventPoint => menu.openAt(parent, eventPoint.position.x, eventPoint.position.y)
                }

                GenCover {
                    anchors.fill: parent
                    visible: !Player.hasSong
                    name: qsTr("Not playing")
                    radius: 12
                    shadow: false
                    live: true
                    colors: root.colors
                }
            }

            // Title and artist on the left, the star and "…" on the right.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 26
                spacing: 6

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Marquee {
                        Layout.fillWidth: true
                        text: Player.hasSong ? Player.title : qsTr("Not playing")
                        color: "white"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }

                    SongByline {
                        Layout.fillWidth: true
                        linkColor: root.dim
                        font.pixelSize: 17
                    }
                }

                IconButton {
                    diameter: 34
                    glyph: Player.starred ? "star_filled" : "star"
                    glyphSize: 20
                    tone: "white"
                    restFill: Qt.rgba(1, 1, 1, 0.12)
                    tip: Player.starred ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
                    enabled: Player.hasSong
                    onClicked: Library.toggleFavorite()
                }

                IconButton {
                    id: more

                    diameter: 34
                    glyph: "more_horiz"
                    glyphSize: 20
                    tone: "white"
                    restFill: Qt.rgba(1, 1, 1, 0.12)
                    tip: qsTr("More")
                    enabled: Player.hasSong
                    onClicked: menu.opened ? menu.close() : menu.openFrom(more, 6, true)

                    PopupMenu {
                        id: menu

                        Action {
                            text: qsTr("Instant Mix")
                            enabled: Player.artists.length > 0
                            onTriggered: Library.instantMixFromPlaying()
                        }
                        PlaylistSubmenu {
                            kind: "song"
                            itemId: Player.songId
                            suggestion: Player.title
                        }

                        MenuDivider {}

                        Action {
                            text: qsTr("Go to Album")
                            enabled: Player.albumId !== ""
                            onTriggered: Navigation.album(Player.albumId)
                        }
                        Action {
                            text: qsTr("Go to Artist")
                            enabled: Player.artists.length > 0
                            onTriggered: Navigation.artist(Player.artists[0].id)
                        }

                        MenuDivider {}

                        Action {
                            text: qsTr("Get Info")
                            onTriggered: Navigation.info("song", Player.songId)
                        }
                        Action {
                            text: qsTr("Search on Spotify")
                            onTriggered: Qt.openUrlExternally("https://open.spotify.com/search/"
                                                              + encodeURIComponent(Player.artist + " " + Player.title))
                        }
                    }
                }
            }

            BarSlider {
                id: seek

                Layout.fillWidth: true
                Layout.topMargin: 18
                from: 0
                to: Math.max(1, Player.duration)
                trackHeight: 5
                fillColor: "white"
                trackColor: Qt.rgba(1, 1, 1, 0.22)
                enabled: Player.hasSong
                Accessible.name: qsTr("Position")
                onPressedChanged: {
                    if (!pressed)
                        Player.seek(value)
                }
                onMoved: {
                    if (!pressed)
                        Player.seek(value)
                }

                Binding on value {
                    when: !seek.pressed
                    value: Player.position
                    restoreMode: Binding.RestoreNone
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4

                Text {
                    text: root.clock(seek.pressed ? seek.value : Player.position)
                    color: root.dim
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }
                // "Mixing" between the times while the old song is still heard, as Apple Music does.
                Item {
                    Layout.fillWidth: true
                    implicitHeight: fullMixing.implicitHeight

                    MixingLabel {
                        id: fullMixing

                        anchors.centerIn: parent
                        color: "white"
                        font.pixelSize: 12
                        running: Player.playing
                        opacity: Player.mixing ? 1 : 0
                        visible: opacity > 0

                        Behavior on opacity {
                            NumberAnimation { duration: Theme.normalMs }
                        }
                    }
                }
                Text {
                    text: "-" + root.clock(Player.duration - (seek.pressed ? seek.value : Player.position))
                    color: root.dim
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                spacing: 14

                IconButton {
                    glyph: "shuffle"
                    glyphSize: 22
                    diameter: 40
                    tone: Player.shuffle ? "white" : root.dim
                    restFill: Player.shuffle ? Qt.rgba(1, 1, 1, 0.16) : "transparent"
                    tip: Player.shuffle ? qsTr("Shuffle is on") : qsTr("Shuffle")
                    onClicked: Player.toggleShuffle()
                }
                IconButton {
                    glyph: "skip_previous"
                    glyphSize: 34
                    diameter: 50
                    tone: "white"
                    tip: qsTr("Previous")
                    enabled: Player.hasSong
                    onClicked: Player.previous()
                }
                IconButton {
                    glyph: Player.playing ? "pause" : "play_arrow"
                    glyphSize: 46
                    diameter: 62
                    tone: "white"
                    tip: Player.playing ? qsTr("Pause") : qsTr("Play")
                    enabled: Player.hasSong
                    onClicked: Player.togglePause()
                }
                IconButton {
                    glyph: "skip_next"
                    glyphSize: 34
                    diameter: 50
                    tone: "white"
                    tip: qsTr("Next")
                    enabled: Player.hasSong
                    onClicked: Player.next()
                }
                IconButton {
                    glyph: Player.repeat === 2 ? "repeat_one" : "repeat"
                    glyphSize: 22
                    diameter: 40
                    tone: Player.repeat > 0 ? "white" : root.dim
                    restFill: Player.repeat > 0 ? Qt.rgba(1, 1, 1, 0.16) : "transparent"
                    tip: [qsTr("Repeat"), qsTr("Repeat all"), qsTr("Repeat this song")][Player.repeat]
                    onClicked: Player.cycleRepeat()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 12
                spacing: 8

                Glyph {
                    name: "volume_down"
                    size: 18
                    color: root.dim
                }

                BarSlider {
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    stepSize: 1
                    value: Player.volume
                    trackHeight: 4
                    fillColor: root.dim
                    trackColor: Qt.rgba(1, 1, 1, 0.22)
                    Accessible.name: qsTr("Volume")
                    onMoved: Player.volume = Math.round(value)
                }

                Glyph {
                    name: "volume_up"
                    size: 18
                    color: root.dim
                }

                // Lyrics (only there when the song has some) and Up Next: each shows its page on the
                // right, and hides it again when it is showing.
                IconButton {
                    Layout.leftMargin: 10
                    visible: Player.lyrics.length > 0
                    diameter: 34
                    glyph: "lyrics"
                    glyphSize: 20
                    tone: root.lyricsShown ? "white" : root.dim
                    restFill: root.lyricsShown ? Qt.rgba(1, 1, 1, 0.16) : "transparent"
                    tip: root.lyricsShown ? qsTr("Hide Lyrics") : qsTr("Show Lyrics")
                    onClicked: root.side = root.lyricsShown ? "" : "lyrics"
                }

                IconButton {
                    objectName: "fullScreenQueueButton"
                    diameter: 34
                    glyph: "format_list_bulleted"
                    glyphSize: 20
                    tone: root.side === "queue" ? "white" : root.dim
                    restFill: root.side === "queue" ? Qt.rgba(1, 1, 1, 0.16) : "transparent"
                    tip: root.side === "queue" ? qsTr("Hide Up Next") : qsTr("Show Up Next")
                    onClicked: root.side = root.side === "queue" ? "" : "queue"
                }
            }
        }
    }

    LyricsView {
        anchors.left: stage.right
        anchors.right: parent.right
        anchors.rightMargin: 64
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 60
        visible: root.lyricsShown
        opacity: root.lyricsShown ? 1 : 0
        textColor: "white"
        lineSize: 30
        gap: 22
        anchor: 0.3

        Behavior on opacity {
            NumberAnimation { duration: Theme.normalMs }
        }
    }

    UpNextList {
        id: queue

        anchors.left: stage.right
        anchors.right: parent.right
        anchors.rightMargin: 64
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 60
        anchors.bottomMargin: 40
        visible: opacity > 0
        opacity: root.side === "queue" ? 1 : 0
        nowPlaying: false
        textColor: "white"
        dimColor: root.dim
        hoverFill: Qt.rgba(1, 1, 1, 0.10)
        clearColor: "#ff8a80"
        titleSize: 22

        Behavior on opacity {
            NumberAnimation { duration: Theme.normalMs }
        }
    }

    WheelScroll {
        flickable: queue
    }
}

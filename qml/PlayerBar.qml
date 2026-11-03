import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts
import Legato

// The floating player bar, compact as Apple Music's on macOS 26: transport on the left; the song in
// the middle (cover, title, artist and album, the quality badge and "…" at the right end, and the
// position as a thin line along the bottom of the block, under the cover too); Up Next and volume
// on the right. Hovering the line thickens it and shows the times in place of the artist line.
// Resting on the speaker grows it to the left into a volume slider. Lyrics are in "…" and the side
// panel. A right click on the song opens the same menu as "…".
Item {
    id: bar

    required property Item source
    // The side panel's page: "", "queue" or "lyrics".
    property string panel

    signal openFullScreen()
    signal togglePanel(string which)

    // 1:05, or 1:02:05 past an hour.
    function clock(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        const h = Math.floor(s / 3600)
        const m = Math.floor(s / 60) % 60
        const ss = String(s % 60).padStart(2, "0")
        return h > 0 ? h + ":" + String(m).padStart(2, "0") + ":" + ss : m + ":" + ss
    }

    implicitHeight: Theme.barHeight

    RectangularShadow {
        anchors.fill: parent
        visible: Theme.shadowsOn
        radius: Theme.radiusBar
        offset.y: 10
        blur: 40
        color: Theme.shadow
    }

    GlassPanel {
        anchors.fill: parent
        source: bar.source
        radius: Theme.radiusBar
        rim: true
    }

    // The bar is solid to the pointer: a click or hover on it never reaches the album under it.
    // The wheel still scrolls the page, except over the volume.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = false
    }

    Row {
        id: transport

        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            diameter: 30
            glyph: "shuffle"
            glyphSize: 17
            tip: Player.shuffle ? qsTr("Shuffle is on") : qsTr("Shuffle")
            tone: Player.shuffle ? Theme.tint : Theme.text2
            onClicked: Player.toggleShuffle()
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            diameter: 30
            glyph: "skip_previous"
            glyphSize: 22
            tip: qsTr("Previous")
            enabled: Player.hasSong
            onClicked: Player.previous()
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            diameter: 36
            glyph: Player.playing ? "pause" : "play_arrow"
            glyphSize: 28
            tip: Player.playing ? qsTr("Pause") : qsTr("Play")
            enabled: Player.hasSong
            onClicked: Player.togglePause()
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            diameter: 30
            glyph: "skip_next"
            glyphSize: 22
            tip: qsTr("Next")
            enabled: Player.hasSong
            onClicked: Player.next()
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            diameter: 30
            glyph: Player.repeat === 2 ? "repeat_one" : "repeat"
            glyphSize: 17
            tip: [qsTr("Repeat"), qsTr("Repeat all"), qsTr("Repeat this song")][Player.repeat]
            tone: Player.repeat > 0 ? Theme.tint : Theme.text2
            onClicked: Player.cycleRepeat()
        }
    }

    Row {
        id: tools

        // Over the volume slider, so the speaker stays a button at its right end.
        z: 3
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            diameter: 30
            glyph: "format_list_bulleted"
            glyphSize: 18
            tip: qsTr("Up Next (Ctrl+J)")
            tone: bar.panel === "queue" ? Theme.tint : Theme.text2
            // Under the open volume slider. Its hover goes too: the row lies above the pill, and a
            // hidden button that still took the hover made the pill close under the pointer.
            enabled: !volume.open
            hoverEnabled: !volume.open
            opacity: volume.open ? 0 : 1
            onClicked: bar.togglePanel("queue")

            Behavior on opacity {
                NumberAnimation { duration: Theme.fastMs }
            }
        }

        // A click mutes and unmutes; resting on it opens the slider to its left; the wheel changes
        // the volume.
        IconButton {
            id: speaker

            property int unmuted: 70

            anchors.verticalCenter: parent.verticalCenter
            objectName: "volumeButton"
            diameter: 30
            glyph: Player.volume === 0 ? "volume_off" : Player.volume < 50 ? "volume_down" : "volume_up"
            glyphSize: 18
            tone: Theme.text2
            // The slider says what the button does; a tooltip would sit on top of it.
            Accessible.name: Player.volume === 0 ? qsTr("Unmute") : qsTr("Mute")
            onClicked: {
                if (Player.volume > 0) {
                    unmuted = Player.volume
                    Player.volume = 0
                } else {
                    Player.volume = unmuted
                }
            }

            WheelHandler {
                target: null
                onWheel: event => Player.volume = Player.volume + Math.round(event.angleDelta.y / 120 * 5)
            }
        }
    }

    // The volume slider: the speaker grows to the left into a pill holding the slider, over the
    // Up Next button and the end of the song block. It opens after a short rest on the speaker, so
    // passing over it does nothing, and stays open while the pointer is on the speaker or the pill.
    Item {
        id: volume

        readonly property bool wanted: speaker.hovered || pillHover.hovered || volumeSlider.pressed
        property bool open: false
        readonly property real openWidth: 150

        objectName: "volumePill"
        z: 2
        x: tools.x + speaker.x + speaker.width + 4 - width
        y: (bar.height - height) / 2
        width: open ? openWidth : speaker.width + 8
        height: speaker.height + 8
        opacity: open ? 1 : 0
        visible: opacity > 0

        onWantedChanged: {
            if (wanted) {
                closing.stop()
                opening.restart()
            } else {
                opening.stop()
                closing.restart()
            }
        }

        Behavior on width {
            enabled: Theme.animationsOn
            NumberAnimation { duration: Theme.normalMs; easing.type: Easing.OutCubic }
        }

        Behavior on opacity {
            NumberAnimation { duration: Theme.fastMs }
        }

        Timer {
            id: opening
            interval: 150
            onTriggered: volume.open = true
        }

        Timer {
            id: closing
            interval: 350
            onTriggered: volume.open = false
        }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Theme.raised
            border.width: 1
            border.color: Theme.line
        }

        HoverHandler {
            id: pillHover
        }

        WheelHandler {
            target: null
            onWheel: event => Player.volume = Player.volume + Math.round(event.angleDelta.y / 120 * 5)
        }

        BarSlider {
            id: volumeSlider

            objectName: "volumeSlider"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 16
            anchors.rightMargin: speaker.width + 12
            anchors.verticalCenter: parent.verticalCenter
            from: 0
            to: 100
            stepSize: 1
            value: Player.volume
            trackHeight: 4
            fillColor: Theme.text2
            enabled: volume.open
            Accessible.name: qsTr("Volume")
            onMoved: Player.volume = Math.round(value)
        }
    }

    // The song, centered in the room between the transport and the tools.
    Item {
        id: song

        readonly property real start: transport.x + transport.width + 14
        readonly property real room: tools.x - 14 - start
        // The line is hovered or held: it thickens and the times replace the artist line.
        readonly property bool scrubbing: Player.hasSong && (scrubHover.hovered || seek.pressed)

        x: start + (room - width) / 2
        y: (bar.height - height) / 2
        width: Math.min(520, room)
        height: 46

        // A right click anywhere on the song opens the "…" menu at the pointer.
        TapHandler {
            acceptedButtons: Qt.RightButton
            enabled: Player.hasSong
            onTapped: eventPoint => songMenu.openAt(song, eventPoint.position.x, eventPoint.position.y)
        }

        // Nothing playing: the song's place stays empty (an error shows there, if one came). No
        // placeholder cover, no "Not playing", no line.
        Text {
            anchors.fill: parent
            visible: !Player.hasSong && Player.error !== ""
            text: Player.error
            color: Theme.red
            font.pixelSize: Theme.fsSmall
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Item {
            id: art

            width: 36
            height: 36
            visible: Player.hasSong

            RectangularShadow {
                anchors.fill: parent
                visible: Theme.shadowsOn && Player.hasSong
                radius: 6
                offset.y: 1
                blur: 5
                color: Qt.rgba(0, 0, 0, 0.22)
            }

            RoundedImage {
                anchors.fill: parent
                radius: 6
                crossfade: true
                // A mix turns the cover as slowly as the songs change.
                crossfadeMs: Player.mixing ? Math.round(Player.mixSeconds * 1000) : Theme.normalMs
                coverArt: Player.coverArt
            }

            TapHandler {
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: bar.openFullScreen()
            }

            HoverHandler {
                id: artHover
                cursorShape: Qt.PointingHandCursor
            }

            ToolTip.text: qsTr("Full-screen player")
            ToolTip.visible: artHover.hovered
            ToolTip.delay: 600
        }

        // The quality badge and "…", at the right end over the line.
        Row {
            id: ends

            anchors.right: parent.right
            anchors.verticalCenter: art.verticalCenter
            spacing: 4
            visible: Player.hasSong
            // Under the open volume slider, which would cut through them.
            opacity: volume.open ? 0 : 1
            enabled: !volume.open

            Behavior on opacity {
                NumberAnimation { duration: Theme.fastMs }
            }

            // The quality badge, or "Mixing" while the old song is still heard under the new one. From
            // the change until the next song the slot keeps the wider of the two, so the title beside
            // it does not narrow or widen (and start its scroll over) while it shows.
            Item {
                id: badgeSlot

                property bool held: false

                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(Player.quality !== "" ? badge.implicitWidth : 0, held ? mixingLabel.implicitWidth : 0)
                height: badge.implicitHeight
                visible: width > 0

                Connections {
                    target: Player

                    function onMixingChanged() {
                        if (Player.mixing)
                            badgeSlot.held = true
                    }
                    function onSongChanged() {
                        badgeSlot.held = Player.mixing
                    }
                }

                QualityBadge {
                    id: badge

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: Player.quality
                    opacity: Player.mixing ? 0 : 1

                    Behavior on opacity {
                        NumberAnimation { duration: Theme.normalMs }
                    }
                }

                MixingLabel {
                    id: mixingLabel

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 10
                    running: Player.playing
                    opacity: Player.mixing ? 1 : 0
                    visible: opacity > 0

                    Behavior on opacity {
                        NumberAnimation { duration: Theme.normalMs }
                    }
                }
            }

            IconButton {
                id: more

                anchors.verticalCenter: parent.verticalCenter
                diameter: 26
                glyph: "more_horiz"
                glyphSize: 20
                tone: Theme.text2
                tip: qsTr("More")
                enabled: Player.hasSong
                onClicked: songMenu.opened ? songMenu.close() : songMenu.openFrom(more, 14, true)

                PopupMenu {
                    id: songMenu

                    Action {
                        text: Player.starred ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
                        onTriggered: Library.toggleFavorite()
                    }
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
                    Action {
                        text: bar.panel === "lyrics" ? qsTr("Hide Lyrics") : qsTr("Show Lyrics")
                        onTriggered: bar.togglePanel("lyrics")
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

        // Title over the artist line, or over the times while scrubbing.
        Item {
            id: words

            anchors.left: art.right
            anchors.leftMargin: 10
            anchors.right: ends.left
            anchors.rightMargin: 8
            height: art.height
            visible: Player.hasSong

            Marquee {
                id: title

                width: parent.width
                y: 0
                text: Player.title
                color: Theme.text
                font.pixelSize: Theme.fsSmall
                font.weight: Font.DemiBold
            }

            SongByline {
                id: artistLine

                anchors.bottom: parent.bottom
                width: parent.width
                opacity: song.scrubbing ? 0 : 1
                visible: opacity > 0
                text: Player.error !== "" ? Player.error : links
                textFormat: Player.error === "" ? Text.RichText : Text.PlainText
                color: Player.error !== "" ? Theme.red : Theme.text2
                font.pixelSize: Theme.fsSmall

                Behavior on opacity {
                    NumberAnimation { duration: Theme.fastMs }
                }
            }

            Item {
                id: times

                objectName: "barTimes"
                anchors.bottom: parent.bottom
                width: parent.width
                height: elapsed.implicitHeight
                opacity: song.scrubbing ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Theme.fastMs }
                }

                Text {
                    id: elapsed

                    text: bar.clock(seek.pressed ? seek.value : Player.position)
                    color: Theme.text2
                    font.pixelSize: Theme.fsTiny
                    font.features: { "tnum": 1 }
                }

                Text {
                    anchors.right: parent.right
                    text: "-" + bar.clock(Player.duration - (seek.pressed ? seek.value : Player.position))
                    color: Theme.text2
                    font.pixelSize: Theme.fsTiny
                    font.features: { "tnum": 1 }
                }
            }
        }

        BarSlider {
            id: seek

            objectName: "seekLine"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -4
            height: 12
            visible: Player.hasSong
            from: 0
            to: Math.max(1, Player.duration)
            trackHeight: song.scrubbing ? 5 : 3
            Accessible.name: qsTr("Position")
            // The knob follows the song except while it is held; the song moves on release.
            onPressedChanged: {
                if (!pressed)
                    Player.seek(value)
            }
            onMoved: {
                if (!pressed)
                    Player.seek(value)
            }

            Behavior on trackHeight {
                NumberAnimation { duration: Theme.fastMs; easing.type: Easing.OutCubic }
            }

            Binding on value {
                when: !seek.pressed
                value: Player.position
                restoreMode: Binding.RestoreNone
            }
        }

        // Where the pointer turns the line into a scrubber: the strip around the line, and while
        // scrubbing also the times above it, so reading them does not hide them again. The artist
        // line above the strip stays free for its links. Declared after the line, whose slider
        // would otherwise take the hover first.
        Item {
            x: -6
            y: song.scrubbing ? words.height - times.height - 2 : art.height
            width: parent.width + 12
            height: bar.height - song.y - y

            HoverHandler {
                id: scrubHover
            }
        }
    }
}

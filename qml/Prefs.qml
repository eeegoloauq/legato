pragma Singleton

import QtQuick
import Legato

// Choices saved in ~/.config/legato/settings.conf besides the appearance (Theme): the sidebar, its
// pins, scrobbling, crossfade and lyrics.
QtObject {
    id: prefs

    // The sidebar shown or slid away (the sidebar button, F9).
    property bool sidebarOpen: true
    // Sidebar rows taken off by their page ("recent", "artists", "albums", "songs", "favorites";
    // "playlists" is the whole Playlists section), comma separated.
    property string hiddenRows: ""
    // Pins of every server, as JSON: { serverKey: [{ kind, id, name, coverArt, hue }] }. Kept here,
    // not on the server: Subsonic has nothing for them.
    property string pinsJson: "{}"
    // The pins of the server in use, in the order they were pinned.
    readonly property var pins: {
        let all = {}
        try {
            all = JSON.parse(pinsJson)
        } catch (e) {
        }
        return all[Library.serverKey] ?? []
    }
    property bool scrobbling: true
    // Automatic by default, as Apple Music's AutoMix: each change fits the songs, and
    // keepAlbumsGapless leaves an album as it was made.
    property bool crossfade: true
    // Fits each change to the songs' outro and intro; crossfadeSeconds is then unused.
    property bool automaticCrossfade: true
    property int crossfadeSeconds: 6
    // 0 Off, 1 Automatic, 2 Fixed, as Settings lists them.
    readonly property int crossfadeMode: !crossfade ? 0 : automaticCrossfade ? 1 : 2

    function setCrossfadeMode(mode) {
        crossfade = mode !== 0
        if (mode !== 0)
            automaticCrossfade = mode === 1
    }
    property bool keepAlbumsGapless: true
    // Off by default: it sends what plays to lrclib.net.
    property bool lrclib: false

    function rowShown(page) {
        return hiddenRows.split(",").indexOf(page) < 0
    }

    function setRowShown(page, shown) {
        const rows = hiddenRows.split(",").filter(row => row !== "" && row !== page)
        if (!shown)
            rows.push(page)
        hiddenRows = rows.join(",")
    }

    function isPinned(kind, id) {
        return pins.some(pin => pin.kind === kind && pin.id === id)
    }

    // `kind` is "album", "artist" or "playlist", as the page it opens.
    function pin(kind, id, name, coverArt, hue) {
        if (!id || Library.serverKey === "" || isPinned(kind, id))
            return
        savePins(pins.concat([{ kind: kind, id: id, name: name, coverArt: coverArt ?? "", hue: hue ?? 0 }]))
    }

    function unpin(kind, id) {
        savePins(pins.filter(pin => pin.kind !== kind || pin.id !== id))
    }

    function savePins(list) {
        let all = {}
        try {
            all = JSON.parse(pinsJson)
        } catch (e) {
        }
        if (list.length > 0)
            all[Library.serverKey] = list
        else
            delete all[Library.serverKey]
        pinsJson = JSON.stringify(all)
    }

    readonly property list<StoredSetting> stored: [
        StoredSetting { category: "shell"; property alias sidebarOpen: prefs.sidebarOpen },
        StoredSetting { category: "shell"; property alias hiddenRows: prefs.hiddenRows },
        StoredSetting { category: "shell"; property alias pins: prefs.pinsJson },
        StoredSetting { category: "playback"; property alias scrobbling: prefs.scrobbling },
        StoredSetting { category: "playback"; property alias crossfade: prefs.crossfade },
        StoredSetting { category: "playback"; property alias automaticCrossfade: prefs.automaticCrossfade },
        StoredSetting { category: "playback"; property alias crossfadeSeconds: prefs.crossfadeSeconds },
        StoredSetting { category: "playback"; property alias keepAlbumsGapless: prefs.keepAlbumsGapless },
        StoredSetting { category: "lyrics"; property alias lrclib: prefs.lrclib }
    ]

    onScrobblingChanged: Library.scrobbling = scrobbling
    onLrclibChanged: Library.lrclib = lrclib
    Component.onCompleted: {
        Library.scrobbling = scrobbling
        Library.lrclib = lrclib
    }

    readonly property Binding crossfadeBinding: Binding {
        target: Player
        property: "crossfade"
        // Automatic falls back to a 6 s fade where it cannot measure the songs.
        value: !prefs.crossfade ? 0 : prefs.automaticCrossfade ? 6 : prefs.crossfadeSeconds
    }

    readonly property Binding automaticBinding: Binding {
        target: Player
        property: "automaticCrossfade"
        value: prefs.automaticCrossfade
    }

    readonly property Binding gaplessBinding: Binding {
        target: Player
        property: "keepAlbumsGapless"
        value: prefs.keepAlbumsGapless
    }
}

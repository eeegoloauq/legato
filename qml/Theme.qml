pragma Singleton

import QtQuick
import Legato

// Design tokens (docs/design.md, "Visual tokens"). Components use these and define no raw values.
QtObject {
    id: theme

    // 0 system, 1 light, 2 dark. Saved in ~/.config/legato/settings.conf.
    readonly property list<StoredSetting> stored: [
        StoredSetting { category: "appearance"; property alias appearance: theme.savedAppearance },
        StoredSetting { category: "appearance"; property alias coverColors: theme.coverColors }
    ]
    property int savedAppearance: 0
    // Pages of an album, artist or playlist take colors from the cover: the window's backdrop and
    // the tint at the top. Off, every page keeps the plain background.
    property bool coverColors: true
    // --appearance overrides the saved choice without saving it (development).
    readonly property int forced: {
        const args = Qt.application.arguments
        for (let i = 0; i < args.length; ++i) {
            let value = ""
            if (args[i] === "--appearance" && i + 1 < args.length)
                value = args[i + 1]
            else if (args[i].startsWith("--appearance="))
                value = args[i].slice(13)
            if (value !== "")
                return ["system", "light", "dark"].indexOf(value)
        }
        return -1
    }
    readonly property int appearance: forced >= 0 ? forced : savedAppearance
    readonly property bool dark: appearance === 2
        || (appearance === 0 && Application.styleHints.colorScheme === Qt.ColorScheme.Dark)

    // M0 switches, set by the frame meter. Each turns one costly effect off.
    property bool blurOn: true
    property bool shadowsOn: true
    property bool animationsOn: true

    // Neutral greys: no blue or violet bias. The sidebar and the content are one surface, split by
    // a hairline, so a page's backdrop (an album's color) can reach under both.
    readonly property color bg: dark ? "#161616" : "#fafafa"
    readonly property color raised: dark ? "#262626" : "#ffffff"
    readonly property color text: dark ? "#f5f5f5" : "#1d1d1d"
    readonly property color text2: dark ? "#a1a1a1" : "#6e6e6e"
    readonly property color text3: dark ? "#6b6b6b" : "#a3a3a3"
    readonly property color line: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color fill: dark ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(0, 0, 0, 0.045)
    readonly property color fill2: dark ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(0, 0, 0, 0.09)
    // No hue of its own: primary buttons, focus rings and selections use the text color. Color in
    // the app comes from the covers (tint, backdrop).
    readonly property color accent: text
    // The playing cover's color overrides the accent for playback controls.
    readonly property color tint: {
        const own = dark ? Player.accentDark : Player.accentLight
        return own.valid ? own : accent
    }
    // The window's background: the current page's backdrop (an album's color), else bg. Main sets
    // it; the sidebar, the side panel and the glass follow it.
    property color backdrop: bg
    // A page background from a cover's main color: dark and muted, or pale in light mode. Grey
    // covers give grey.
    function backdropFrom(color) {
        const c = Qt.color(color)
        return Qt.hsla(Math.max(0, c.hslHue), c.hslSaturation * (dark ? 0.42 : 0.5), dark ? 0.115 : 0.955, 1)
    }
    // Text on the accent or tint color. (A name starting with "on" would be read as a signal handler.)
    readonly property color textOnAccent: dark ? "#161616" : "#ffffff"
    readonly property color red: dark ? "#ff6961" : "#d70015"
    readonly property color green: dark ? "#30d158" : "#248a3d"

    // Glass: a strong blur under a dense tint, as Apple's materials.
    // Dense and neutral, as Apple's bar: the content shows as light and shade, not as its colors.
    // Made from the backdrop, so the bars over an album's page carry its color faintly.
    readonly property color glass: dark ? Qt.alpha(Qt.tint(backdrop, Qt.rgba(1, 1, 1, 0.06)), 0.80)
                                        : Qt.alpha(Qt.tint(backdrop, Qt.rgba(1, 1, 1, 0.6)), 0.82)
    readonly property color glassEdge: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(1, 1, 1, 0.75)
    readonly property real glassBlur: 64
    // Stretches the blur beyond MultiEffect's largest radius (64 px) to about twice that: at
    // 64 px the covers still showed through.
    readonly property real glassBlurMultiplier: 1
    // Below zero mutes the colors under the glass. At +0.45 a bright cover tinted the whole bar.
    readonly property real glassSaturation: -0.5
    readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.45) : Qt.rgba(0, 0, 0, 0.14)

    readonly property int fs: 14
    readonly property int fsSmall: 13
    readonly property int fsTiny: 11
    readonly property int fsTitle: 34
    readonly property int fsSection: 21

    readonly property real radiusSmall: 6
    readonly property real radius: 10
    readonly property real radiusArt: 8
    readonly property real radiusCard: 14
    readonly property real radiusBar: 22

    readonly property real sidebarWidth: 236
    readonly property real barWidth: 760
    readonly property real barHeight: 58
    readonly property real panelWidth: 330
    readonly property real headerHeight: 52
    readonly property real gutter: 32
    // Covers keep their size and the width decides how many fit: as many columns of at least
    // coverMin as there is room for, up to maxColumns (four in the default window, five on a
    // maximized 1080p screen). A narrow window shows fewer covers, not smaller ones. Grids and
    // carousels share the rule, so a cover is about the same size everywhere.
    readonly property real coverMin: 200
    readonly property int maxColumns: 5
    readonly property real columnGap: 22
    // Covers in a row `width` wide (no gap after the last one).
    function columnsFor(width) {
        return Math.max(2, Math.min(maxColumns, Math.floor((width + columnGap) / (coverMin + columnGap))))
    }
    readonly property real rowGap: 28

    // Hover answers at once; other motion uses short eases.
    readonly property int hoverMs: 70
    readonly property int fastMs: 140
    readonly property int normalMs: 260
    // The side panel sliding in or out, with the content following it.
    readonly property int panelMs: 320
    // One full drift cycle of generated covers.
    readonly property int driftMs: 14000

    // A server's status dot (Session.servers): answers, does not, not known.
    function statusColor(status) {
        return status === "up" ? green : status === "down" ? red : text3
    }

    function icon(name) {
        return "qrc:/qt/qml/Legato/icons/" + name + ".svg"
    }
}

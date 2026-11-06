pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Legato

// The shell (docs/design.md, "Shell"): the sidebar, pages in the content area under a glass header
// and player bar, the side panel, the full-screen player and the frame meter.
ApplicationWindow {
    id: window

    property bool fullScreenOpen: Qt.application.arguments.indexOf("--full-screen") >= 0
    // The right panel: open or not, and which page it shows ("queue" or "lyrics").
    // --panel queue|lyrics opens it at start (development).
    readonly property int panelArg: Qt.application.arguments.indexOf("--panel")
    property bool panelOpen: panelArg >= 0
    property string panelPage: panelArg >= 0 ? Qt.application.arguments[panelArg + 1] || "queue" : "queue"

    // Under this width the sidebar takes no room from the content: it stays away, and the sidebar
    // button (F9) opens it over the content until a page is chosen or the content is clicked.
    readonly property bool narrow: width < 1000
    property bool sidebarFloating: false
    readonly property bool sidebarShown: narrow ? sidebarFloating : Prefs.sidebarOpen
    // The content leaves room for the sidebar from the moment it starts to show until it is gone.
    readonly property bool sidebarSpace: !narrow && (Prefs.sidebarOpen || sidebar.visible)

    function toggleSidebar() {
        if (narrow)
            sidebarFloating = !sidebarFloating
        else
            Prefs.sidebarOpen = !Prefs.sidebarOpen
    }

    onNarrowChanged: sidebarFloating = false

    function togglePanel(page) {
        if (panelOpen && panelPage === page) {
            panelOpen = false
        } else {
            panelPage = page
            panelOpen = true
        }
    }

    // The one history for back and forward (docs/design.md, "Navigation"): every page visited,
    // oldest first, as { route, offset, page, seen }, and `at`, the one showing. Only the pages
    // seen last stay made (keptPages), so back to them is instant; the others are a route and a
    // scroll offset and are made again when gone back to. Kept pages wait hidden in `parked`, where a
    // change of the content's width (the side panel, the window) does not lay them out again
    // (docs/architecture.md).
    readonly property int keptPages: 4
    readonly property int historyLength: 100
    property var history: []
    property int at: -1
    property int seen: 0
    readonly property bool canGoBack: at > 0
    readonly property bool canGoForward: at < history.length - 1
    readonly property ContentPage currentPage: pages.currentItem as ContentPage

    // Shows history[at], made again if it was let go.
    function show(immediate) {
        const entry = history[at]
        // A page sized only by the stack drops to its implicit size when it leaves, and one with
        // none (0) laid its grid out again twice per visit. Made and left at the content's size,
        // a page is laid out again only if the window changed while it waited.
        const size = { implicitWidth: pages.width, implicitHeight: pages.height }
        if (pages.currentItem)
            Object.assign(pages.currentItem, size)
        if (!entry.page) {
            entry.page = pageFor(entry.route).createObject(parked, Object.assign({ route: entry.route }, size))
            entry.page.restore(entry.offset)
        }
        entry.seen = ++seen
        pages.replace(null, entry.page, {}, immediate ? StackView.Immediate : StackView.Transition)
        const made = history.filter(other => other.page).sort((a, b) => b.seen - a.seen)
        for (const old of made.slice(keptPages))
            release(old)
    }

    function release(entry) {
        if (!entry.page)
            return
        entry.offset = entry.page.offset()
        entry.page.destroy()
        entry.page = null
    }

    // Before leaving the page showing: where it was scrolled to, for when it is made again.
    function leave() {
        const entry = history[at]
        if (entry && entry.page)
            entry.offset = entry.page.offset()
    }

    function open(route) {
        const page = pageFor(route)
        if (!page || (currentPage && JSON.stringify(currentPage.route) === JSON.stringify(route)))
            return
        leave()
        // A new page drops what was ahead of the current one, as a browser does, and the oldest
        // pages past historyLength.
        const kept = history.slice(0, at + 1)
        for (const gone of history.slice(at + 1))
            release(gone)
        kept.push({ route: route, offset: 0, page: null, seen: 0 })
        for (const gone of kept.slice(0, Math.max(0, kept.length - historyLength)))
            release(gone)
        history = kept.slice(-historyLength)
        at = history.length - 1
        show(history.length === 1)
    }

    function back() {
        if (!canGoBack || pages.busy)
            return
        leave()
        at = at - 1
        show(false)
    }

    function forward() {
        if (!canGoForward || pages.busy)
            return
        leave()
        at = at + 1
        show(false)
    }

    function pageFor(route) {
        return {
            home: homePage, recent: recentPage, albums: albumsPage, album: albumPage, artists: artistsPage,
            artist: artistPage, songs: songsPage, favorites: favoritesPage, playlists: playlistsPage,
            playlist: playlistPage, settings: settingsPage
        }[route.page] ?? null
    }

    // --open albums or --open album:<id> starts there (development); --run-test needs the album grid.
    readonly property var startRoute: {
        const args = Qt.application.arguments
        const at = args.indexOf("--open")
        const value = String(at >= 0 && at + 1 < args.length ? args[at + 1] : args.indexOf("--run-test") >= 0 ? "albums" : "")
        if (value === "")
            return null
        const colon = value.indexOf(":")
        return colon < 0 ? { page: value } : { page: value.slice(0, colon), id: value.slice(colon + 1) }
    }

    width: 1280
    height: 860
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Legato"
    // legato draws its own drag area and window buttons, so GNOME adds no header bar above the design.
    flags: Qt.Window | Qt.FramelessWindowHint
    color: Theme.bg
    font.pixelSize: Theme.fs

    // The window's ground: the page's backdrop (an album's color) or the plain background, under
    // the sidebar, the content and the side panel alike. It changes color gently between pages.
    background: Rectangle {
        color: Theme.backdrop

        Behavior on color {
            enabled: Theme.animationsOn
            ColorAnimation { duration: Theme.normalMs * 2; easing.type: Easing.OutCubic }
        }
    }

    Binding {
        target: Theme
        property: "backdrop"
        value: Theme.coverColors && window.currentPage && window.currentPage.backdrop.a > 0
               ? window.currentPage.backdrop : Theme.bg
    }
    palette.toolTipBase: Theme.raised
    palette.toolTipText: Theme.text

    Component.onCompleted: {
        Session.start()
        open({ page: "home" })
        if (startRoute)
            open(startRoute)
    }

    Connections {
        target: Navigation

        function onRequested(route) {
            window.fullScreenOpen = false
            window.open(route)
        }

        function onInfoRequested(kind, id) {
            infoCard.show(kind, id)
        }

        function onNameRequested(title, action, initial, done) {
            askDialog.askName(title, action, initial, done)
        }

        function onConfirmRequested(title, detail, action, done) {
            askDialog.confirm(title, detail, action, done)
        }
    }

    Connections {
        target: Library

        function onNotice(text) {
            notice.show(text)
        }

        function onPlaylistDeleted(id) {
            Prefs.unpin("playlist", id)
            const route = window.currentPage ? window.currentPage.route : null
            if (route && route.page === "playlist" && route.id === id)
                window.open({ page: "playlists" })
        }
    }

    // Everything under a layer that covers the window (the full-screen player, the sign-in card) is
    // hidden while it is there, not only covered. Qt hands a press to the pointer handlers of items
    // below the one that took it (qquickdeliveryagent.cpp), so a MouseArea on top did not stop the
    // TapHandler of an album card behind the full-screen player. Hidden items get no events at all,
    // and nothing nobody sees is drawn.
    Item {
        id: shell

        anchors.fill: parent
        visible: !window.fullScreenOpen && Session.state !== Session.SignIn
        // The keyboard goes back to the page once it shows again (hidden items take no focus).
        onVisibleChanged: if (visible && window.currentPage) window.currentPage.forceActiveFocus()

        Sidebar {
            id: sidebar

            // 0 hidden, 1 shown. Like the side panel, the content changes width once: hiding, the
            // sidebar slides out to the left and the content widens when it is gone; showing, the
            // content narrows at once and the sidebar slides into the space.
            property real reveal: window.sidebarShown ? 1 : 0

            x: -width * (1 - reveal)
            visible: reveal > 0
            z: window.narrow ? 3 : 0
            floating: window.narrow
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Theme.sidebarWidth
            route: window.currentPage ? window.currentPage.route : ({ page: "" })
            onNavigate: route => {
                window.sidebarFloating = false
                window.open(route)
            }

            Behavior on reveal {
                enabled: Theme.animationsOn
                NumberAnimation { duration: Theme.panelMs; easing.type: Easing.InOutCubic }
            }
        }

        Item {
            id: content

            anchors.left: parent.left
            anchors.leftMargin: window.sidebarSpace ? sidebar.width : 0
            // The side panel sits beside the content, as Apple Music's does, so nothing lies under it.
            // The content changes width once, not with every frame of the slide (a grid laid out again
            // 60 times a second flickered): opening, the panel slides in over the content, which
            // narrows when it is in place; closing, the content widens at once under the panel, which
            // then slides away.
            anchors.right: parent.right
            anchors.rightMargin: window.panelOpen && sidePanel.reveal === 1 ? sidePanel.width : 0
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            StackView {
                id: pages

                anchors.fill: parent
                focus: true
                // Carousels slide out of view at the content's edge, not over the sidebar or the panel.
                clip: true
                onCurrentItemChanged: if (currentItem) currentItem.forceActiveFocus()

                // The old page goes at once and the new one fades in over the background. A crossfade
                // showed both pages at half strength, one through the other.
                // The stack holds only the page showing; Main keeps the history (window.show).
                replaceEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.fastMs } }
                replaceExit: Transition { PropertyAction { property: "opacity"; value: 0 } }
            }

            // The pages the history keeps made while they do not show (window.keptPages).
            Item {
                id: parked

                visible: false
            }

            UnreachableView {
                anchors.fill: parent
                visible: Session.state === Session.Unreachable
            }

            TopBar {
                id: topBar

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                source: pages
                title: window.currentPage ? window.currentPage.title : ""
                page: window.currentPage
                windowControls: !sidePanel.visible
                // Once the page title has scrolled under the header.
                scrolled: {
                    const view = window.currentPage ? window.currentPage.flickable : null
                    return view !== null && view.contentY - view.originY > Theme.headerHeight - 8
                }
                leftInset: window.sidebarSpace ? 20 : sidebarButton.x + sidebarButton.width + 8
                canGoBack: window.canGoBack
                canGoForward: window.canGoForward
                onBackRequested: window.back()
                onForwardRequested: window.forward()
                onActionRequested: name => {
                    switch (name) {
                    case "lyrics":
                    case "queue":
                        window.panelPage = name
                        window.panelOpen = true
                        break
                    case "fullScreen":
                        window.fullScreenOpen = true
                        break
                    case "settings":
                        window.open({ page: "settings" })
                        break
                    }
                }
            }

            Notice {
                id: notice

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: playerBar.top
                anchors.bottomMargin: 12
                source: pages
            }

            PlayerBar {
                id: playerBar

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16
                width: Math.min(Theme.barWidth, parent.width - 32)
                source: pages
                panel: window.panelOpen ? window.panelPage : ""
                onOpenFullScreen: window.fullScreenOpen = true
                onTogglePanel: page => window.togglePanel(page)
            }

            // The M0 frame meter, for checking performance: --frame-meter or F2 shows it.
            Loader {
                id: meterPanel

                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 16
                anchors.topMargin: Theme.headerHeight + 12
                active: Qt.application.arguments.indexOf("--frame-meter") >= 0
                        || Qt.application.arguments.indexOf("--run-test") >= 0
                sourceComponent: FrameMeterPanel {
                    objectName: "frameMeter"
                    view: window.currentPage ? window.currentPage.flickable as GridView : null
                    onFullScreenRequested: open => window.fullScreenOpen = open
                }
            }
        }

        // A click on the content closes the sidebar lying over it.
        MouseArea {
            anchors.fill: parent
            z: 2
            visible: window.sidebarFloating
            onClicked: window.sidebarFloating = false
        }

        // Over the sidebar's top strip, its glyph in line with the rows' glyphs, and in the same place
        // while the sidebar is away: then back and forward move over to make room.
        IconButton {
            id: sidebarButton

            x: 15
            y: (Theme.headerHeight - height) / 2
            glyph: "dock_to_right"
            z: 4
            tip: window.sidebarShown ? qsTr("Hide Sidebar (F9)") : qsTr("Show Sidebar (F9)")
            tone: Theme.text2
            onClicked: window.toggleSidebar()
        }

        // Only while it slides over the content; in place, a hairline divides them.
        Shadow {
            anchors.fill: sidePanel
            visible: sidePanel.visible && sidePanel.reveal < 1 && Theme.shadowsOn
            offset.x: -8
            blur: 32
            color: Theme.shadow
        }

        SidePanel {
            id: sidePanel

            // 0 closed, 1 open: it slides in from the right edge and out again.
            property real reveal: window.panelOpen ? 1 : 0

            x: parent.width - width * reveal
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Theme.panelWidth
            visible: reveal > 0
            page: window.panelPage

            Behavior on reveal {
                enabled: Theme.animationsOn
                NumberAnimation { duration: Theme.panelMs; easing.type: Easing.InOutCubic }
            }
            onCloseRequested: window.panelOpen = false
            onPageRequested: page => window.panelPage = page
        }
    }

    Loader {
        anchors.fill: parent
        active: window.fullScreenOpen
        // Takes the keyboard from the grid behind it, so Esc reaches the player and arrows or
        // Enter do not move or play albums nobody sees.
        onLoaded: (item as Item).forceActiveFocus()
        sourceComponent: FullScreenPlayer {
            onClosed: window.fullScreenOpen = false
        }
    }

    // The sign-in card covers the whole window: no sidebar, no player bar until a server answers.
    Loader {
        anchors.fill: parent
        active: Session.state === Session.SignIn
        // Once the view is in the window: focus does not stick to an item outside it.
        onLoaded: (item as SignInView).focusFirstField()
        sourceComponent: SignInView {}
    }

    AskDialog {
        id: askDialog
    }

    InfoCard {
        id: infoCard
    }

    ResizeEdges {}

    Component {
        id: homePage

        HomePage {}
    }

    Component {
        id: recentPage

        AlbumsPage {
            recent: true
        }
    }

    Component {
        id: albumsPage

        AlbumsPage {}
    }

    Component {
        id: albumPage

        AlbumPage {}
    }

    Component {
        id: artistsPage

        ArtistsPage {}
    }

    Component {
        id: artistPage

        ArtistPage {}
    }

    Component {
        id: songsPage

        SongsPage {}
    }

    Component {
        id: favoritesPage

        FavoritesPage {}
    }

    Component {
        id: playlistsPage

        PlaylistsPage {}
    }

    Component {
        id: playlistPage

        PlaylistPage {}
    }

    Component {
        id: settingsPage

        SettingsPage {}
    }

    Shortcut {
        sequence: "F2"
        onActivated: meterPanel.active = !meterPanel.active
    }

    // The keyboard map of docs/design.md, as far as the features exist.
    Shortcut {
        sequence: "Space"
        onActivated: Player.togglePause()
    }
    Shortcut {
        sequence: "Ctrl+Right"
        onActivated: Player.next()
    }
    Shortcut {
        sequence: "Ctrl+Left"
        onActivated: Player.previous()
    }
    Shortcut {
        sequence: "Shift+Right"
        onActivated: Player.seek(Player.position + 10)
    }
    Shortcut {
        sequence: "Shift+Left"
        onActivated: Player.seek(Player.position - 10)
    }
    Shortcut {
        sequence: "Ctrl+Up"
        onActivated: Player.volume = Player.volume + 5
    }
    Shortcut {
        sequence: "Ctrl+Down"
        onActivated: Player.volume = Player.volume - 5
    }
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: Player.toggleShuffle()
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        onActivated: Library.shuffleLibrary()
    }
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: Player.cycleRepeat()
    }
    Shortcut {
        sequence: "Ctrl+L"
        onActivated: window.togglePanel("lyrics")
    }
    Shortcut {
        sequence: "Ctrl+J"
        onActivated: window.togglePanel("queue")
    }
    Shortcut {
        sequence: "Alt+Left"
        onActivated: window.back()
    }
    Shortcut {
        sequence: "Alt+Right"
        onActivated: window.forward()
    }
    Shortcut {
        sequence: "Ctrl+1"
        onActivated: window.open({ page: "home" })
    }
    Shortcut {
        sequence: "Ctrl+2"
        onActivated: window.open({ page: "albums" })
    }
    Shortcut {
        sequence: "Ctrl+3"
        onActivated: window.open({ page: "artists" })
    }
    Shortcut {
        sequence: "Ctrl+4"
        onActivated: window.open({ page: "songs" })
    }
    Shortcut {
        sequence: "Ctrl+5"
        onActivated: window.open({ page: "playlists" })
    }
    Shortcut {
        sequence: "F9"
        onActivated: window.toggleSidebar()
    }
    Shortcut {
        sequence: "Ctrl+,"
        onActivated: window.open({ page: "settings" })
    }
    // The search in the header (docs/design.md, "Search"). A text field keeps / for typing; the
    // shortcut fires only where it types nothing. From the full-screen player, it goes back first.
    Shortcut {
        sequences: ["Ctrl+K", "/", "Ctrl+F"]
        enabled: Session.state === Session.Ready || Session.state === Session.Unreachable
        onActivated: {
            window.fullScreenOpen = false
            topBar.search()
        }
    }
    // A song list with a chosen row takes Ctrl+I for that song first.
    Shortcut {
        sequence: "Ctrl+I"
        enabled: Player.hasSong
        onActivated: Navigation.info("song", Player.songId)
    }
    // Home's rows change only while no Home is on screen.
    Binding {
        target: HomeFeed
        property: "showing"
        value: window.currentPage !== null && window.currentPage.route.page === "home" && !window.fullScreenOpen
    }

    // The full-screen player's Esc is the window's, not the player's: a click on a slider or a
    // menu that closes can leave the keyboard elsewhere, and Esc then did nothing.
    Shortcut {
        sequence: "Esc"
        enabled: window.fullScreenOpen
        onActivated: window.fullScreenOpen = false
    }
    Shortcut {
        sequence: "Esc"
        enabled: window.panelOpen && !window.fullScreenOpen
        onActivated: window.panelOpen = false
    }
    Shortcut {
        sequence: "Esc"
        enabled: window.sidebarFloating && !window.panelOpen && !window.fullScreenOpen
        onActivated: window.sidebarFloating = false
    }
}

import QtQuick

// A page of the content area (docs/design.md, "Navigation"). Main shows pages in a StackView with
// one history for back and forward; the header reads `title` and `flickable` for its scrolled look,
// and the bars blur whatever the page draws.
FocusScope {
    // Where the page is in the app, as the history stores it: { page: "home" }, later with an id.
    property var route: ({ page: "" })
    property string title
    property Flickable flickable: null
    // The window's background while the page shows (an album's color); transparent for the plain one.
    property color backdrop: "transparent"
    // Find in page (docs/design.md, "Song lists"): a page with a song list sets findable, and the
    // header's find field edits findText. Down or Enter in the field commit: the list chooses its
    // first match, and Enter plays it.
    property bool findable: false
    property string findText
    signal findCommitted(bool play)
}

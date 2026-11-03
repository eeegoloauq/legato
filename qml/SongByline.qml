import QtQuick
import Legato

// The playing song's "Artist & Artist — Album" line: each artist and the album is a link, the one
// under the pointer underlined (docs/design.md, "Player bar" and "Full-screen player"). Names
// without an id are plain text. Used by the player bar and the full-screen player.
Marquee {
    id: byline

    // The color of the names; the links take it too.
    property color linkColor: Theme.text2

    readonly property string links: {
        const color = byline.linkColor
        const escape = text => text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        const link = (href, text) => href === "" ? escape(text)
            : "<a href=\"" + href + "\" style=\"color:" + color + ";text-decoration:"
              + (byline.hoveredLink === href ? "underline" : "none") + "\">" + escape(text) + "</a>"
        const artists = Player.artists.length > 0 ? Player.artists : [{ id: "", name: Player.artist }]
        const names = artists.map(artist => link(artist.id ? "artist:" + artist.id : "", artist.name))
        let line = names.length > 1 ? names.slice(0, -1).join(", ") + " &amp; " + names[names.length - 1] : names[0]
        if (Player.album !== "")
            line += " — " + link(Player.albumId ? "album:" + Player.albumId : "", Player.album)
        return line
    }

    text: links
    textFormat: Text.RichText
    color: linkColor
    onLinkActivated: link => {
        if (link.startsWith("artist:"))
            Navigation.artist(link.slice(7))
        else if (link.startsWith("album:"))
            Navigation.album(link.slice(6))
    }
}

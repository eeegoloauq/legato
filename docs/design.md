# Design

A sidebar, large artwork, a floating translucent player bar and a lot of empty space, in the manner
of current desktop music apps. Light, dark and system themes. Color comes from the covers; the
interface itself is neutral.

## Shell

- **Sidebar**, top to bottom:
  - a thin strip for dragging the window, with the sidebar button at its left end: it slides the
    sidebar away and back (F9, remembered). The button stays in place while the sidebar is away.
  - Home, Search (opens the search palette)
  - Pinned: albums, artists (round photo) and playlists pinned from their "…" menu, in the order
    they were pinned. Pins are kept on this computer, per server and account. A right click on a pin
    offers Unpin.
  - Library: Recently Added, Artists, Albums, Songs, Favorites
  - Playlists: All Playlists, then each playlist with a small cover
  - Settings at the bottom, a row like the others

  Library rows and the Playlists section can be hidden: a right click offers Hide from Sidebar and
  Sidebar Settings…, and Settings has a switch per row. A section with no rows left loses its title.
  The sidebar has no background of its own; it and the content are one surface, split by a hairline.
- **Content** scrolls under a sticky header with back and forward at the top left. When the page
  scrolls, the header turns translucent and shows the page title.
- **Navigation**: every page, Settings included, opens in the content area on one history. Back
  returns to the previous page as it was left, scroll position included.
- **Player bar** floats over the bottom of the content. Confirmations ("Added to Road Trip") show
  for a moment as a glass pill just above it.
- **Glass**: the player bar, the scrolled header, menus, dropdowns and the search's results blur
  and tint what is under them. The bars are dense and neutral; what opens for a moment (menus, the
  search's results, the confirmation pill) is thinner, so the color under it shows. Settings >
  Transparency turns the blur off (a solid tint), for older graphics.
- **Moving the window**: there is no title bar. A drag on the header's empty parts, the sidebar's
  top strip or the side panel's moves the window, a double click maximizes it. A drag that starts
  on a control (a text field, a slider) stays with it.
- **Side panel** for Up Next and Lyrics sits beside the content, full height, and slides in from the
  window's edge. The content changes width once per opening or closing, never with each frame of the
  slide.
- **Layers that cover the window** (the full-screen player, the sign-in card) hide everything under
  them while open. They move the window themselves (drag, double click to maximize) and have their
  own window buttons.
- **Page changes**: the old page goes at once and the new one fades in. Pages with details (album,
  artist, playlist) show once their details are in, never filled in piece by piece.

Sidebar icons are in the text color; the current page's row is lit by its fill and bold text.

## Player bar

58 px tall, up to 760 px wide, radius 22 px, frosted glass that mutes the colors under it, so a
bright cover does not color the bar. The bar takes every click and hover over it; the wheel over it
still scrolls the page. With nothing playing it holds only the dimmed controls and the Up Next and
volume buttons.

- Left: shuffle, previous, play/pause, next, repeat (off / all / one).
- Center, at most 520 px: cover (opens the full-screen player), the song title over "artist —
  album", and at the right end the quality badge ("Lossless", "Hi-Res Lossless", "MP3 320") and "…".
  The position is a thin line along the bottom of the block. Hovering the line thickens it and shows
  elapsed and remaining time in place of the artist line.
  - Each artist and the album are links. The song title is not.
  - A line that does not fit rests, glides left until its end shows, rests and glides back.
- During a crossfade of 2 s or more, "Mixing" takes the quality badge's place, with a highlight
  sweeping across it, and the cover dissolves over the whole change.
- Right: Up Next, then the speaker. Resting on the speaker grows it into a pill with a volume
  slider. A click mutes; the wheel changes the volume.
- A right click on the song opens its "…" menu.
- The progress line and the controls that are on use the color of the playing cover.

## Home

- "Home" with a Shuffle button beside it (shuffles the whole library, Ctrl+Shift+S).
- Rows: Recently added ›, Random albums (with a refresh button), Most played, Playlists ›. Titles
  with › open the full page. An empty row is left out.

## Covers and carousels

- Covers keep their size and the width decides how many fit. Grids show as many columns of at least
  200 px covers as fit, at most five. Carousels show the grid's columns plus 0.4 of the next cover.
- Placeholders are a neutral fill; a cover fades in once decoded. A cover that changes in place (the
  player bar, the full-screen player) crossfades from the old one.
- Carousels have arrow buttons at their edges on hover and no scroll bar.
- Hover lifts a cover and shows a play button (40 px) and a smaller "…" (32 px), dark enough to show
  on a pale cover.
- Playlists without artwork get a generated cover: soft blurred color blobs with the name in white,
  drifting on the playlist page and in the full-screen player.

## Album page

- The window takes the cover's main color as its background while the album shows (dark and muted
  in dark mode, a pale wash in light mode), and the top of the page is tinted with the cover.
- Cover on the left; beside it "Album", title, artist link, the main genre and year, quality badge.
- The album comment in one line with "More". Rip and tool comments (EAC, URLs) are hidden.
- Play, Shuffle, favorite star, "…". Track list with numbers and durations, by disc.
- Footer: "9 songs, 41 minutes, 312 MB · FLAC · 24-bit / 96 kHz".

## Artist page

- Round photo, name, Play, Shuffle and a favorite star.
- Latest Release on the left; Top Songs as a grid of three rows on the right. Top songs use the
  listener's play counts first and Last.fm's top tracks as fallback.
- Albums, then Singles & EPs, then Appears On (other artists' albums with songs of this artist), as
  carousels.

## Playlist page

- Cover (generated if the playlist has none), name, description or "Updated <date>", Play and
  Shuffle. Rows: cover, title, "album – artist", time, "…".

## Song lists

- A click selects a row. Double-click or Enter plays it; the track number turns into a play button
  on hover.
- ↑/↓ move between rows. "…", a right click or Shift+F10 opens the song menu. Ctrl+I opens Get info.
- The playing row shows a small level meter in the playing cover's color.

### Find in page

On the Album, Playlist and Favorites pages a magnifier sits at the right end of the header; a click
or Ctrl+F opens it into a field. A song matches when every word is in its title, artists or album,
ignoring case and accents. ↓ goes to the first match, Enter plays it, Esc clears the field. Playing a
match plays the whole album or playlist from that song. Elsewhere Ctrl+F opens the search palette.

## Menus

- Song: Play Next, Add to Queue, Add to Playlist ›, Instant Mix, Go to Album, Go to Artist,
  favorite, Get Info, Search on Spotify.
- Album: Play Next, Add to Queue, Add to Playlist ›, Instant Mix, Go to Artist, favorite, Pin to
  Sidebar, Get Info, Search on Spotify.
- Artist: Instant Mix, Pin to Sidebar, Search on Spotify. Playlist: Play, Shuffle, Pin to Sidebar,
  and on the listener's own: Rename…, Delete Playlist….
- A right click opens the same menu as "…" wherever there is one. An Up Next row's menu starts with
  Remove from Up Next; a song's menu on the listener's own playlist starts with Remove from
  Playlist.
- A right-click menu opens at the pointer and flips up or left where the window ends. "…" opens its
  menu under the button, or over it where there is no room.
- Play Next and Add to Queue with nothing playing start playing.
- Search on Spotify opens Spotify's search for the item in the browser.

## Instant Mix

From the song, album and artist menus, the player's "…" and the search palette. From the playing
song, that song plays on and the mix replaces Up Next; from anything else, the mix replaces the
queue. The mix is the artist's (`getSimilarSongs2`, 100 songs), arranged so an artist plays twice in
a row only when nothing else is left. With nothing similar on the server, the player bar says so and
the queue stays.

## Get info

A card over the dimmed window from the song and album menus, the player's "…", the search's
actions and Ctrl+I. Esc or a click outside closes it. It asks the server each time, so play counts are fresh. Song:
album, track, year, genres, length, format, bit rate, size, plays, last played, date added, comment.
Album: released, genres, songs, length, format, size, plays, last played, date added, label. Empty
rows are left out, except plays: "None" says it was never played. Every genre is listed here; the
album's header has room for the first only. Nothing in the card is a link: it is for reading and
copying.

## Search

One palette for everything: Ctrl+K, `/` or the sidebar's Search. Up to 4 artists, 5 albums and 8
songs from the server once typing pauses for 150 ms; playlists and actions match at once. Actions:
Shuffle Library, Instant Mix, Show Lyrics, Show Queue, Open Full-Screen Player, Get Info, Change
Theme, Settings. Enter on an artist, album or playlist opens it; on a song, plays its album from it.

## Up Next

Cover, title, artist and duration per row; × on hover removes a song, a click jumps to it. "Playing
from <album / playlist / mix>" and Clear at the top. Shuffle puts the playing song first and the rest
in random order.

## Lyrics

Synced lyrics scroll with the song: the current line bright, earlier ones dim, a click seeks.
Instrumental gaps show "• • •". In the side panel and the full-screen player.

## Full-screen player

- Large blurred blobs in the cover's colors drift behind everything, darkened just enough for white
  text to read (WCAG 4.5:1 on the brightest of them): a yellow cover more than a navy one. Close
  with X or Esc.
- Cover and controls on the left; lyrics or Up Next on the right, chosen with two buttons after the
  volume slider and remembered. With neither, everything is centered.
- Title over "Artist — Album" with links, star and "…", the progress bar with times, the controls and
  volume. "Mixing" between the times during a crossfade.

## Sign-in and servers

Legato pings the active server at every start:

- **No server yet, or the sign-in is refused**: the sign-in card, with the reason above the fields.
- **Server not reachable**: the app opens and the content says "Can't reach <server>" with Try
  Again, and offers a saved server that answers. Legato retries every 15 seconds and when the
  window comes to the front.

The sign-in card: Server address, Username, Password, "Keep me signed in" (on) and Sign In. An
address without a scheme gets `https://`; plain `http://` works without a warning, as home servers
often have no certificate.

- "Keep me signed in" stores the password in the system keychain. Subsonic's token is computed from
  the password for every request, so Legato keeps the password itself. Off, it stays in memory.
- Without a keychain service the checkbox is disabled and says why. Legato never writes a password
  to a file. A keychain that refuses an entry shows its reason in Settings under that server.

## Settings

A page (sidebar or Ctrl+,). Rows in groups with the label on the left and a dropdown or switch on
the right.

- Appearance: Theme (System / Light / Dark); Colors from covers; Transparency (on).
- Sidebar: a switch per Library row and for Playlists.
- Playback: Scrobbling; Crossfade (Off / Automatic / Fixed; Automatic by default, as Apple Music's
  AutoMix), with Crossfade length for Fixed and Keep albums gapless (on, so an album plays as it
  was made); Find missing lyrics on LRCLIB (off).
- Servers: status dot, Switch, Rename, Sign Out, Remove, Add Server.

## Keyboard

| Keys | Action |
|---|---|
| Space | Play or pause |
| Ctrl+→ / Ctrl+← | Next song / previous song (restarts the song if more than 3 s in) |
| Shift+→ / Shift+← | Forward / back 10 seconds |
| Ctrl+↑ / Ctrl+↓ | Volume up / down |
| Ctrl+S | Shuffle on or off |
| Ctrl+R | Repeat: off, all, this song |
| Ctrl+Shift+S | Shuffle the whole library |
| ↑ / ↓, Enter | Move in a list, play the selected song |
| Shift+F10 | Menu for the selected song |
| Ctrl+K or / | Search and actions |
| Ctrl+F | Find in the current album, playlist or favorites; elsewhere search |
| Ctrl+I | Get info for the chosen song, else the playing one |
| Ctrl+L / Ctrl+J | Lyrics / queue |
| Alt+← / Alt+→ | Back / forward |
| Ctrl+1 … 5 | Home, Albums, Artists, Songs, Playlists |
| Ctrl+, | Settings |
| F9 | Show or hide the sidebar |
| Arrows, Home, End, Page Up/Down | Choose a cover in a grid; Enter plays it |
| Esc | Close the full-screen player, a menu or a dialog |

## Visual tokens

- Light: background `#fafafa`, raised `#ffffff`, text `#1d1d1d`, secondary `#6e6e6e`, tertiary
  `#a3a3a3`.
- Dark: background `#101010`, raised `#202020`, text `#f5f5f5`, secondary `#a1a1a1`, tertiary
  `#6b6b6b`.
- Lines and fills are translucent neutrals, so buttons on an album's color take that color faintly.
  They stay solid, not glass: blurring a flat color changes nothing, and contrast comes from the
  colors under them, which Legato keeps dark (or pale) enough. The accent is the text color.
- Font: the system UI font. Body 14 px, page titles 34–36 px, section titles 21 px.
- Radii: covers 8 px, cards 14 px, player bar 22 px. Motion: short eases (140–260 ms), all of it
  off with reduced motion.

## Wording

Interface text names things the way listeners know them: "Crossfade", "Up Next", "Get info". Errors
say what went wrong and what to do.

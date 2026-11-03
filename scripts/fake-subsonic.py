#!/usr/bin/env python3
"""A fake Subsonic server for running legato without a real one.

Serves 342 generated albums by ten artists with gradient covers and a 64-second test tone for every
song (four shapes of intro and ending, half of them with drums, for Automatic crossfade), under user
"test", password "test", with token authentication checked as a real server does.
The play queue legato saves is
kept in memory and printed. Odd-numbered songs have synced lyrics, even ones none; the first song of each album has a long
title. Every seventh album is a single or an EP, every eleventh has two discs. Three playlists, a
few favorites and play counts make every page show something.

    scripts/fake-subsonic.py [--port 4533] [--latency-ms 40]
    LEGATO_TEST_SERVER_URL=http://127.0.0.1:4533 LEGATO_TEST_USERNAME=test LEGATO_TEST_PASSWORD=test legato

Standard library only.
"""

import argparse
import array
import colorsys
import functools
import hashlib
import io
import json
import math
import random
import re
import struct
import time
import wave
import zlib
from functools import lru_cache
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

USER, PASSWORD = "test", "test"
# savePlayQueue's last call, served by getPlayQueue. One user, kept in memory.
PLAY_QUEUE = {}
ALBUM_COUNT = 342
SONGS_PER_ALBUM = 6
WORDS = ["Blue", "Night", "Glass", "River", "Silver", "Echo", "Northern", "Paper", "Velvet", "Quiet",
         "Golden", "Static", "Summer", "Distant", "Electric", "Hollow", "Lunar", "Open", "Coral", "Amber"]
ARTISTS = ["June Marlow", "Low Tide", "The Field Recordings", "Mara Lind", "Kōji Sato",
           "Ana Ferreira", "Night Office", "Haze Theory", "Elin Berg", "Paper Harbor"]


def artist_id(name):
    return f"ar{ARTISTS.index(name)}"


def album(i):
    name = f"{WORDS[i % len(WORDS)]} {WORDS[(i * 7 + 3) % len(WORDS)]}"
    if i % 5 == 0:
        name += " (Deluxe Edition, Remastered)"
    artist = ARTISTS[i % len(ARTISTS)]
    kind = ["single", "ep"][i % 2] if i % 7 == 0 else "album"
    return {"id": f"a{i}", "name": name, "artist": artist, "artistId": artist_id(artist),
            "artists": [{"id": artist_id(artist), "name": artist}],
            "coverArt": f"al-a{i}", "year": 1965 + i % 60, "genre": ["Jazz", "Folk", "Electronic"][i % 3],
            # Every fifth album is tagged with many genres, as real libraries often are.
            "genres": [{"name": g} for g in [["Jazz", "Folk", "Electronic"][i % 3]]
                       + (["Ambient", "Soundtrack", "Downtempo", "Chillout", "Experimental"] if i % 5 == 0 else [])],
            "songCount": 2 if kind == "single" else SONGS_PER_ALBUM, "duration": 30 * SONGS_PER_ALBUM,
            "created": f"2026-{1 + i % 12:02d}-{1 + i % 28:02d}T12:00:00Z", "releaseTypes": [kind],
            "playCount": (i * 37) % 23 if i % 4 == 0 else 0, "recordLabels": [{"name": "Fake Records"}],
            "releaseDate": {"year": 1965 + i % 60, "month": 1 + i % 12, "day": 1 + i % 28}}


ALBUMS = sorted((album(i) for i in range(ALBUM_COUNT)), key=lambda a: a["name"].lower())
BY_ID = {a["id"]: a for a in ALBUMS}
STARRED = {"a3-s1", "a10-s2", "a17-s3", "a3", "a24", "ar1"}
PLAYLISTS = [
    {"id": "p1", "name": "Late Night", "comment": "Quiet songs for after midnight", "owner": USER,
     "changed": "2026-10-28T21:15:00Z", "songs": [f"a{i}-s{1 + i % 6}" for i in range(0, 120, 5)]},
    {"id": "p2", "name": "Road Trip", "comment": "", "owner": USER,
     "changed": "2026-09-02T08:00:00Z", "songs": [f"a{i}-s2" for i in range(3, 300, 13)]},
    {"id": "p3", "name": "Empty For Now", "comment": "", "owner": USER, "changed": "2026-11-01T10:00:00Z",
     "songs": []},
    # Another user's public playlist: listed, read only, as Navidrome sends it.
    {"id": "p4", "name": "Shared by Sam", "comment": "", "owner": "sam", "readonly": True,
     "changed": "2026-10-20T10:00:00Z", "songs": [f"a{i}-s3" for i in range(5, 200, 31)]},
]


def now():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def starred(item, key):
    if key in STARRED:
        item["starred"] = "2026-01-01T00:00:00Z"
    return item


def songs(a):
    out = []
    count = a["songCount"]
    for n in range(1, count + 1):
        title = f"Song {n} of {a['name']}"
        if n == 1:
            title += " (Live at the Paradiso, Amsterdam, Late Show, 1974)"
        two_discs = int(a["id"][1:]) % 11 == 0
        song = {"id": f"{a['id']}-s{n}", "title": title, "artist": a["artist"], "artistId": a["artistId"],
                "artists": a["artists"], "album": a["name"], "albumId": a["id"], "coverArt": a["coverArt"],
                "duration": 30, "suffix": "flac", "bitRate": 900, "bitDepth": 16 + 8 * (n % 2),
                "samplingRate": 44100, "track": n if not two_discs or n <= count // 2 else n - count // 2,
                "discNumber": 2 if two_discs and n > count // 2 else 1, "year": a["year"],
                "size": 5_300_000, "playCount": a["playCount"] * (n % 3), "genre": a["genre"],
                "genres": [{"name": a["genre"]}, {"name": "Ambient"}], "created": a["created"]}
        if song["playCount"]:
            song["played"] = "2026-10-30T19:45:00Z"
        out.append(starred(song, song["id"]))
    return out


def song_by_id(song_id):
    album_id, n = song_id.rsplit("-s", 1)
    a = BY_ID.get(album_id)
    return songs(a)[int(n) - 1] if a and 0 < int(n) <= a["songCount"] else None


def artists():
    out = []
    for name in ARTISTS:
        count = sum(1 for a in ALBUMS if a["artist"] == name)
        out.append(starred({"id": artist_id(name), "name": name, "coverArt": f"ar-{artist_id(name)}",
                            "albumCount": count}, artist_id(name)))
    return sorted(out, key=lambda a: a["name"].lower())


def playlist(p, entries=False):
    items = [s for s in (song_by_id(i) for i in p["songs"]) if s]
    out = {k: v for k, v in p.items() if k != "songs"}
    out.update(songCount=len(items), duration=sum(s["duration"] for s in items), coverArt=f"pl-{p['id']}" if items else "")
    if entries:
        out["entry"] = items
    return out


def album_list(kind):
    if kind == "newest":
        return sorted(ALBUMS, key=lambda a: a["created"], reverse=True)
    if kind == "alphabeticalByArtist":
        return sorted(ALBUMS, key=lambda a: (a["artist"].lower(), a["name"].lower()))
    if kind == "byYear":
        return sorted(ALBUMS, key=lambda a: a["year"], reverse=True)
    if kind == "frequent":
        return sorted((a for a in ALBUMS if a["playCount"]), key=lambda a: a["playCount"], reverse=True)
    if kind == "random":
        return random.sample(ALBUMS, len(ALBUMS))
    if kind == "starred":
        return [a for a in ALBUMS if a["id"] in STARRED]
    return ALBUMS


def lyrics(song_id):
    if int(song_id.rsplit("s", 1)[-1]) % 2 == 0:
        return {"lyricsList": {}}
    lines = [{"start": 0, "value": ""}]
    lines += [{"start": 2000 + 3000 * k, "value": f"Line {k + 1}, sung a little after {2 + 3 * k} seconds"}
              for k in range(9)]
    return {"lyricsList": {"structuredLyrics": [{"lang": "eng", "synced": True, "offset": 0, "line": lines}]}}


def png(width, height, pixel):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel(x / width, y / height))
    chunk = lambda kind, data: (struct.pack(">I", len(data)) + kind + data
                                + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b""))


@lru_cache(maxsize=4096)
def cover(cover_id, size):
    seed = int(hashlib.md5(cover_id.encode()).hexdigest()[:8], 16)
    h1, h2 = (seed % 360) / 360, ((seed >> 9) % 360) / 360
    c1 = colorsys.hls_to_rgb(h1, 0.55, 0.7)
    c2 = colorsys.hls_to_rgb(h2, 0.35, 0.6)
    cx, cy = ((seed >> 3) % 100) / 100, ((seed >> 5) % 100) / 100

    def pixel(x, y):
        t = min(1.0, math.hypot(x - cx, y - cy) * 1.2)
        return bytes(int(255 * (a * (1 - t) + b * t)) for a, b in zip(c1, c2))

    return png(size, size, pixel)


@functools.cache
def tone(shape=0, drums=False):
    # Longer than 30 s, so a play can count as a listen (scrobble). Four shapes of ending and intro
    # for Automatic crossfade, by track number: 0 short ramps (as before), 1 a second of silence, then
    # a hard ending and two seconds of silence, 2 a 14 s fade-out and a second of silence, 3 a quiet
    # five-second intro. Each shape has its own pitch, to hear which song is which. With `drums`, a
    # kick at 120 BPM and a hi-hat between plays over it, inside the same envelope, so two songs with
    # drums meet as two beats that are not matched.
    rate, seconds = 44100, 64
    total = rate * seconds
    pitch = (440, 330, 392, 523)[shape]

    def envelope(n):
        t = n / rate
        edges = min(1.0, n / 2000, (total - n) / 2000)
        if shape == 1:
            return 0.0 if t < 1 or t > seconds - 2 else min(edges, (t - 1) * 20, (seconds - 2 - t) * 20)
        if shape == 2:
            fade = (seconds - 1 - t) / 14
            return 0.0 if fade <= 0 else min(edges, 1.0, 10 ** (3 * (min(fade, 1) - 1)))
        if shape == 3:
            return min(edges, 0.06 + 0.94 * min(1.0, max(0.0, t - 4.5) * 2) if t < 5 else 1.0)
        return edges

    noise = random.Random(shape)

    def beat(n):
        t = n / rate
        on, off = t % 0.5, (t + 0.25) % 0.5
        kick = math.exp(-on * 25) * math.sin(2 * math.pi * 60 * on)
        hat = math.exp(-off * 80) * (noise.random() * 2 - 1) * 0.5
        return 1.4 * kick + hat

    samples = array.array("h", (int(6000 * envelope(n) * (math.sin(2 * math.pi * pitch * n / rate)
                                                           + (beat(n) if drums else 0)))
                                for n in range(total)))
    frames = samples.tobytes()
    out = io.BytesIO()
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(frames)
    return out.getvalue()


class Handler(BaseHTTPRequestHandler):
    latency = 0.0

    def log_message(self, *args):
        pass

    def send(self, status, body, content_type, ranges=False):
        # Byte ranges, as Navidrome serves files: mpv seeks in a stream by asking for a range.
        first = None
        if ranges:
            match = re.fullmatch(r"bytes=(\d+)-(\d*)", self.headers.get("Range", ""))
            if match and int(match[1]) < len(body):
                first, last = int(match[1]), min(int(match[2] or len(body) - 1), len(body) - 1)
                size = len(body)
                body = body[first:last + 1]
                status = 206
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        if ranges:
            self.send_header("Accept-Ranges", "bytes")
        if first is not None:
            self.send_header("Content-Range", f"bytes {first}-{first + len(body) - 1}/{size}")
        self.end_headers()
        self.wfile.write(body)

    def api(self, body=None, error=None):
        response = {"status": "failed" if error else "ok", "version": "1.16.1", "openSubsonic": True}
        if error:
            response["error"] = {"code": error[0], "message": error[1]}
        response.update(body or {})
        self.send(200, json.dumps({"subsonic-response": response}).encode(), "application/json")

    def do_POST(self):
        # OpenSubsonic formPost: the parameters arrive as a form body.
        length = int(self.headers.get("Content-Length", 0))
        self.do_GET(self.rfile.read(length).decode())

    def do_GET(self, form=""):
        url = urlparse(self.path)
        query = "&".join(part for part in (url.query, form) if part)
        all_q = parse_qs(query)
        q = {k: v[0] for k, v in all_q.items()}
        token = hashlib.md5((PASSWORD + q.get("s", "")).encode()).hexdigest()
        if q.get("u") != USER or q.get("t") != token:
            return self.api(error=(40, "Wrong username or password"))
        endpoint = url.path.rsplit("/", 1)[-1].removesuffix(".view")
        if endpoint == "ping":
            return self.api()
        if endpoint == "getAlbumList2":
            size, offset = min(int(q.get("size", 10)), 500), int(q.get("offset", 0))
            albums = [starred(dict(a), a["id"]) for a in album_list(q.get("type"))[offset:offset + size]]
            return self.api({"albumList2": {"album": albums}})
        if endpoint == "getAlbum":
            a = BY_ID.get(q.get("id"))
            if not a:
                return self.api(error=(70, "Album not found"))
            return self.api({"album": starred(dict(a, song=songs(a)), a["id"])})
        if endpoint == "getSong":
            found = song_by_id(q.get("id", "")) if "-s" in q.get("id", "") else None
            if not found:
                return self.api(error=(70, "Song not found"))
            return self.api({"song": found})
        if endpoint == "getRandomSongs":
            size = min(int(q.get("size", 10)), 500)
            picks = [s for a in ALBUMS[::7] for s in songs(a)][:size]
            return self.api({"randomSongs": {"song": picks}})
        if endpoint == "getArtists":
            return self.api({"artists": {"ignoredArticles": "The", "index": [{"name": "#", "artist": artists()}]}})
        if endpoint == "getArtist":
            found = next((a for a in artists() if a["id"] == q.get("id")), None)
            if not found:
                return self.api(error=(70, "Artist not found"))
            albums = [starred(dict(a), a["id"]) for a in ALBUMS if a["artistId"] == found["id"]]
            return self.api({"artist": dict(found, album=albums)})
        if endpoint == "getTopSongs":
            picks = [s for a in ALBUMS if a["artist"] == q.get("artist") for s in songs(a)][:int(q.get("count", 50))]
            return self.api({"topSongs": {"song": picks}})
        if endpoint == "getSimilarSongs2":
            # The artist and the two after it in ARTISTS stand in for similar artists.
            found = next((i for i, name in enumerate(ARTISTS) if artist_id(name) == q.get("id")), None)
            if found is None:
                return self.api(error=(70, "Artist not found"))
            similar = {ARTISTS[(found + k) % len(ARTISTS)] for k in range(3)}
            picks = [s for a in ALBUMS if a["artist"] in similar for s in songs(a)][:int(q.get("count", 50))]
            return self.api({"similarSongs2": {"song": picks}})
        if endpoint == "getStarred2":
            return self.api({"starred2": {
                "song": [s for s in (song_by_id(i) for i in sorted(STARRED) if "-s" in i) if s],
                "album": [starred(dict(a), a["id"]) for a in ALBUMS if a["id"] in STARRED],
                "artist": [a for a in artists() if a["id"] in STARRED]}})
        if endpoint == "getPlaylists":
            return self.api({"playlists": {"playlist": [playlist(p) for p in PLAYLISTS]}})
        if endpoint == "getPlaylist":
            p = next((p for p in PLAYLISTS if p["id"] == q.get("id")), None)
            if not p:
                return self.api(error=(70, "Playlist not found"))
            return self.api({"playlist": playlist(p, entries=True)})
        if endpoint == "createPlaylist":
            new = {"id": f"p{max(int(p['id'][1:]) for p in PLAYLISTS) + 1}", "name": q.get("name", ""),
                   "comment": "", "owner": USER, "changed": now(), "songs": all_q.get("songId", [])}
            PLAYLISTS.append(new)
            return self.api({"playlist": playlist(new, entries=True)})
        if endpoint in ("updatePlaylist", "deletePlaylist"):
            p = next((p for p in PLAYLISTS if p["id"] == q.get("playlistId", q.get("id"))), None)
            if not p:
                return self.api(error=(70, "Playlist not found"))
            if p["owner"] != USER:
                return self.api(error=(50, "You are not allowed to change this playlist"))
            if endpoint == "deletePlaylist":
                PLAYLISTS.remove(p)
                return self.api()
            if "name" in q:
                p["name"] = q["name"]
            drop = {int(i) for i in all_q.get("songIndexToRemove", [])}
            p["songs"] = [s for i, s in enumerate(p["songs"]) if i not in drop] + all_q.get("songIdToAdd", [])
            p["changed"] = now()
            return self.api()
        if endpoint == "search3":
            size, offset = min(int(q.get("songCount", 20)), 500), int(q.get("songOffset", 0))
            query = q.get("query", "").strip('"').lower()
            every = [s for a in ALBUMS for s in songs(a) if query in s["title"].lower()]
            result = {"song": every[offset:offset + size]}
            # The Songs page lists every song with an empty query and asks for no artists or albums.
            artist_count, album_count = int(q.get("artistCount", 20)), int(q.get("albumCount", 20))
            if query and artist_count:
                result["artist"] = [a for a in artists() if query in a["name"].lower()][:artist_count]
            if query and album_count:
                result["album"] = [a for a in ALBUMS if query in a["name"].lower()][:album_count]
            return self.api({"searchResult3": result})
        if endpoint == "getScanStatus":
            return self.api({"scanStatus": {"scanning": False, "count": sum(a["songCount"] for a in ALBUMS)}})
        if endpoint == "getLyricsBySongId":
            return self.api(lyrics(q.get("id", "s0")))
        if endpoint in ("star", "unstar"):
            for key in ("id", "albumId", "artistId"):
                if key in q:
                    (STARRED.add if endpoint == "star" else STARRED.discard)(q[key])
            return self.api()
        if endpoint == "scrobble":
            # Printed, so a test run can check what legato reported.
            print("scrobble", q.get("submission"), all_q.get("id", []), all_q.get("time", []), flush=True)
            # A listen counts, as on Navidrome: Most played and Recently played follow it.
            if q.get("submission", "true") != "false":
                for song_id in all_q.get("id", []):
                    a = BY_ID.get(song_id.rsplit("-s", 1)[0])
                    if a:
                        a["playCount"] += 1
                        a["played"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
            return self.api()
        if endpoint == "getOpenSubsonicExtensions":
            return self.api({"openSubsonicExtensions": [{"name": "formPost", "versions": [1]},
                                                        {"name": "songLyrics", "versions": [1]}]})
        if endpoint == "savePlayQueue":
            ids = all_q.get("id", [])
            print("savePlayQueue", self.command, len(ids), q.get("current"), q.get("position"), flush=True)
            PLAY_QUEUE.clear()
            if ids:
                PLAY_QUEUE.update(ids=ids, current=q.get("current"), position=int(q.get("position", 0)),
                                  changedBy=q.get("c", ""), changed=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
            return self.api()
        if endpoint == "getPlayQueue":
            if not PLAY_QUEUE:
                return self.api({"playQueue": {}})
            entries = [s for s in (song_by_id(i) for i in PLAY_QUEUE["ids"]) if s]
            return self.api({"playQueue": dict({k: v for k, v in PLAY_QUEUE.items() if k != "ids"},
                                               username=USER, entry=entries)})
        if endpoint == "getCoverArt":
            time.sleep(self.latency)
            size = max(16, min(int(q.get("size", 300)), 1200))
            return self.send(200, cover(q.get("id", ""), size), "image/png")
        if endpoint == "stream":
            n = int(q.get("id", "-s0").rsplit("-s", 1)[-1] or 0)
            return self.send(200, tone(n % 4, n // 4 % 2 == 1), "audio/wav", ranges=True)
        return self.api(error=(0, f"Unknown endpoint {endpoint}"))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=4533)
    parser.add_argument("--latency-ms", type=int, default=40, help="delay before each cover")
    args = parser.parse_args()
    Handler.latency = args.latency_ms / 1000
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"fake Subsonic server on http://127.0.0.1:{args.port} (user {USER}, password {PASSWORD})")
    server.serve_forever()


if __name__ == "__main__":
    main()

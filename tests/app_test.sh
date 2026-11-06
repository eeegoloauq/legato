#!/usr/bin/env bash
# The app itself against scripts/fake-subsonic.py: starts on the album grid, plays the first album
# to silent output, opens the side panel's queue, saves a screenshot and quits. Under the sanitize
# preset this is the run that covers the library, the pages, the player and the covers; a report
# from a sanitizer fails it with the sanitizer's exit code.
#
#   tests/app_test.sh <legato binary> <fake-subsonic.py>
set -euo pipefail

app=$1
server=$2
work=$(mktemp -d)
cleanup() {
    [ -n "${server_pid:-}" ] && kill "$server_pid" 2>/dev/null || true
    rm -rf "$work"
}
trap cleanup EXIT

port=$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
python3 "$server" --port "$port" --latency-ms 0 >"$work/server.log" 2>&1 &
server_pid=$!
for _ in $(seq 50); do
    python3 -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:$port/rest/ping')" 2>/dev/null && break
    sleep 0.1
done

# A session bus of its own for MPRIS and the keychain's question, as the mpris test has.
bus=
command -v dbus-run-session >/dev/null && bus="dbus-run-session --"

# A home, settings and caches of its own; no proxy between it and the server.
env -u http_proxy -u https_proxy -u HTTP_PROXY -u HTTPS_PROXY -u all_proxy -u ALL_PROXY \
    HOME="$work" XDG_CONFIG_HOME="$work/config" XDG_CACHE_HOME="$work/cache" \
    XDG_DATA_HOME="$work/data" XDG_STATE_HOME="$work/state" \
    LEGATO_TEST_SERVER_URL="http://127.0.0.1:$port" LEGATO_TEST_USERNAME=test LEGATO_TEST_PASSWORD=test \
    QT_QPA_PLATFORM=xcb \
    $bus xvfb-run -a -s '-screen 0 1600x1000x24' \
    "$app" --open albums --play 0 --panel queue --screenshot "$work/window.png" --delay 8000

[ -s "$work/window.png" ] || { echo "app_test: no screenshot" >&2; exit 1; }

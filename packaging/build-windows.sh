#!/usr/bin/env bash
# The Windows release: the app's bundle with libmpv and QtKeychain beside it, and the installer that
# carries them (legato.iss). Runs in Git Bash with the Qt of scripts/install-qt.sh, its MinGW and
# Ninja on PATH, CMAKE_PREFIX_PATH naming that Qt, and ISCC (Inno Setup 6). The same commands by
# hand and in release.yml.
set -euo pipefail

cd "$(dirname "$0")/.."
version=${1:-$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo 0.1.0)}
bundle=dist/bundle
native() { if command -v cygpath >/dev/null; then cygpath -m "$1"; else echo "$1"; fi; }

rm -rf dist build/windows
mkdir -p dist

echo "==> libmpv"
packaging/fetch-libmpv.sh dist/mpv
echo "==> qtkeychain"
packaging/build-qtkeychain.sh dist/qtkeychain

echo "==> app"
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DLEGATO_VERSION="$version" -DMPV_DIR="$(native "$PWD/dist/mpv")" \
  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$(native "$PWD/dist/qtkeychain")"
cmake --build build/windows
cmake --install build/windows --prefix "$bundle"

# What the licences of the libraries the bundle carries ask to go with them. libmpv is GPLv2+: its
# notice, the licence texts and where its source is.
echo "==> licences"
mkdir -p "$bundle/licenses"
cp -r dist/mpv/licenses "$bundle/licenses/mpv"
cp -r dist/qtkeychain/licenses "$bundle/licenses/qtkeychain"
cp LICENSE "$bundle/licenses/LICENSE"

echo "==> installer"
# Git Bash would take /DVersion for a path and rewrite it.
MSYS_NO_PATHCONV=1 iscc "/DVersion=$version" "/DFileVersion=${version%%-*}" packaging/legato.iss
echo "==> dist/legato-$version-windows-x86_64-setup.exe"

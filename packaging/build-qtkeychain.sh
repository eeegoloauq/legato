#!/usr/bin/env bash
# QtKeychain for Windows, which has no package of it: the pinned release, built with the Qt on
# CMAKE_PREFIX_PATH and installed into $1 (Linux uses the distribution's). It stores passwords in
# the Windows Credential Manager.
#
#     packaging/build-qtkeychain.sh dist/qtkeychain
set -euo pipefail

version=0.17.0
sha256=3b85c3929034b0a99da777130c34d99f006fcd3a9d56564159399a33fee0e504

# CMake on Windows takes C:/ paths, not Git Bash's /c/ ones.
native() { if command -v cygpath >/dev/null; then cygpath -m "$1"; else echo "$1"; fi; }

dir=$1
mkdir -p "$dir"
work=$(mktemp -d)
curl -fsSL -o "$work/qtkeychain.tar.gz" \
  "https://github.com/frankosterfeld/qtkeychain/archive/refs/tags/$version.tar.gz"
echo "$sha256  $work/qtkeychain.tar.gz" | sha256sum -c
tar xzf "$work/qtkeychain.tar.gz" -C "$work"
cmake -S "$(native "$work/qtkeychain-$version")" -B "$(native "$work/build")" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TRANSLATIONS=OFF -DBUILD_TESTING=OFF \
  -DCMAKE_INSTALL_PREFIX="$(native "$dir")"
cmake --build "$(native "$work/build")"
cmake --install "$(native "$work/build")"
install -Dm644 "$work/qtkeychain-$version/COPYING" "$dir/licenses/COPYING"
rm -rf "$work"

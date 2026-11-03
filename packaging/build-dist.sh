#!/usr/bin/env bash
# Builds what a Linux release is made of: the app's bundle (the binary with the Qt it was built and
# tested with) and the tarball that carries it with the desktop files and icons. The RPM and the Arch
# package start from this tarball, so a package built by hand and one built by CI come from the same
# commands. Qt comes from CMAKE_PREFIX_PATH (scripts/install-qt.sh prints it).
set -euo pipefail

cd "$(dirname "$0")/.."
version=${1:-$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo 0.1.0)}
name="legato-$version-linux-x86_64"
stage="dist/$name"
id=io.github.eeegoloauq.legato

rm -rf dist build/release
mkdir -p "$stage"

echo "==> app"
cmake --preset release -DLEGATO_VERSION="$version"
cmake --build --preset release
cmake --install build/release --prefix "$stage/bundle"

# Every size the desktops ask for. 16 to 32 come from icon-small.svg, drawn heavier for them, so no
# desktop has to shrink a larger one.
echo "==> icons"
for size in 16 24 32 48 64 128 256 512; do
  src=packaging/icon.svg
  [ $size -le 32 ] && src=packaging/icon-small.svg
  mkdir -p "$stage/icons/hicolor/${size}x${size}/apps"
  rsvg-convert -w $size -h $size $src -o "$stage/icons/hicolor/${size}x${size}/apps/$id.png"
done
install -Dm644 packaging/icon.svg "$stage/icons/hicolor/scalable/apps/$id.svg"
install -Dm644 packaging/icon-symbolic.svg "$stage/icons/hicolor/symbolic/apps/$id-symbolic.svg"

cp packaging/legato.sh packaging/$id.desktop packaging/$id.metainfo.xml LICENSE README.md "$stage/"

tar czf "dist/$name.tar.gz" -C dist "$name"
echo "==> dist/$name.tar.gz"

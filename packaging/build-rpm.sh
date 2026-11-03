#!/usr/bin/env bash
# Packages the tarball from build-dist.sh as an RPM. Build it on Fedora (the release workflow uses
# Fedora's container): the spec validates with tools that only exist under those names there.
set -euo pipefail

cd "$(dirname "$0")/.."
version=${1:-$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo 0.1.0)}
tarball="dist/legato-$version-linux-x86_64.tar.gz"

[ -f "$tarball" ] || packaging/build-dist.sh "$version"

elsewhere=()
if ! grep -qi 'fedora\|rhel\|centos' /etc/os-release; then
  # The package comes out the same; what is skipped is the validation, which CI does.
  echo "not a Fedora machine: skipping the BuildRequires check and %check" >&2
  elsewhere=(--nodeps --nocheck)
fi

topdir=$PWD/dist/rpmbuild
mkdir -p "$topdir"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
cp "$tarball" "$topdir/SOURCES/"
{ cat packaging/legato.spec; packaging/release-notes.py rpm; } > "$topdir/SPECS/legato.spec"
# A beta's dash becomes ~, which rpm sorts before the release itself.
rpmbuild "${elsewhere[@]}" --define "_topdir $topdir" \
         --define "version ${version/-/\~}" --define "tarver $version" \
         -bb "$topdir/SPECS/legato.spec"
find "$topdir/RPMS" -name '*.rpm'

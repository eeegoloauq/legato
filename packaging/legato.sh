#!/usr/bin/bash
# What /usr/bin/legato is: finds the bundle, /usr/lib64/legato from the RPM, /usr/lib/legato from
# the Arch package, or bundle/ beside this script in the tarball.
here=$(dirname "$(readlink -f "$0")")
for dir in "$here/../lib64/legato" "$here/../lib/legato" "$here/bundle"; do
  if [ -x "$dir/bin/legato" ]; then
    exec "$dir/bin/legato" "$@"
  fi
done
echo "legato: no application bundle found from $here" >&2
exit 1

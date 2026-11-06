# Development

Legato is Qt Quick and C++ with libmpv ([architecture](architecture.md),
[design](design.md), [roadmap](roadmap.md)). It builds with the Qt from
`scripts/install-qt.sh`, CMake 3.22 or newer, Ninja, and the libmpv and QtKeychain development files
(Fedora: `sudo dnf install cmake ninja-build gcc-c++ mpv-libs-devel qtkeychain-qt6-devel`; Debian
and Ubuntu: `sudo apt install cmake ninja-build g++ libmpv-dev qtkeychain-qt6-dev`). The `dev` preset
also uses ccache and mold.

```sh
qt=$(scripts/install-qt.sh)                          # Qt 6.12.0 under ~/.cache/legato/qt
cmake --preset dev -DCMAKE_PREFIX_PATH="$qt"
cmake --build --preset dev                           # build/dev/bin/legato
ctest --test-dir build/dev                           # tests
cmake --build build/dev --target all_qmllint         # QML lint
```

`scripts/fake-subsonic.py` serves a generated library (user and password `test`).
`LEGATO_TEST_SERVER_URL`, `LEGATO_TEST_USERNAME` and `LEGATO_TEST_PASSWORD` use a server without
saving it:

```sh
scripts/fake-subsonic.py &
LEGATO_TEST_SERVER_URL=http://127.0.0.1:4533 LEGATO_TEST_USERNAME=test LEGATO_TEST_PASSWORD=test build/dev/bin/legato
```

`legato --help` lists the development options. Release packages come from the scripts in
`packaging/`; a `v*` tag builds and publishes them ([architecture](architecture.md#packaging-and-releases)).

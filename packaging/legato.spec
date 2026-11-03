# The package is built from the tarball of build-dist.sh, not from source: the bundle carries the Qt
# it was built and tested with, so a Fedora upgrade cannot leave it on a Qt it was never run against.
Name:           legato
Version:        %{?version}%{!?version:0.1.0}
# The tarball's own version: a beta's is X.Y.Z-beta.N where Version has a ~.
%{!?tarver:%global tarver %{version}}
Release:        1%{?dist}
Summary:        Play the music on your Navidrome or OpenSubsonic server

License:        MIT
URL:            https://github.com/eeegoloauq/legato
Source0:        %{name}-%{tarver}-linux-x86_64.tar.gz
ExclusiveArch:  x86_64

BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

# Qt and ICU are in the bundle: rpm neither offers them to other packages nor looks for them in the
# system. Everything else the bundle links, libmpv and QtKeychain among it, rpm's dependency
# generator finds and requires.
%global __provides_exclude_from ^%{_libdir}/%{name}/.*$
%global __requires_exclude ^lib(Qt6|icu).*$

%description
Legato is a simple music player for Navidrome and other OpenSubsonic servers, with smooth mixing
between songs, Instant Mix, synced lyrics and the queue kept on the server.

%global debug_package %{nil}
%{!?_metainfodir: %global _metainfodir %{_datadir}/metainfo}

%prep
%setup -q -n %{name}-%{tarver}-linux-x86_64

%build
# Nothing to build: see the note at the top.

%install
# The bundle stays in one piece: the binary looks for Qt at $ORIGIN/../lib and for its plugins and
# QML beside that.
install -d %{buildroot}%{_libdir}/%{name}
cp -a bundle/. %{buildroot}%{_libdir}/%{name}/
install -Dm755 legato.sh %{buildroot}%{_bindir}/%{name}
install -Dm644 io.github.eeegoloauq.legato.desktop -t %{buildroot}%{_datadir}/applications
install -Dm644 io.github.eeegoloauq.legato.metainfo.xml -t %{buildroot}%{_metainfodir}
install -d %{buildroot}%{_datadir}/icons
cp -a icons/hicolor %{buildroot}%{_datadir}/icons/

%check
# gdk-pixbuf (GNOME Shell's app grid) looks for "<svg" in the first bytes only.
head -c 200 %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/io.github.eeegoloauq.legato.svg | grep -q '<svg'
desktop-file-validate %{buildroot}%{_datadir}/applications/io.github.eeegoloauq.legato.desktop
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/io.github.eeegoloauq.legato.metainfo.xml

%files
%license LICENSE
%{_bindir}/%{name}
%{_libdir}/%{name}/
%{_datadir}/applications/io.github.eeegoloauq.legato.desktop
%{_metainfodir}/io.github.eeegoloauq.legato.metainfo.xml
%{_datadir}/icons/hicolor/*/apps/io.github.eeegoloauq.legato*

# Filled by build-rpm.sh from the AppStream releases (release-notes.py).
%changelog

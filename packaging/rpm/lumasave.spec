Name:           lumasave
Version:        0.1.0~alpha.1
Release:        1%{?dist}
Summary:        Content-adaptive LCD backlight saving for KDE Plasma
License:        MIT
URL:            https://github.com/lkarlslund/lumasave
Source0:        lumasave.tar.gz

BuildRequires:  cmake >= 3.25
BuildRequires:  cargo
BuildRequires:  gcc-c++
BuildRequires:  rust
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(epoxy)
BuildRequires:  pkgconfig(wayland-server)
%if 0%{?fedora}
BuildRequires:  extra-cmake-modules >= 6.0
BuildRequires:  kf6-kcmutils-devel >= 6.0
BuildRequires:  kwin-devel >= 6.6
BuildRequires:  libkscreen-devel >= 6.0
BuildRequires:  qt6-qtbase-devel >= 6.7
BuildRequires:  qt6-qtdeclarative-devel >= 6.7
%global kwin_version %(rpm -q --qf '%%{VERSION}' kwin)
Requires:       kwin%{?_isa} >= %{kwin_version}
Requires:       kwin%{?_isa} < %{kwin_version}.1
Requires:       plasma-workspace
%else
BuildRequires:  kf6-extra-cmake-modules >= 6.0
BuildRequires:  kf6-kcmutils-devel >= 6.0
BuildRequires:  kf6-kwindowsystem-devel >= 6.0
BuildRequires:  kwin6-devel >= 6.6
BuildRequires:  libkscreen6-devel >= 6.0
BuildRequires:  qt6-base-devel >= 6.7
BuildRequires:  qt6-declarative-devel >= 6.7
%global kwin_version %(rpm -q --qf '%%{VERSION}' kwin6)
Requires:       kwin6 >= %{kwin_version}
Requires:       kwin6 < %{kwin_version}.1
Requires:       plasma6-workspace
%endif
Requires:       kdialog

%description
LumaSave lowers an internal LCD backlight and compensates the image in KWin to
preserve useful contrast and readability.

%prep
%autosetup -n lumasave

%build
cargo fetch --locked
cmake -S kwin -B build \
    -DCMAKE_BUILD_TYPE=None \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_INSTALL_LIBDIR=%{_lib} \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
    -DKDE_INSTALL_PLUGINDIR=%{_lib}/qt6/plugins \
    -Wno-dev
CARGO_NET_OFFLINE=true cmake --build build --parallel %{?_smp_build_ncpus}

%install
DESTDIR=%{buildroot} cmake --install build

%files
%license LICENSE
%{_bindir}/lumasave-calibrate
%{_bindir}/lumasave-calibrate-preview
%{_bindir}/lumasave-check
%{_libdir}/qt6/plugins/kwin/effects/plugins/lumasave.so
%{_libdir}/qt6/plugins/plasma/kcms/systemsettings/kcm_lumasave.so
%{_libdir}/qt6/qml/org/kde/lumasave/
%{_datadir}/applications/kcm_lumasave.desktop
%{_datadir}/plasma/plasmoids/com.github.lkarlslund.lumasave/

%changelog
* Fri Aug 14 2026 Lars Karlslund <lkarlslund@users.noreply.github.com> - 0.1.0~alpha.1-1
- Initial package

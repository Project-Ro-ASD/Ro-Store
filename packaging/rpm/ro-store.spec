%global debug_package %{nil}

Name:           ro-store
Version:        1.0.0
Release:        %{?ro_release}%{!?ro_release:1}%{?dist}
Summary:        Ro-ASD application store
License:        GPL-3.0-or-later
URL:            https://github.com/Project-Ro-ASD/Ro-Store
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  appstream-qt-devel
BuildRequires:  desktop-file-utils
BuildRequires:  appstream
Requires:       dnf5
Requires:       dnf5-command(download)
Requires:       dnf5daemon-server
Requires:       dnf5daemon-server-polkit
Requires:       polkit
Requires:       kf6-kirigami

%description
Ro-Store is the graphical application store for Ro-ASD. It supports RPM and
Flatpak application management and Fedora system updates.

%prep
%autosetup

%build
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DCMAKE_SKIP_RPATH=ON \
  -DBUILD_TESTING=OFF
cmake --build build --parallel %{_smp_build_ncpus}

%install
DESTDIR=%{buildroot} cmake --install build

%check
desktop-file-validate build/org.projectro.RoStore.desktop
appstreamcli validate --no-net build/org.projectro.RoStore.metainfo.xml

%files
%license LICENSE
%doc docs/user-guide-v1.0.md
%{_bindir}/ro-store
%{_libexecdir}/ro-store/ro-store-repo-helper
%{_datadir}/applications/org.projectro.RoStore.desktop
%{_datadir}/metainfo/org.projectro.RoStore.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/ro-store.svg
%{_datadir}/polkit-1/actions/org.projectro.RoStore.repository.policy

%changelog
* Sat Nov 28 2026 Project Ro-ASD - 1.0.0-1
- Prepare the Fedora 44 release candidate.

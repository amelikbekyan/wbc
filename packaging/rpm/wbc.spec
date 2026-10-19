# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The Fedora package. ci/build-rpm.sh builds it in a fedora container, with
# Source0 made from the checkout in the layout of the tag's tarball; the same
# file is meant for COPR later, which fetches both sources itself.

Name:           wbc
Version:        0.1.0
Release:        1%{?dist}
Summary:        Blur or replace the background of your webcam
# The program is GPL; the segmentation model it installs is Apache-2.0.
License:        GPL-3.0-or-later AND Apache-2.0
URL:            https://github.com/amelikbekyan/wbc
Source0:        %{url}/archive/refs/tags/v%{version}/wbc-%{version}.tar.gz
# The model is fetched at configure time from a source tree; a package build
# has no network, so it comes as a source of its own. CMake still checks its
# hash.
Source1:        https://huggingface.co/onnx-community/mediapipe_selfie_segmentation/resolve/main/onnx/model.onnx#/selfie_segmentation.onnx

BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-linguist
BuildRequires:  opencv-devel
BuildRequires:  opencv-ximgproc
BuildRequires:  onnxruntime-devel
BuildRequires:  gtest-devel
BuildRequires:  gmock-devel
# The window tests measure text, and a build root has no font of its own.
BuildRequires:  dejavu-sans-fonts

# The kernel module is in RPM Fusion, not in Fedora, so it is only
# recommended: the package installs without it, and the README says where to
# get it.
Recommends:     akmod-v4l2loopback

%description
Webcam Background Changer takes your real camera, blurs the room behind you
or replaces it with a picture, a video or a solid colour, and offers the
result as a second camera that Zoom, Google Meet, Microsoft Teams, OBS and
any browser can choose.

%prep
%autosetup -n wbc-%{version}
mkdir -p models
cp %{SOURCE1} models/selfie_segmentation.onnx

%build
# WBC_SOURCE_TREE_HELPER off: Fedora builds RelWithDebInfo, for which it
# would default on and compile the build root's path into the program.
%cmake -G Ninja \
    -DWBC_SOURCE_TREE_HELPER=OFF \
    -DWBC_MODULE_INSTALL_COMMAND="sudo dnf install akmod-v4l2loopback  # from RPM Fusion"
%cmake_build

%install
%cmake_install

%check
%ctest -L "unit|ui"

%post
%{_libexecdir}/wbc/wbc-install-hook || :

%postun
# Only on removal: on an upgrade, $1 is the number of versions left, 1.
if [ "$1" -eq 0 ]; then
    rm -f %{_sysconfdir}/modules-load.d/wbc.conf %{_sysconfdir}/modprobe.d/wbc.conf
fi

%files
%license %{_docdir}/wbc/LICENSE
%doc %{_docdir}/wbc/THIRD-PARTY-NOTICES.md
%{_bindir}/wbc
%{_libexecdir}/wbc/
%{_datadir}/wbc/
%{_datadir}/applications/wbc.desktop
%{_datadir}/metainfo/io.github.amelikbekyan.wbc.metainfo.xml
%{_datadir}/icons/hicolor/*/apps/wbc.*
%{_datadir}/polkit-1/actions/org.wbc.setup-loopback.policy

%changelog
* Mon Oct 19 2026 Arthur Melikbekyan <arthur.melikbekyan@protonmail.com> - 0.1.0-1
- First release.

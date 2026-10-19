#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the Arch package from packaging/arch/PKGBUILD.in, as a user would
# with makepkg, running its tests in check(), then installs it and checks it
# with ci/verify-package.sh. The tarball is made from the checkout by
# ci/source-tarball.sh; the package ends up in dist/ only to be verified
# again in a clean container.
#
# Runs as root in an archlinux container; makepkg itself refuses root, so it
# runs as a user made for it.
#
# Usage: ci/build-arch.sh

set -eu

pacman -Syu --noconfirm --needed base-devel git
id builder >/dev/null 2>&1 || useradd -m builder
echo "builder ALL=(ALL) NOPASSWD: ALL" >/etc/sudoers.d/builder

version="$(sed -n 's/^project(wbc VERSION \([0-9.]*\) .*/\1/p' CMakeLists.txt)"
work="$(mktemp -d)"

ci/source-tarball.sh "${version}" "${work}/wbc-${version}.tar.gz"
[ -f models/selfie_segmentation.onnx ] && cp models/selfie_segmentation.onnx "${work}/"

sha256="$(sha256sum "${work}/wbc-${version}.tar.gz" | cut -d' ' -f1)"
sed -e "s/@VERSION@/${version}/" -e "s/@SOURCE_SHA256@/${sha256}/" \
    packaging/arch/PKGBUILD.in >"${work}/PKGBUILD"
cp packaging/arch/wbc.install "${work}/"
chown -R builder "${work}"

# -s installs the dependencies; ONNX Runtime has several providers, and the
# CPU one is what a machine without a particular GPU wants.
pacman -S --noconfirm --needed onnxruntime-cpu
su builder -c "cd '${work}' && makepkg -s --noconfirm" >"${work}/build.log" 2>&1 ||
    { cat "${work}/build.log"; exit 1; }
cat "${work}/build.log"
ci/check-configure-log.sh "${work}/build.log"

mkdir -p dist
cp "${work}"/wbc-"${version}"-*-x86_64.pkg.tar.zst dist/
ci/verify-package.sh dist/wbc-"${version}"-*-x86_64.pkg.tar.zst

#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds, tests and packages the Debian package for one distribution, inside
# a container of that distribution, and puts it in dist/.
#
# Usage: ci/build-deb.sh <distribution>, e.g. ubuntu26.04 or debian13

set -eu

distro="$1"
build="build/package-${distro}"

# A build directory of its own, so that the checkout's own builds, made on
# another system, are left alone.
mkdir -p build
cmake -S . -B "${build}" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr -DWBC_PACKAGE_DISTRO="${distro}" >"${build}.configure.log" 2>&1 ||
    { cat "${build}.configure.log"; exit 1; }
cat "${build}.configure.log"
ci/check-configure-log.sh "${build}.configure.log"
cmake --build "${build}"
ctest --test-dir "${build}" -L "unit|ui" --output-on-failure
(cd "${build}" && cpack -G DEB)
mkdir -p dist
# GitHub turns the tilde of a release file's name into a dot, and then
# SHA256SUMS and the README would name files the release does not have. So
# the file is named as GitHub would show it; the version inside the package
# keeps the tilde.
for deb in "${build}"/wbc_*"~${distro}"_*.deb; do
    cp "${deb}" "dist/$(basename "${deb}" | tr '~' '.')"
done
ls -l dist

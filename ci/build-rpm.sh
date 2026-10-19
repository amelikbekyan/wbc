#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the Fedora package from packaging/rpm/wbc.spec, running its tests
# in %check, and puts it in dist/. Source0 is made from the checkout by
# ci/source-tarball.sh; the model comes from models/ when CMake has fetched
# it already.
#
# Usage: ci/build-rpm.sh

set -eu

version="$(sed -n 's/^Version: *//p' packaging/rpm/wbc.spec)"
top="$(pwd)/build/rpmbuild"
mkdir -p "${top}/SOURCES"

ci/source-tarball.sh "${version}" "${top}/SOURCES/wbc-${version}.tar.gz"

model_url="$(sed -n 's/^Source1: *\(.*\)#\/.*/\1/p' packaging/rpm/wbc.spec)"
model_sha256="$(sed -n 's/^ *"\([0-9a-f]\{64\}\)")$/\1/p' cmake/FetchModel.cmake)"
if [ -f models/selfie_segmentation.onnx ]; then
    cp models/selfie_segmentation.onnx "${top}/SOURCES/"
else
    curl -sSL --retry 3 -o "${top}/SOURCES/selfie_segmentation.onnx" "${model_url}"
fi
echo "${model_sha256}  ${top}/SOURCES/selfie_segmentation.onnx" | sha256sum --check

rpmbuild -bb --define "_topdir ${top}" packaging/rpm/wbc.spec >"${top}/build.log" 2>&1 ||
    { cat "${top}/build.log"; exit 1; }
cat "${top}/build.log"
ci/check-configure-log.sh "${top}/build.log"
mkdir -p dist
cp "${top}"/RPMS/x86_64/wbc-"${version}"-*.x86_64.rpm dist/
ls -l dist

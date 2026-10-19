#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Stops a release whose tag does not match the version the sources carry:
# project() in CMakeLists.txt, which names the Debian packages, and Version
# in the RPM spec. Files named 0.1.0 under a tag v0.1.1 would be worse than
# no release.
#
# Usage: ci/check-tag-version.sh <tag>, e.g. v0.1.0

set -eu

tag_version="${1#v}"
cmake_version="$(sed -n 's/^project(wbc VERSION \([0-9.]*\) .*/\1/p' CMakeLists.txt)"
spec_version="$(sed -n 's/^Version: *//p' packaging/rpm/wbc.spec)"

for version in "${cmake_version}" "${spec_version}"; do
    if [ "${version}" != "${tag_version}" ]; then
        echo "the tag $1 does not match the sources: CMakeLists.txt says ${cmake_version}, wbc.spec ${spec_version}" >&2
        exit 1
    fi
done
echo "the tag $1 matches the sources"

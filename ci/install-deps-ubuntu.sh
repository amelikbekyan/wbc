#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Installs everything needed to build and test on Ubuntu 24.04, the oldest
# release this project promises to build on. CI runs this script, and so can a
# person: it is the same list in both places, so the two cannot disagree.
#
# Ubuntu 24.04 does not package ONNX Runtime, so the official release is
# fetched instead and checked against a pinned hash, as the model is.

set -euo pipefail

ORT_VERSION="1.23.2"
ORT_SHA256="1fa4dcaef22f6f7d5cd81b28c2800414350c10116f5fdd46a2160082551c5f9b"
ORT_PREFIX="${ORT_PREFIX:-/opt/onnxruntime}"

sudo_if_needed() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}

# fonts-dejavu-core: the window tests measure text, and a bare system has no
# font of its own. DejaVu Sans is wider than the fonts desktops ship, so the
# check that nothing is cut off errs on the side of catching it.
sudo_if_needed apt-get update
sudo_if_needed env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build ca-certificates curl \
    qt6-base-dev qt6-tools-dev-tools \
    libopencv-dev libopencv-contrib-dev \
    libgtest-dev libgmock-dev gcovr \
    fonts-dejavu-core

if [ -f "${ORT_PREFIX}/lib/libonnxruntime.so" ]; then
    echo "ONNX Runtime already present in ${ORT_PREFIX}"
    exit 0
fi

# ORT_ARCHIVE names a copy fetched beforehand, for a machine without network
# access at build time. It is held to the same hash as a download.
if [ -n "${ORT_ARCHIVE:-}" ]; then
    archive="${ORT_ARCHIVE}"
else
    archive="$(mktemp --suffix=.tgz)"
    trap 'rm -f "${archive}"' EXIT
    curl -sSL --retry 3 -o "${archive}" \
        "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x64-${ORT_VERSION}.tgz"
fi
echo "${ORT_SHA256}  ${archive}" | sha256sum --check --quiet

sudo_if_needed mkdir -p "${ORT_PREFIX}"
sudo_if_needed tar -xzf "${archive}" -C "${ORT_PREFIX}" --strip-components=1
echo "ONNX Runtime ${ORT_VERSION} installed in ${ORT_PREFIX}"

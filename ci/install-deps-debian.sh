#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Installs what building, testing and packaging need on a Debian or Ubuntu
# release whose archive has ONNX Runtime: Ubuntu 26.04 and Debian 13. The
# release workflow runs it in a container of each; ci/install-deps-ubuntu.sh
# is for Ubuntu 24.04, which has to fetch ONNX Runtime itself.
#
# dpkg-dev and file: cpack's dpkg-shlibdeps, which turns the libraries the
# program links into the package's dependencies.
# fonts-dejavu-core: the window tests measure text; see install-deps-ubuntu.sh.

set -eu

sudo_if_needed() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}

sudo_if_needed apt-get update
sudo_if_needed env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build ca-certificates \
    qt6-base-dev qt6-tools-dev-tools \
    libopencv-dev libopencv-contrib-dev libonnxruntime-dev \
    libgtest-dev libgmock-dev \
    dpkg-dev file fonts-dejavu-core

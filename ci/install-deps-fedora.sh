#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Installs what ci/build-rpm.sh needs on Fedora: rpmbuild, git for the source
# tarball, and the spec's BuildRequires, read from the spec itself so that
# the two lists cannot disagree.

set -eu

dnf install -y rpm-build dnf-plugins-core git-core
dnf builddep -y packaging/rpm/wbc.spec

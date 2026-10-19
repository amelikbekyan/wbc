#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fails when a configure log shows that an optional piece was not found. A
# build from source may go without translations or ximgproc, and only warns;
# a package must not, or every user of that distribution gets an English-only
# program with rougher edges and nobody notices.
#
# Usage: ci/check-configure-log.sh <log>

set -eu

if grep -E "(lrelease|ximgproc) not found" "$1"; then
    echo "a package must be built with translations and ximgproc" >&2
    exit 1
fi

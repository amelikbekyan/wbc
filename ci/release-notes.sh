#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Prints the section of CHANGELOG.md for one version, without its heading:
# the notes of that version's GitHub release. Fails when there is none, so a
# release is never made with empty notes.
#
# Usage: ci/release-notes.sh <version>, e.g. 0.1.0

set -eu

notes="$(awk -v heading="## $1" '
    $0 == heading { inside = 1; next }
    /^## / { inside = 0 }
    inside' CHANGELOG.md)"
if [ -z "$(printf '%s' "${notes}" | tr -d '[:space:]')" ]; then
    echo "CHANGELOG.md has no section for $1" >&2
    exit 1
fi
printf '%s\n' "${notes}"

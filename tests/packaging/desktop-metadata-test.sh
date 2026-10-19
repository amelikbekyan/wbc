#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Validates the desktop entry and the AppStream metadata with the tools that
# software centres and Flathub use. Either tool may be missing on a build
# machine; then its check is skipped, and with neither the test reports
# itself skipped (77) rather than passing with nothing checked.
#
# Usage: desktop-metadata-test.sh <wbc.desktop> <metainfo.xml>

set -u

desktop="$1"
metainfo="$2"
checked=0
failed=0

if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "${desktop}" || failed=1
    checked=1
fi
if command -v appstreamcli >/dev/null 2>&1; then
    # --no-net: the URLs point at the public repository, which a build need
    # not be able to reach. Infos and pedantic notes do not fail validation.
    appstreamcli validate --no-net "${metainfo}" || failed=1
    checked=1
fi

[ "${checked}" -eq 1 ] || exit 77
exit "${failed}"

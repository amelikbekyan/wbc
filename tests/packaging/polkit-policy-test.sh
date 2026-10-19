#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks the configured polkit action: that it is well-formed XML, and that
# its exec.path is the path the helper is installed to. pkexec compares that
# annotation with the program it runs, so a mismatch silently brings back the
# generic "run modprobe as root" prompt rather than failing anywhere visible.
#
# Usage: polkit-policy-test.sh <configured .policy> <installed helper path>

set -u

policy="$1"
helper="$2"

if command -v xmllint >/dev/null 2>&1; then
    xmllint --noout "${policy}" || exit 1
else
    echo "xmllint not found; checking the exec.path alone"
fi

grep -q "<annotate key=\"org.freedesktop.policykit.exec.path\">${helper}</annotate>" "${policy}" || {
    echo "FAIL: ${policy} does not name ${helper} as its exec.path" >&2
    exit 1
}
grep -q '<allow_active>auth_admin_keep</allow_active>' "${policy}" || {
    echo "FAIL: ${policy} does not ask for an administrator" >&2
    exit 1
}
echo "the polkit action names ${helper}"

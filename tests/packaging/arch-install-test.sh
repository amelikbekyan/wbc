#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the functions of packaging/arch/wbc.install, the PKGBUILD's install
# script, against a temporary root, as maintainer-scripts-test.sh does for
# the Debian scripts. /usr/libexec and /etc in it are pointed into the root
# with sed, so neither pacman nor root is needed.
#
# Usage: arch-install-test.sh <wbc.install>

set -u

script="$1"
root="$(mktemp -d)"
trap 'rm -rf "${root}"' EXIT

failures=0
fail() {
    echo "FAIL: $1" >&2
    failures=$((failures + 1))
}

sed -e "s|/usr/libexec/|${root}/usr/libexec/|g" -e "s|/etc/|${root}/etc/|g" "${script}" >"${root}/wbc.install"

# Each call in a shell of its own, as pacman runs them.
call() {
    sh -c ". '${root}/wbc.install' && $*"
}

# A fake hook that logs its call and exits with FAKE_HOOK_EXIT.
hook="${root}/usr/libexec/wbc/wbc-install-hook"
mkdir -p "$(dirname "${hook}")"
printf '#!/bin/sh\necho called >>"$0.log"\nexit "${FAKE_HOOK_EXIT:-0}"\n' >"${hook}"
chmod +x "${hook}"

call post_install 0.1.0-1 || fail "post_install failed"
[ -e "${hook}.log" ] || fail "post_install did not run the install hook"
FAKE_HOOK_EXIT=1 call post_install 0.1.0-1 || fail "post_install failed when the hook did"

# An upgrade sets the camera up again, for a version that changes the
# setting, and keeps the files.
mkdir -p "${root}/etc/modules-load.d" "${root}/etc/modprobe.d"
echo v4l2loopback >"${root}/etc/modules-load.d/wbc.conf"
echo "options v4l2loopback devices=1" >"${root}/etc/modprobe.d/wbc.conf"
echo "options other" >"${root}/etc/modprobe.d/other.conf"
rm -f "${hook}.log"
call post_upgrade 0.1.1-1 0.1.0-1 || fail "post_upgrade failed"
[ -e "${hook}.log" ] || fail "post_upgrade did not run the install hook"
[ -f "${root}/etc/modules-load.d/wbc.conf" ] || fail "an upgrade deleted modules-load.d/wbc.conf"
[ -f "${root}/etc/modprobe.d/wbc.conf" ] || fail "an upgrade deleted modprobe.d/wbc.conf"

mv "${hook}" "${hook}.away"
call post_install 0.1.0-1 2>/dev/null || fail "post_install failed without the hook"

call post_remove 0.1.0-1 || fail "post_remove failed"
[ ! -e "${root}/etc/modules-load.d/wbc.conf" ] || fail "removal left modules-load.d/wbc.conf"
[ ! -e "${root}/etc/modprobe.d/wbc.conf" ] || fail "removal left modprobe.d/wbc.conf"
[ -f "${root}/etc/modprobe.d/other.conf" ] || fail "removal deleted a file that is not ours"
call post_remove 0.1.0-1 || fail "a second removal, with nothing left to delete, failed"

if [ "${failures}" -ne 0 ]; then
    echo "${failures} check(s) failed" >&2
    exit 1
fi
echo "the Arch install script never fails the installation and keeps the setting across upgrades"

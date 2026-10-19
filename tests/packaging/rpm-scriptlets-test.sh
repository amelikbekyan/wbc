#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the %post and %postun scriptlets of packaging/rpm/wbc.spec against a
# temporary root, as maintainer-scripts-test.sh does for the Debian ones. The
# bodies are cut out of the spec with sed and its two directory macros
# pointed into the root, so neither rpm nor root is needed.
#
# Usage: rpm-scriptlets-test.sh <wbc.spec>

set -u

spec="$1"
root="$(mktemp -d)"
trap 'rm -rf "${root}"' EXIT

failures=0
fail() {
    echo "FAIL: $1" >&2
    failures=$((failures + 1))
}

# The lines after "%<name>" up to the next section, with the macros replaced.
scriptlet() {
    sed -n "/^%$1\$/,/^%[a-z]/p" "${spec}" | sed '1d;$d' |
        sed -e "s|%{_libexecdir}|${root}/usr/libexec|g" -e "s|%{_sysconfdir}|${root}/etc|g"
}
scriptlet post >"${root}/post"
scriptlet postun >"${root}/postun"
[ -s "${root}/post" ] || fail "the spec has no %post"
[ -s "${root}/postun" ] || fail "the spec has no %postun"

# A fake hook that logs its call and exits with FAKE_HOOK_EXIT.
hook="${root}/usr/libexec/wbc/wbc-install-hook"
mkdir -p "$(dirname "${hook}")"
printf '#!/bin/sh\necho called >>"$0.log"\nexit "${FAKE_HOOK_EXIT:-0}"\n' >"${hook}"
chmod +x "${hook}"

# %post runs the hook and fails neither when the hook does nor without it.
sh "${root}/post" 1 || fail "%post failed"
[ -e "${hook}.log" ] || fail "%post did not run the install hook"
FAKE_HOOK_EXIT=1 sh "${root}/post" 1 || fail "%post failed when the hook did"
mv "${hook}" "${hook}.away"
sh "${root}/post" 1 2>/dev/null || fail "%post failed without the hook"

# %postun gets the number of versions left: 1 on an upgrade, which keeps the
# boot-time setting, and 0 on removal, which deletes it.
mkdir -p "${root}/etc/modules-load.d" "${root}/etc/modprobe.d"
echo v4l2loopback >"${root}/etc/modules-load.d/wbc.conf"
echo "options v4l2loopback devices=1" >"${root}/etc/modprobe.d/wbc.conf"
echo "options other" >"${root}/etc/modprobe.d/other.conf"
sh "${root}/postun" 1 || fail "%postun 1 failed"
[ -f "${root}/etc/modules-load.d/wbc.conf" ] || fail "an upgrade deleted modules-load.d/wbc.conf"
[ -f "${root}/etc/modprobe.d/wbc.conf" ] || fail "an upgrade deleted modprobe.d/wbc.conf"
sh "${root}/postun" 0 || fail "%postun 0 failed"
[ ! -e "${root}/etc/modules-load.d/wbc.conf" ] || fail "removal left modules-load.d/wbc.conf"
[ ! -e "${root}/etc/modprobe.d/wbc.conf" ] || fail "removal left modprobe.d/wbc.conf"
[ -f "${root}/etc/modprobe.d/other.conf" ] || fail "removal deleted a file that is not ours"
sh "${root}/postun" 0 || fail "a second removal, with nothing left to delete, failed"

if [ "${failures}" -ne 0 ]; then
    echo "${failures} check(s) failed" >&2
    exit 1
fi
echo "the RPM scriptlets never fail the installation and keep the setting across upgrades"

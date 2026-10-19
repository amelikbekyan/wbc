#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the install hook every package calls and the Debian postinst and
# postrm, configured by tests/CMakeLists.txt with the helper, the hook and
# /etc pointed into the build tree, so that nothing here needs root or
# touches the real system.
#
# Usage: maintainer-scripts-test.sh <install hook> <postinst> <postrm> <fake helper path> <fake etc>

set -u

hook="$1"
postinst="$2"
postrm="$3"
helper="$4"
etc="$5"

failures=0
fail() {
    echo "FAIL: $1" >&2
    failures=$((failures + 1))
}

# The fake helper logs its arguments and exits with FAKE_HELPER_EXIT.
mkdir -p "$(dirname "${helper}")"
cat >"${helper}" <<'FAKE'
#!/bin/sh
echo "$*" >>"$0.log"
echo "wbc-setup-loopback: the fake helper says no" >&2
exit "${FAKE_HELPER_EXIT:-0}"
FAKE
chmod +x "${helper}"
log="${helper}.log"

# Whatever the helper answers, the hook succeeds, so no package's
# installation fails because of it.
for code in 0 1 2 3; do
    rm -f "${log}"
    FAKE_HELPER_EXIT="${code}" "${hook}" 2>"${log}.stderr"
    status=$?
    [ "${status}" -eq 0 ] || fail "the hook exited ${status} when the helper exited ${code}"
    [ "$(cat "${log}")" = "Webcam Background Changer 10" ] ||
        fail "the hook passed '$(cat "${log}")' to the helper"
    if [ "${code}" -ne 0 ]; then
        grep -q "could not be set up now" "${log}.stderr" ||
            fail "the hook said nothing when the helper exited ${code}"
    elif grep -q "could not be set up now" "${log}.stderr"; then
        fail "the hook reported a failure when the helper succeeded"
    fi
done

# A helper that is not there at all, as when the package is half-unpacked.
mv "${helper}" "${helper}.away"
"${hook}" 2>/dev/null || fail "the hook failed without a helper"
mv "${helper}.away" "${helper}"

# The postinst runs the hook on configure, and only then: an aborted upgrade
# does not.
rm -f "${log}"
sh "${postinst}" configure 2>/dev/null || fail "postinst configure failed"
[ -e "${log}" ] || fail "postinst configure did not run the hook"
rm -f "${log}"
sh "${postinst}" abort-upgrade 1.0 || fail "postinst abort-upgrade failed"
[ ! -e "${log}" ] || fail "postinst ran the hook for abort-upgrade"

# Nor does a failing or missing hook fail the postinst.
FAKE_HELPER_EXIT=1 sh "${postinst}" configure 2>/dev/null || fail "postinst failed when the helper did"
mv "${hook}" "${hook}.away"
sh "${postinst}" configure 2>/dev/null || fail "postinst failed without the hook"
mv "${hook}.away" "${hook}"

# remove keeps the configuration; purge deletes it.
mkdir -p "${etc}/modules-load.d" "${etc}/modprobe.d"
echo v4l2loopback >"${etc}/modules-load.d/wbc.conf"
echo "options v4l2loopback devices=1" >"${etc}/modprobe.d/wbc.conf"
echo "options other" >"${etc}/modprobe.d/other.conf"
sh "${postrm}" remove || fail "postrm remove failed"
[ -f "${etc}/modprobe.d/wbc.conf" ] || fail "postrm remove deleted the configuration"
sh "${postrm}" purge || fail "postrm purge failed"
[ ! -e "${etc}/modules-load.d/wbc.conf" ] || fail "postrm purge left modules-load.d/wbc.conf"
[ ! -e "${etc}/modprobe.d/wbc.conf" ] || fail "postrm purge left modprobe.d/wbc.conf"
[ -f "${etc}/modprobe.d/other.conf" ] || fail "postrm purge deleted a file that is not ours"
sh "${postrm}" purge || fail "a second purge, with nothing left to delete, failed"

if [ "${failures}" -ne 0 ]; then
    echo "${failures} check(s) failed" >&2
    exit 1
fi
echo "the install hook and the maintainer scripts never fail the installation and purge what they should"

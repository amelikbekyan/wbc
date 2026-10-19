#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the Debian postinst and postrm, configured by tests/CMakeLists.txt
# with the helper and /etc pointed into the build tree, so that nothing here
# needs root or touches the real system.
#
# Usage: maintainer-scripts-test.sh <postinst> <postrm> <fake helper path> <fake etc>

set -u

postinst="$1"
postrm="$2"
helper="$3"
etc="$4"

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

# Whatever the helper answers, the installation goes on.
for code in 0 1 2 3; do
    rm -f "${log}"
    FAKE_HELPER_EXIT="${code}" sh "${postinst}" configure 2>"${log}.stderr"
    status=$?
    [ "${status}" -eq 0 ] || fail "postinst exited ${status} when the helper exited ${code}"
    [ "$(cat "${log}")" = "Webcam Background Changer 10" ] ||
        fail "postinst passed '$(cat "${log}")' to the helper"
    if [ "${code}" -ne 0 ]; then
        grep -q "could not be set up now" "${log}.stderr" ||
            fail "postinst said nothing when the helper exited ${code}"
    elif [ -s "${log}.stderr" ] && grep -q "could not be set up now" "${log}.stderr"; then
        fail "postinst reported a failure when the helper succeeded"
    fi
done

# A helper that is not there at all, as when the package is half-unpacked.
mv "${helper}" "${helper}.away"
sh "${postinst}" configure 2>/dev/null || fail "postinst failed without a helper"
mv "${helper}.away" "${helper}"

# Only configure runs it; an aborted upgrade does not.
rm -f "${log}"
sh "${postinst}" abort-upgrade 1.0 || fail "postinst abort-upgrade failed"
[ ! -e "${log}" ] || fail "postinst ran the helper for abort-upgrade"

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
echo "the maintainer scripts never fail the installation and purge what they should"

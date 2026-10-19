#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Installs a built package into a clean container of the distribution it was
# built for and checks that it is usable, then removes it and checks that it
# cleaned up. A container is a fair stand-in for the hardest case: no kernel
# modules, no v4l2loopback, so the install-time setup cannot finish and must
# still not fail the installation.
#
# Usage: ci/verify-package.sh <.deb, .rpm or .pkg.tar.zst>

set -eu

package="$(realpath "$1")"
failures=0
fail() {
    echo "FAIL: $1" >&2
    failures=$((failures + 1))
}

log="$(mktemp)"
# Through a file, not a pipe into tee: sh has no pipefail, and the pipe would
# report tee's success for an installation that failed.
install_logged() {
    "$@" >"${log}" 2>&1 || { cat "${log}"; echo "FAIL: the package did not install" >&2; exit 1; }
    cat "${log}"
}
# The module is only recommended, and is left out here on purpose: the
# package must install without it.
if command -v apt-get >/dev/null; then
    apt-get update
    install_logged env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${package}"
    remove() { DEBIAN_FRONTEND=noninteractive apt-get purge -y wbc; }
elif command -v dnf >/dev/null; then
    install_logged dnf install -y --setopt=install_weak_deps=False "${package}"
    remove() { dnf remove -y wbc; }
elif command -v pacman >/dev/null; then
    pacman -Sy --noconfirm
    install_logged pacman -U --noconfirm "${package}"
    remove() { pacman -R --noconfirm wbc; }
else
    echo "no apt-get, dnf or pacman here" >&2
    exit 1
fi
[ -x /usr/bin/wbc ] || { echo "FAIL: the package did not install" >&2; exit 1; }

# A library the package does not depend on would only show when a user
# starts the program.
if ldd /usr/bin/wbc | grep "not found"; then
    fail "/usr/bin/wbc needs libraries the package did not bring"
fi

for file in /usr/libexec/wbc/wbc-setup-loopback /usr/libexec/wbc/wbc-install-hook; do
    [ -x "${file}" ] || fail "${file} is missing or not executable"
done
for file in /usr/share/polkit-1/actions/org.wbc.setup-loopback.policy \
            /usr/share/metainfo/io.github.amelikbekyan.wbc.metainfo.xml \
            /usr/share/applications/wbc.desktop \
            /usr/share/wbc/selfie_segmentation.onnx; do
    [ -f "${file}" ] || fail "${file} is missing"
done
grep -q 'exec.path">/usr/libexec/wbc/wbc-setup-loopback<' \
    /usr/share/polkit-1/actions/org.wbc.setup-loopback.policy ||
    fail "the polkit action does not name the installed helper"

# The hook ran: the helper writes the boot-time setting before it looks for
# the module, and here it cannot find one, so it said why.
[ -f /etc/modprobe.d/wbc.conf ] || fail "the install-time setup did not run"
grep -q "could not be set up now" "${log}" ||
    fail "the installation did not say that the virtual camera still needs setting up"

remove
[ ! -e /usr/bin/wbc ] || fail "removing the package left /usr/bin/wbc"
for file in /etc/modules-load.d/wbc.conf /etc/modprobe.d/wbc.conf; do
    [ ! -e "${file}" ] || fail "removing the package left ${file}"
done

if [ "${failures}" -ne 0 ]; then
    echo "${failures} check(s) failed" >&2
    exit 1
fi
echo "$(basename "${package}") installs, is complete, and removes cleanly"

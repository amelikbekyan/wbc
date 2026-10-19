#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs packaging/wbc-setup-loopback against a temporary root and a fake
# modprobe, so that every exit code and the idempotence promise are checked
# without root, without the kernel module and without touching /etc.
#
# Usage: setup-loopback-test.sh <path to wbc-setup-loopback> <module install command>

set -u

helper="$1"
install_command="$2"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

failures=0
fail() {
    echo "FAIL: $1" >&2
    failures=$((failures + 1))
}

label="Webcam Background Changer"
modules_load="etc/modules-load.d/wbc.conf"
modprobe_d="etc/modprobe.d/wbc.conf"
expected_options='options v4l2loopback devices=1 video_nr=10 card_label="Webcam Background Changer" exclusive_caps=1'

# The fake modprobe logs every call and keeps its state as files in the root:
# "installed" makes the module exist, "broken" makes loading it fail, and a
# load creates the sysfs entries a real one would, with the label it was given.
cat >"${work}/modprobe" <<'EOF'
#!/bin/sh
echo "$*" >>"${WBC_SETUP_ROOT}/modprobe.log"
if [ "$1" = "-n" ]; then
    [ -f "${WBC_SETUP_ROOT}/installed" ]
    exit $?
fi
if [ -f "${WBC_SETUP_ROOT}/broken" ]; then
    echo "modprobe: ERROR: could not insert 'v4l2loopback': Operation not permitted" >&2
    exit 1
fi
for argument in "$@"; do
    case "${argument}" in
        card_label=*) name="${argument#card_label=}" ;;
    esac
done
mkdir -p "${WBC_SETUP_ROOT}/sys/module/v4l2loopback" "${WBC_SETUP_ROOT}/sys/class/video4linux/video10"
printf '%s\n' "${name}" >"${WBC_SETUP_ROOT}/sys/class/video4linux/video10/name"
EOF
chmod +x "${work}/modprobe"

# A fresh root for each case. Prints its path.
new_root() {
    root="${work}/$1"
    mkdir -p "${root}"
    touch "${root}/modprobe.log"
    echo "${root}"
}

# Runs the helper in a root; leaves the exit code in $status and what it
# wrote to standard error in ${root}/stderr.
run() {
    root="$1"
    shift
    WBC_SETUP_ROOT="${root}" WBC_SETUP_MODPROBE="${work}/modprobe" \
        sh "${helper}" "$@" 2>"${root}/stderr"
    status=$?
}

expect_status() {
    [ "${status}" -eq "$1" ] || fail "$2: exit ${status}, expected $1 ($(cat "${root}/stderr"))"
}

loads() {
    grep -c -v '^-n ' "$1/modprobe.log"
}

# A first run on a machine with the module installed and not loaded writes
# both files with exactly the expected content and loads the module.
root="$(new_root fresh)"
touch "${root}/installed"
run "${root}" "${label}" 10
expect_status 0 "fresh setup"
[ "$(cat "${root}/${modules_load}")" = "v4l2loopback" ] || fail "fresh setup: modules-load.d content"
[ "$(cat "${root}/${modprobe_d}")" = "${expected_options}" ] || fail "fresh setup: modprobe.d content"
[ "$(loads "${root}")" -eq 1 ] || fail "fresh setup: modprobe was not run exactly once"
grep -q "^v4l2loopback devices=1 video_nr=10 card_label=${label} exclusive_caps=1\$" "${root}/modprobe.log" ||
    fail "fresh setup: modprobe arguments were $(cat "${root}/modprobe.log")"
[ "$(cat "${root}/sys/class/video4linux/video10/name")" = "${label}" ] ||
    fail "fresh setup: the label did not reach modprobe as one argument"

# Running it again changes nothing: neither file is rewritten, and the module
# is not loaded a second time.
before="$(stat -c '%i %Y' "${root}/${modules_load}" "${root}/${modprobe_d}")"
sleep 1
run "${root}" "${label}" 10
expect_status 0 "second run"
[ "$(stat -c '%i %Y' "${root}/${modules_load}" "${root}/${modprobe_d}")" = "${before}" ] ||
    fail "second run: a file that was already right was rewritten"
[ "$(loads "${root}")" -eq 1 ] || fail "second run: modprobe loaded the module again"
[ -z "$(ls "${root}/etc/modprobe.d" | grep -v '^wbc.conf$')" ] || fail "second run: a temporary file was left behind"

# A file with other content, even one only a newline longer, is replaced.
root="$(new_root stale)"
touch "${root}/installed"
mkdir -p "${root}/etc/modprobe.d"
printf '%s\n\n' "${expected_options}" >"${root}/${modprobe_d}"
run "${root}" "${label}" 10
expect_status 0 "stale file"
[ "$(wc -l <"${root}/${modprobe_d}")" -eq 1 ] || fail "stale file: content was not replaced"

# Not installed: exit 2, saying which package to install, without loading
# anything. The files are written all the same, so that installing the module
# later is enough.
root="$(new_root missing)"
run "${root}" "${label}" 10
expect_status 2 "module missing"
grep -qF -- "${install_command}" "${root}/stderr" || fail "module missing: the message does not give this build's install command"
[ "$(loads "${root}")" -eq 0 ] || fail "module missing: modprobe tried to load it"
[ -f "${root}/${modprobe_d}" ] || fail "module missing: the boot-time setting was not saved"

# Loaded by something else, with no device of ours: exit 3, a message about
# rebooting, and the module left loaded.
root="$(new_root foreign)"
touch "${root}/installed"
mkdir -p "${root}/sys/module/v4l2loopback" "${root}/sys/class/video4linux/video4"
echo "OBS Virtual Camera" >"${root}/sys/class/video4linux/video4/name"
run "${root}" "${label}" 10
expect_status 3 "foreign module"
grep -q "reboot" "${root}/stderr" || fail "foreign module: the message does not mention a reboot"
[ "$(loads "${root}")" -eq 0 ] || fail "foreign module: modprobe was run ($(cat "${root}/modprobe.log"))"
[ -f "${root}/${modprobe_d}" ] || fail "foreign module: the boot-time setting was not saved"

# Loaded already with our device is success, and nothing is loaded.
root="$(new_root existing)"
touch "${root}/installed"
mkdir -p "${root}/sys/module/v4l2loopback" "${root}/sys/class/video4linux/video10"
echo "${label}" >"${root}/sys/class/video4linux/video10/name"
run "${root}" "${label}" 10
expect_status 0 "already loaded"
[ "$(loads "${root}")" -eq 0 ] || fail "already loaded: modprobe was run"

# modprobe itself failing is exit 1, with modprobe's own words passed on.
root="$(new_root broken)"
touch "${root}/installed" "${root}/broken"
run "${root}" "${label}" 10
expect_status 1 "modprobe fails"
grep -q "Operation not permitted" "${root}/stderr" || fail "modprobe fails: its message was lost"

# Arguments that could escape the configuration line, or that are not what
# the window would send, are refused before anything is written.
for bad in "Evil\" debug=1 \"" "a;b" "\$(reboot)" ""; do
    root="$(new_root "bad-label-$(echo "${bad}" | md5sum | cut -c1-8)")"
    touch "${root}/installed"
    run "${root}" "${bad}" 10
    expect_status 1 "bad label '${bad}'"
    [ ! -e "${root}/etc" ] || fail "bad label '${bad}': a file was written"
done
for number in "x" "1x" "-1" "" "256" "010" "1000"; do
    root="$(new_root "bad-number-$(echo "${number}" | md5sum | cut -c1-8)")"
    touch "${root}/installed"
    run "${root}" "${label}" "${number}"
    expect_status 1 "bad number '${number}'"
    [ ! -e "${root}/etc" ] || fail "bad number '${number}': a file was written"
done
root="$(new_root no-arguments)"
run "${root}"
expect_status 1 "no arguments"
root="$(new_root extra-argument)"
run "${root}" "${label}" 10 extra
expect_status 1 "an extra argument"

# The boundaries of what is accepted.
for number in 0 255; do
    root="$(new_root "number-${number}")"
    touch "${root}/installed"
    run "${root}" "Under_score 9" "${number}"
    expect_status 0 "number ${number}"
done

if [ "${failures}" -ne 0 ]; then
    echo "${failures} check(s) failed" >&2
    exit 1
fi
echo "all wbc-setup-loopback checks passed"

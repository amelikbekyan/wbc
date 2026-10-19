#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the small release scripts of ci/ against made-up sources in a
# temporary directory: the tag check, the release notes, the configure log
# check and the source tarball. Each guards a release against a mistake that
# would otherwise only show on GitHub, after the files were published.
#
# Usage: release-scripts-test.sh <ci directory>

set -u

ci="$(realpath "$1")"
root="$(mktemp -d)"
trap 'rm -rf "${root}"' EXIT
cd "${root}" || exit 1

failures=0
fail() {
    echo "FAIL: $1" >&2
    failures=$((failures + 1))
}

# --- check-tag-version.sh: the tag must match CMakeLists.txt and the spec.
mkdir -p packaging/rpm
sources() {
    echo "project(wbc VERSION $1 LANGUAGES CXX)" >CMakeLists.txt
    printf 'Name: wbc\nVersion:        %s\n' "$2" >packaging/rpm/wbc.spec
}
sources 0.1.0 0.1.0
sh "${ci}/check-tag-version.sh" v0.1.0 >/dev/null 2>&1 || fail "a matching tag was refused"
sh "${ci}/check-tag-version.sh" v0.1.1 >/dev/null 2>&1 && fail "a tag ahead of both files was accepted"
sources 0.1.1 0.1.0
sh "${ci}/check-tag-version.sh" v0.1.1 >/dev/null 2>&1 && fail "a tag the spec does not match was accepted"
sources 0.1.0 0.1.1
sh "${ci}/check-tag-version.sh" v0.1.1 >/dev/null 2>&1 && fail "a tag CMakeLists.txt does not match was accepted"

# --- release-notes.sh: one version's section, without its heading.
cat >CHANGELOG.md <<'EOF'
# Changes

## 0.2.0

- second

## 0.1.0

- first

## 0.0.1

EOF
notes="$(sh "${ci}/release-notes.sh" 0.1.0 2>/dev/null)" || fail "the notes of a listed version were refused"
[ "$(printf '%s' "${notes}" | tr -d '\n')" = "- first" ] ||
    fail "the notes of 0.1.0 were not just its section: '${notes}'"
sh "${ci}/release-notes.sh" 0.3.0 >/dev/null 2>&1 && fail "a version CHANGELOG.md does not list got notes"
sh "${ci}/release-notes.sh" 0.0.1 >/dev/null 2>&1 && fail "a version with an empty section got notes"

# --- check-configure-log.sh: a package needs translations and ximgproc.
echo "-- Configuring done" >clean.log
sh "${ci}/check-configure-log.sh" clean.log >/dev/null 2>&1 || fail "a clean configure log was refused"
# The warnings as CMakeLists.txt words them.
for missing in "lrelease not found; the application will only speak English" \
               "opencv ximgproc not found; edge refinement falls back to bilateralFilter"; do
    printf 'CMake Warning at CMakeLists.txt:1 (message):\n  %s\n' "${missing}" >missing.log
    sh "${ci}/check-configure-log.sh" missing.log >/dev/null 2>&1 &&
        fail "a configure log with '${missing}' was accepted"
done

# --- source-tarball.sh: every file git does not ignore, committed or not,
# under wbc-<version>/, and the repository's own index left as it was.
# Only the RPM and Arch builds need it, and only they install git: a Debian
# package's build has none, and skips this part.
if command -v git >/dev/null; then
    repo="${root}/repo"
    mkdir "${repo}" && cd "${repo}" || exit 1
    git init -q
    git config user.email test@example.com
    git config user.name test
    echo ignored.txt >.gitignore
    echo committed >committed.txt
    git add . && git commit -qm initial
    echo changed >committed.txt
    echo new >new.txt
    echo ignored >ignored.txt
    index_before="$(git ls-files --stage | git hash-object --stdin)"
    if sh "${ci}/source-tarball.sh" 9.9.9 "${root}/out.tar.gz"; then
        listing="$(tar -tzf "${root}/out.tar.gz")"
        for file in wbc-9.9.9/committed.txt wbc-9.9.9/new.txt wbc-9.9.9/.gitignore; do
            printf '%s\n' "${listing}" | grep -qx "${file}" || fail "the tarball lacks ${file}"
        done
        printf '%s\n' "${listing}" | grep -q ignored.txt && fail "the tarball has a file git ignores"
        [ "$(tar -xzOf "${root}/out.tar.gz" wbc-9.9.9/committed.txt)" = changed ] ||
            fail "the tarball has the committed contents, not the working tree's"
    else
        fail "source-tarball.sh failed"
    fi
    [ "$(git ls-files --stage | git hash-object --stdin)" = "${index_before}" ] ||
        fail "source-tarball.sh changed the repository's index"
else
    echo "no git here: source-tarball.sh not tested"
fi
cd "${root}" || exit 1

if [ "${failures}" -ne 0 ]; then
    echo "${failures} check(s) failed" >&2
    exit 1
fi
echo "the release scripts stop a mismatched tag, empty notes and a package without its optional parts"

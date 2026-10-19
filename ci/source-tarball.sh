#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Makes a source tarball in the layout of the one GitHub makes for a tag,
# wbc-<version>/, from the working tree: committed or not, every file git
# does not ignore. So the RPM and Arch builds work before the tag exists, and
# a local build packages what is being tested.
#
# Usage: ci/source-tarball.sh <version> <output .tar.gz>

set -eu

# safe.directory: in a container the checkout belongs to another user.
git() { command git -c safe.directory="$(pwd)" "$@"; }
# A temporary index, so that the repository's own is left as it was.
index="$(mktemp)"
trap 'rm -f "${index}"' EXIT
rm -f "${index}"
GIT_INDEX_FILE="${index}" git add -A
tree="$(GIT_INDEX_FILE="${index}" git write-tree)"
git archive --format=tar.gz --prefix="wbc-$1/" -o "$2" "${tree}"

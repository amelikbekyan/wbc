#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Renders packaging/wbc.svg to the PNG sizes the window and the desktop use.
# The PNGs are committed, so that neither building nor running the program
# needs an SVG renderer; run this after changing the SVG.

set -euo pipefail
cd "$(dirname "$0")"
mkdir -p icons
for size in 16 24 32 48 64 128 256; do
    rsvg-convert -w "${size}" -h "${size}" wbc.svg -o "icons/wbc-${size}.png"
done
echo "rendered: $(ls icons | tr '\n' ' ')"

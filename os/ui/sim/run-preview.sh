#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Reproducible host-only LVGL preview for the PW3 Home UI.
#
# Builds host_sim.c with the project-pinned LVGL 8.3.11 and an in-memory
# 1072x1448 framebuffer, renders Home / Settings / Calendar / Notes / Network,
# runs the geometry suite and converts the PPM snapshots to PNG.  It never
# opens /dev/fb0, /dev/input, /run/gw-system.cmd or any physical device.
#
# Environment overrides: MAKE, HOSTCC, MAGICK, GW_UI_PREVIEW_BUILD.
set -eu

UI_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$UI_DIR/../.." && pwd)
OUT=${1:-"$REPO_ROOT/docs/evidence/pw3-ui-redesign-2026-10-31"}
BUILD=${GW_UI_PREVIEW_BUILD:-/tmp/opencode/gw-ui-preview}

if [ -z "${MAKE:-}" ]; then
    MAKE=$(command -v make || true)
fi
if [ -z "${HOSTCC:-}" ]; then
    HOSTCC=$(command -v gcc || command -v cc || true)
fi
if [ -z "${MAGICK:-}" ]; then
    if command -v magick >/dev/null 2>&1; then MAGICK=$(command -v magick)
    else MAGICK=$(command -v convert || true); fi
fi
[ -n "$MAKE" ] || { echo "ERROR: make not found (set MAKE=)" >&2; exit 1; }
[ -n "$HOSTCC" ] || { echo "ERROR: host compiler not found (set HOSTCC=)" >&2; exit 1; }
[ -n "$MAGICK" ] || { echo "ERROR: ImageMagick not found (set MAGICK=)" >&2; exit 1; }

mkdir -p "$BUILD" "$OUT/screenshots"

"$MAKE" -C "$UI_DIR" host-sim HOSTCC="$HOSTCC"
"$UI_DIR/host-sim" "$BUILD"

SCREENS="home home-empty settings settings-scrolled \
calendar calendar-next-month calendar-selected calendar-empty \
todos todos-history todos-empty todos-undo \
network network-connected network-connecting network-list network-password \
network-confirm network-empty \
apps apps-empty apps-long lock confirm-remove kit"

for screen in $SCREENS; do
    if [ ! -f "$BUILD/$screen.ppm" ]; then
        echo "ERROR: missing $screen.ppm from host-sim" >&2
        exit 1
    fi
    "$MAGICK" "$BUILD/$screen.ppm" -colorspace Gray -strip \
        "$OUT/screenshots/$screen.png"
    size=$("$MAGICK" identify -format '%wx%h' "$OUT/screenshots/$screen.png")
    if [ "$size" != "1072x1448" ]; then
        echo "ERROR: $screen.png is $size, expected 1072x1448" >&2
        exit 1
    fi
done

cp "$BUILD/geometry.json" "$OUT/geometry.json"
rm -f "$OUT/screenshots/network-popup.png"
echo "Preview written to $OUT/screenshots (1072x1448 each)"
echo "Geometry report: $OUT/geometry.json"

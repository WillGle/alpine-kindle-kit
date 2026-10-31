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
    if command -v make >/dev/null 2>&1; then MAKE=$(command -v make)
    else MAKE=$(ls /nix/store/*gnumake*/bin/make 2>/dev/null | head -n 1 || true); fi
fi
if [ -z "${HOSTCC:-}" ]; then
    if command -v gcc >/dev/null 2>&1; then HOSTCC=$(command -v gcc)
    else HOSTCC=$(ls /nix/store/*gcc-wrapper*/bin/gcc 2>/dev/null | head -n 1 || true); fi
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

for screen in home settings calendar todos todos-history network apps update home-empty network-popup; do
    [ -f "$BUILD/$screen.ppm" ] || continue
    "$MAGICK" "$BUILD/$screen.ppm" -colorspace Gray -strip \
        "$OUT/screenshots/$screen.png"
    size=$("$MAGICK" identify -format '%wx%h' "$OUT/screenshots/$screen.png")
    if [ "$size" != "1072x1448" ]; then
        echo "ERROR: $screen.png is $size, expected 1072x1448" >&2
        exit 1
    fi
done

cp "$BUILD/geometry.json" "$OUT/geometry.json"
echo "Preview written to $OUT/screenshots (1072x1448 each)"
echo "Geometry report: $OUT/geometry.json"

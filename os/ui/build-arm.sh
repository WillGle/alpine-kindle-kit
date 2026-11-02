#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the real ARM/musl gw-shell artifact on the host.  This script does not
# deploy, copy, SSH, or touch a Kindle.
#
# Overrides: CC, STRIP, MUSL_PREFIX, LINUX_HEADERS, LVGL_DIR, LEGACY_LIBS,
# GMP_LIB, OUT.
set -eu

UI_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$UI_DIR/../.." && pwd)
# shellcheck source=../../tools/build/toolchain.sh
. "$REPO_ROOT/tools/build/toolchain.sh"
OUT=${OUT:-"$UI_DIR/gw-shell"}

CFLAGS_ARM="-std=c99 -Wall -Wextra -Os -D_GNU_SOURCE \
    -marm -march=armv7-a -mfloat-abi=soft \
    -nostdinc -isystem $MUSL_PREFIX/include \
    -idirafter $LINUX_HEADERS \
    -L$MUSL_PREFIX/lib -B$MUSL_PREFIX/lib -static"

"$MAKE" -C "$UI_DIR" clean >/dev/null 2>&1 || true
"$MAKE" -C "$UI_DIR" \
    CC="$CC" STRIP="$STRIP" LVGL_DIR="$LVGL_DIR" CFLAGS="$CFLAGS_ARM" \
    GW_UI_LIFECYCLE_SPIKE=1 GW_UI_EPDC_ACCEPTED=0 GW_UI_TOUCH_ACCEPTED=0 \
    GW_ENABLE_HW_TRIAL=${GW_ENABLE_HW_TRIAL:-0} PIEFLAGS= \
    GW_UI_PERF=${GW_UI_PERF:-0} GW_UI_DIAG=${GW_UI_DIAG:-0} GW_UI_PERF_WAIT=${GW_UI_PERF_WAIT:-0} -j4

if [ "$OUT" != "$UI_DIR/gw-shell" ]; then cp "$UI_DIR/gw-shell" "$OUT"; fi
echo "ARM gw-shell: $OUT"
sha256sum "$OUT"

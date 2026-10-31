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
TOOLCHAIN=${TOOLCHAIN:-"$REPO_ROOT/work/toolchains/toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin"}
CC=${CC:-"$TOOLCHAIN/arm-linux-gnueabi-gcc"}
STRIP=${STRIP:-"$TOOLCHAIN/arm-linux-gnueabi-strip"}
MUSL_PREFIX=${MUSL_PREFIX:-"$REPO_ROOT/work/toolchains/musl-arm-prefix"}
LINUX_HEADERS=${LINUX_HEADERS:-/nix/store/0xlb57h8i6jjzx4k44z363ig29yxkwya-linux-headers-6.18.7/include}
LVGL_DIR=${LVGL_DIR:-"$REPO_ROOT/work/sources/lvgl"}
LEGACY_LIBS=${LEGACY_LIBS:-"$REPO_ROOT/work/toolchains/legacy-host-libs/lib"}
GMP_LIB=${GMP_LIB:-$(dirname "$(ls /nix/store/*gmp-6*/lib/libgmp.so.10 2>/dev/null | head -n 1 || true)" 2>/dev/null || true)}
OUT=${OUT:-"$UI_DIR/gw-shell"}

[ -x "$CC" ] || { echo "ERROR: ARM compiler not found: $CC" >&2; exit 1; }
[ -d "$MUSL_PREFIX/include" ] || { echo "ERROR: musl prefix not found" >&2; exit 1; }
[ -d "$LVGL_DIR/src" ] || { echo "ERROR: LVGL source not found" >&2; exit 1; }

export LD_LIBRARY_PATH="$LEGACY_LIBS${GMP_LIB:+:$GMP_LIB}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [ -z "${MAKE:-}" ]; then
    if command -v make >/dev/null 2>&1; then MAKE=$(command -v make)
    else MAKE=$(ls /nix/store/*gnumake*/bin/make 2>/dev/null | head -n 1 || true); fi
fi
[ -n "$MAKE" ] || { echo "ERROR: make not found (set MAKE=)" >&2; exit 1; }

CFLAGS_ARM="-std=c99 -Wall -Wextra -Os -D_GNU_SOURCE \
    -marm -march=armv7-a -mfloat-abi=soft \
    -nostdinc -isystem $MUSL_PREFIX/include \
    -idirafter $LINUX_HEADERS \
    -L$MUSL_PREFIX/lib -B$MUSL_PREFIX/lib -static"

"$MAKE" -C "$UI_DIR" clean >/dev/null 2>&1 || true
"$MAKE" -C "$UI_DIR" \
    CC="$CC" STRIP="$STRIP" LVGL_DIR="$LVGL_DIR" CFLAGS="$CFLAGS_ARM" \
    GW_UI_LIFECYCLE_SPIKE=1 GW_UI_EPDC_SPIKE=1 GW_UI_TOUCH_SPIKE=1 \
    GW_UI_EPDC_ACCEPTED=0 GW_UI_TOUCH_ACCEPTED=0 \
    GW_UI_EINK_FAST_MODE=${GW_UI_EINK_FAST_MODE:-1} \
    GW_UI_PERF=${GW_UI_PERF:-0} GW_UI_DIAG=${GW_UI_DIAG:-0} -j4

if [ "$OUT" != "$UI_DIR/gw-shell" ]; then cp "$UI_DIR/gw-shell" "$OUT"; fi
echo "ARM gw-shell: $OUT"
sha256sum "$OUT"

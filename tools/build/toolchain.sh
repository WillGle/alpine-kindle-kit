#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Single source of truth for the ARM/musl cross-build environment.
#
# Source this file from build scripts.  It validates every path and fails
# closed; it never guesses an environment-manager store path and never adds "." to
# LD_LIBRARY_PATH.  Set the environment variables to match your Nix shell.
#
# Required:
#   LINUX_HEADERS   Linux UAPI headers (include/)
#   GMP_LIB         directory containing libgmp.so.10
#   MPC_LIB         directory containing libmpc.so.3
# Optional overrides:
#   TOOLCHAIN, CC, STRIP, MUSL_PREFIX, LVGL_DIR, DYN_ROOT, LEGACY_LIBS

if [ -n "${GW_TOOLCHAIN_LOADED:-}" ]; then
    return 0 2>/dev/null || exit 0
fi
GW_TOOLCHAIN_LOADED=1

TOOLCHAIN_REPO=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TOOLCHAIN=${TOOLCHAIN:-"$TOOLCHAIN_REPO/work/toolchains/toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin"}
CC=${CC:-"$TOOLCHAIN/arm-linux-gnueabi-gcc"}
STRIP=${STRIP:-"$TOOLCHAIN/arm-linux-gnueabi-strip"}
MUSL_PREFIX=${MUSL_PREFIX:-"$TOOLCHAIN_REPO/work/toolchains/musl-arm-prefix"}
LEGACY_LIBS=${LEGACY_LIBS:-"$TOOLCHAIN_REPO/work/toolchains/legacy-host-libs/lib"}
LVGL_DIR=${LVGL_DIR:-"$TOOLCHAIN_REPO/work/sources/lvgl"}
DYN_ROOT=${DYN_ROOT:-"$TOOLCHAIN_REPO/work/build/pw3-reader-storage-20261101/root"}

: "${LINUX_HEADERS:?set LINUX_HEADERS to the Linux UAPI include directory}"
: "${GMP_LIB:?set GMP_LIB to the directory containing libgmp.so.10}"
: "${MPC_LIB:?set MPC_LIB to the directory containing libmpc.so.3}"

[ -x "$CC" ] || { echo "ERROR: ARM compiler not found: $CC" >&2; return 1 2>/dev/null || exit 1; }
[ -x "$STRIP" ] || { echo "ERROR: ARM strip not found: $STRIP" >&2; return 1 2>/dev/null || exit 1; }
[ -d "$MUSL_PREFIX/include" ] || { echo "ERROR: musl prefix not found: $MUSL_PREFIX" >&2; return 1 2>/dev/null || exit 1; }
[ -d "$LINUX_HEADERS" ] || { echo "ERROR: Linux UAPI headers not found: $LINUX_HEADERS" >&2; return 1 2>/dev/null || exit 1; }
[ -d "$GMP_LIB" ] || { echo "ERROR: GMP library directory not found: $GMP_LIB" >&2; return 1 2>/dev/null || exit 1; }
[ -d "$MPC_LIB" ] || { echo "ERROR: MPC library directory not found: $MPC_LIB" >&2; return 1 2>/dev/null || exit 1; }
[ -d "$LVGL_DIR/src" ] || { echo "ERROR: LVGL source not found: $LVGL_DIR" >&2; return 1 2>/dev/null || exit 1; }

case ":$LEGACY_LIBS:$GMP_LIB:$MPC_LIB:" in
    *::*|*":.:"*)
        echo "ERROR: refusing an empty or '.' library path" >&2
        return 1 2>/dev/null || exit 1
        ;;
esac

export TOOLCHAIN CC STRIP MUSL_PREFIX LINUX_HEADERS GMP_LIB MPC_LIB
export LEGACY_LIBS LVGL_DIR DYN_ROOT
export LD_LIBRARY_PATH="$LEGACY_LIBS:$GMP_LIB:$MPC_LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [ -z "${MAKE:-}" ]; then
    if command -v make >/dev/null 2>&1; then MAKE=$(command -v make)
    else echo "ERROR: make not found (set MAKE=)" >&2; return 1 2>/dev/null || exit 1; fi
fi
export MAKE

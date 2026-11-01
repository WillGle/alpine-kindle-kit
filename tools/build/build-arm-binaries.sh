#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the four ARM/musl device binaries without touching a device.
#
# gw-system/gw-appctl must be dynamic: they load /usr/lib/libcrypto.so.3 with
# dlopen for signature verification, and musl static binaries cannot dlopen.
# gw-shell/gwctl are static.  Dynamic libc/libcrypto inputs default to the
# private prepared root; override DYN_ROOT if retention moved.
#
# Overrides: TOOLCHAIN, MUSL_PREFIX, LINUX_HEADERS, DYN_ROOT, OUT, GMP_LIB.
set -eu

REPO=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TOOLCHAIN=${TOOLCHAIN:-"$REPO/work/toolchains/toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin"}
CC=${CC:-"$TOOLCHAIN/arm-linux-gnueabi-gcc"}
MUSL_PREFIX=${MUSL_PREFIX:-"$REPO/work/toolchains/musl-arm-prefix"}
LINUX_HEADERS=${LINUX_HEADERS:-/nix/store/9licwzbc8pzhrh43vz5wg137gky9anz3-linux-headers-6.18.7/include}
DYN_ROOT=${DYN_ROOT:-"$REPO/work/build/pw3-reader-storage-20261101/root"}
OUT=${OUT:-"$REPO/work/build/arm-hardening-$(date +%Y%m%d)"}
LEGACY_LIBS=${LEGACY_LIBS:-"$REPO/work/toolchains/legacy-host-libs/lib"}
GMP_LIB=${GMP_LIB:-/nix/store/1sf1j6c8rbipp4yayll7vy6v4bpwz95y-gmp-6.3.0/lib}
MPC_LIB=${MPC_LIB:-/nix/store/9wxp73cmlcjzgi78i2109365jdxvqzgq-libmpc-1.4.0/lib}

[ -x "$CC" ] || { echo "ERROR: ARM compiler not found: $CC" >&2; exit 1; }
[ -d "$MUSL_PREFIX/lib" ] || { echo "ERROR: musl prefix not found" >&2; exit 1; }
[ -d "$LINUX_HEADERS" ] || { echo "ERROR: Linux UAPI headers not found" >&2; exit 1; }
[ -f "$DYN_ROOT/lib/ld-musl-armhf.so.1" ] ||
    { echo "ERROR: dynamic musl libc not found under DYN_ROOT=$DYN_ROOT" >&2; exit 1; }
[ -f "$DYN_ROOT/usr/lib/libcrypto.so.3" ] ||
    { echo "ERROR: libcrypto.so.3 not found under DYN_ROOT=$DYN_ROOT" >&2; exit 1; }

export LD_LIBRARY_PATH="$LEGACY_LIBS:$GMP_LIB:$MPC_LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

MAKE=${MAKE:-$(command -v make || true)}
[ -n "$MAKE" ] || { echo "ERROR: make not found" >&2; exit 1; }

COMMON="-std=c99 -Wall -Wextra -Os -fno-stack-protector -D_FORTIFY_SOURCE=2 \
    -marm -march=armv7-a -mfloat-abi=soft -nostdinc \
    -isystem $MUSL_PREFIX/include -idirafter $LINUX_HEADERS \
    -DGW_POWER_SUSPEND_ACCEPTED=1 -DGW_POWER_LIGHT_ACCEPTED=1 \
    -DGW_POWER_LIGHT_SPIKE=0 -DGW_LIFECYCLE_SPIKE=1 \
    -DGW_WIFI_SPIKE=1 -DGW_WIFI_ACCEPTED=0 \
    -DGW_POWER_POWEROFF_ACCEPTED=0"

# Dynamic sysroot staged from the prepared Alpine root.
DYN_LIBS="$OUT/.dynlibs"
mkdir -p "$OUT" "$DYN_LIBS"
cp "$DYN_ROOT/lib/ld-musl-armhf.so.1" "$DYN_ROOT/usr/lib/libcrypto.so.3" "$DYN_LIBS/"
ln -sf ld-musl-armhf.so.1 "$DYN_LIBS/libc.so"
ln -sf ld-musl-armhf.so.1 "$DYN_LIBS/libdl.so"
ln -sf ld-musl-armhf.so.1 "$DYN_LIBS/librt.so"

"$MAKE" -C "$REPO/os/system" clean >/dev/null

# Static tools that never call dlopen.
"$MAKE" -C "$REPO/os/system" CC="$CC" CFLAGS="$COMMON -static -L$MUSL_PREFIX/lib -B$MUSL_PREFIX/lib" \
    LDFLAGS= PIEFLAGS= -j4 gwctl

# Dynamic loader present on the deployed image; musl libc owns dlopen/clock.
"$MAKE" -C "$REPO/os/system" CC="$CC" \
    CFLAGS="$COMMON -L$DYN_LIBS -L$MUSL_PREFIX/lib -B$MUSL_PREFIX/lib" \
    LIBS= LDFLAGS="-Wl,-z,relro,-z,now -Wl,-dynamic-linker,/lib/ld-musl-armhf.so.1" \
    PIEFLAGS= -j4 gw-system gw-appctl

cp "$REPO/os/system/gw-system" "$REPO/os/system/gwctl" \
   "$REPO/os/system/gw-appctl" "$OUT/"
OUT="$OUT/gw-shell" env -u CC -u STRIP -u CFLAGS \
    "$REPO/os/ui/build-arm.sh"
chmod 700 "$OUT/gw-shell" "$OUT/gw-system" "$OUT/gw-appctl" "$OUT/gwctl"
( cd "$OUT" && sha256sum gw-system gwctl gw-appctl gw-shell > SHA256SUMS )
echo "ARM binaries: $OUT"
cat "$OUT/SHA256SUMS"

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# QEMU target gate: build and execute ARM PW3 test binaries on an x86-64 host.
set -eu

REPO=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
ROOT=${PW3_ROOT:-"$REPO/work/build/pw3-reader-storage-20261101/root"}
MUSL=${MUSL_PREFIX:-"$REPO/work/toolchains/musl-arm-prefix"}
TOOLCHAIN=${TOOLCHAIN:-"$REPO/work/toolchains/toolchain-gcc-4.9.4/gcc-4.9.4-nolibc/arm-linux-gnueabi/bin"}
CC=${CC:-"$TOOLCHAIN/arm-linux-gnueabi-gcc"}
QEMU_ARM=${QEMU_ARM:-$(command -v qemu-arm || true)}
OUT=${OUT:-"$(mktemp -d /tmp/pw3-qemu-arm.XXXXXX)"}
KEY="$REPO/os/system/tests/keys/test-release.pem"
FIX="$REPO/os/system/tests/fixtures"
KERNEL_HEADERS=${KERNEL_HEADERS:-/nix/store/9licwzbc8pzhrh43vz5wg137gky9anz3-linux-headers-6.18.7/include}
LEGACY_LIBS=${LEGACY_LIBS:-"$REPO/work/toolchains/legacy-host-libs/lib"}
GMP_LIB=${GMP_LIB:-/nix/store/1sf1j6c8rbipp4yayll7vy6v4bpwz95y-gmp-6.3.0/lib}
MPC_LIB=${MPC_LIB:-/nix/store/9wxp73cmlcjzgi78i2109365jdxvqzgq-libmpc-1.4.0/lib}

[ "$(uname -s)" = Linux ] || { echo "ERROR: host must be Linux" >&2; exit 1; }
case "$(uname -m)" in x86_64|amd64) ;; *) echo "ERROR: host must be x86-64/AMD64" >&2; exit 1;; esac
[ -n "$QEMU_ARM" ] || { echo "ERROR: qemu-arm not found" >&2; exit 1; }
[ -x "$CC" ] || { echo "ERROR: ARM compiler not found" >&2; exit 1; }
[ -d "$ROOT" ] || { echo "ERROR: prepared ARM root not found: $ROOT" >&2; exit 1; }
[ -f "$ROOT/usr/lib/libcrypto.so.3" ] || { echo "ERROR: ARM libcrypto missing" >&2; exit 1; }
[ -x "$ROOT/bin/busybox" ] || { echo "ERROR: ARM busybox missing" >&2; exit 1; }
[ -f "$KEY" ] || { echo "ERROR: test key missing" >&2; exit 1; }
mkdir -p "$OUT"

export LD_LIBRARY_PATH="$LEGACY_LIBS:$GMP_LIB:$MPC_LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
CFLAGS="-std=c99 -O2 -Wall -Wextra -fno-stack-protector -marm -march=armv7-a -mfloat-abi=soft -nostdinc -isystem $MUSL/include -idirafter $KERNEL_HEADERS -L$MUSL/lib -B$MUSL/lib"

echo "==> ARM static policy tests"
"$CC" $CFLAGS -static -I"$REPO/os/system" -o "$OUT/test-power-policy" \
    "$REPO/os/system/test-power-policy.c" "$REPO/os/system/gw-power-policy.c"
"$QEMU_ARM" "$OUT/test-power-policy"

"$CC" $CFLAGS -static -I"$REPO/os/system" -o "$OUT/test-stage3" \
    "$REPO/os/system/test-stage3.c" "$REPO/os/system/gw-stage3.c"
"$QEMU_ARM" "$OUT/test-stage3"

echo "==> ARM dynamic signature + fingerprint test"
SHA=$(sha256sum "$KEY" | awk '{print $1}')
DYN_LIBS="$OUT/dynlibs"
mkdir -p "$DYN_LIBS"
cp "$ROOT/lib/ld-musl-armhf.so.1" "$ROOT/usr/lib/libcrypto.so.3" "$DYN_LIBS/"
ln -sf ld-musl-armhf.so.1 "$DYN_LIBS/libc.so"
"$CC" $CFLAGS -L"$DYN_LIBS" \
    -DGW_CRYPTO_LIB="\"$ROOT/usr/lib/libcrypto.so.3\"" \
    -DGW_APP_KEY_SHA256="\"$SHA\"" \
    -Wl,-dynamic-linker,/lib/ld-musl-armhf.so.1 \
    -I"$REPO/os/system" -o "$OUT/test-signature" \
    "$REPO/vm/qemu-arm/test-signature.c" "$REPO/os/system/gw-signature.c" \
    "$DYN_LIBS/libc.so"
"$QEMU_ARM" -L "$ROOT" "$OUT/test-signature" \
    "$FIX/valid.gwapp" "$FIX/wrong-key.gwapp" "$KEY"

echo "==> ARM CLI smoke"
if [ -x "$REPO/work/build/arm-hardening-20261101/gw-appctl" ]; then
    "$QEMU_ARM" -L "$ROOT" "$REPO/work/build/arm-hardening-20261101/gw-appctl" || [ $? -eq 2 ]
fi

echo "QEMU ARM TARGET GATE: PASS ($OUT)"

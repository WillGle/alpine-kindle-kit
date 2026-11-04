#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Build into a new output tree; never clean or replace existing device binaries.
set -eu
REPO=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
: "${OUT:?set OUT to a new build directory}"
KEY=${GW_APP_PUBLIC_KEY:-"$REPO/os/system/app-keys/release.pem"}
# Core userspace must match Alpine's hard-float ABI. The legacy PW3 bootstrap
# toolchain is soft-float, including its GCC runtime, and stays unchanged.
CORE_TOOLCHAIN=${CORE_TOOLCHAIN:-"$REPO/work/toolchains/bootlin-armv7-musl-2026.08/armv7-eabihf--musl--stable-2026.08-1"}
CC=${CC:-"$CORE_TOOLCHAIN/bin/arm-buildroot-linux-musleabihf-gcc"}
STRIP=${STRIP:-"$CORE_TOOLCHAIN/bin/arm-buildroot-linux-musleabihf-strip"}
MAKE=${MAKE:-make}
DYN_ROOT=${DYN_ROOT:-"$REPO/work/build/pw3-reader-storage-20261101/root"}
export LVGL_DIR=${LVGL_DIR:-"$REPO/work/sources/lvgl"}
case $("$CC" -dumpmachine) in
    arm*linux*musleabihf) ;;
    *) echo "Core requires an ARM musl hard-float compiler, not the legacy toolchain" >&2; exit 1 ;;
esac
[ ! -e "$OUT" ] || { echo "Output already exists: $OUT" >&2; exit 1; }
case "$OUT" in /*) ;; *) echo "OUT must be absolute" >&2; exit 1 ;; esac
# Validate the loader input before creating any build output.
python3 - "$DYN_ROOT/lib/ld-musl-armhf.so.1" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()[:52]
if (len(data) != 52 or data[:6] != b'\x7fELF\x01\x01' or
        int.from_bytes(data[18:20], 'little') != 40 or
        int.from_bytes(data[36:40], 'little') & 0x600 != 0x400):
    raise SystemExit('Core loader must be ARM ELF32 little-endian hard-float')
PY
mkdir -p "$OUT/lib"
cp "$DYN_ROOT/lib/ld-musl-armhf.so.1" "$OUT/lib/"
ln -s ld-musl-armhf.so.1 "$OUT/lib/libc.so"
SHA=$(sha256sum "$KEY" | awk '{print $1}')
COMMON="-std=c99 -Os -Wall -Wextra -marm -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -fno-stack-protector -L$OUT/lib -DGW_LIFECYCLE_SPIKE=1 -DGW_READER_BUILTIN=0"
# The optional KOReader export contains ~1000 regular members. Keep a bounded
# core-specific ceiling; legacy images retain their existing 512-member policy.
COMMON="$COMMON -DGW_PACKAGE_MAX_ENTRIES=4096"
"$CC" --version > "$OUT/compiler.txt"
printf '%s\n' "$COMMON" > "$OUT/compiler-flags.txt"
cd "$REPO/os/system"
# The key fingerprint is compiled in, including when a VM-only key is supplied.
"$CC" $COMMON -DGW_APP_KEY_SHA256="\"$SHA\"" \
    -Wl,-dynamic-linker,/lib/ld-musl-armhf.so.1 -o "$OUT/gw-system" \
    gw-system.c gw-system-state.c gw-system-power.c gw-app-manifest.c \
    gw-network.c gw-stage3.c gw-file.c gw-power-policy.c gw-apps.c gw-signature.c
"$CC" $COMMON -DGW_APP_KEY_SHA256="\"$SHA\"" \
    -Wl,-dynamic-linker,/lib/ld-musl-armhf.so.1 -o "$OUT/gw-appctl" \
    gw-appctl.c gw-apps.c gw-app-manifest.c gw-signature.c
"$CC" $COMMON -static -o "$OUT/gwctl" gwctl.c
"$STRIP" "$OUT/gw-system" "$OUT/gw-appctl" "$OUT/gwctl"
if [ "${GW_CORE_VM_SHELL:-0}" = 1 ]; then
    "$CC" $COMMON -static -I. -o "$OUT/vm-shell" \
        "$REPO/vm/qemu-system/core-shell.c" gw-ipc-client.c
    "$STRIP" "$OUT/vm-shell"
fi
if [ "${GW_CORE_UI:-0}" = 1 ]; then
    # Reuse the UI build definition but place its artifact in the new output;
    # no make clean and no replacement of existing workspace/device binaries.
    "$MAKE" -C "$REPO/os/ui" -f Makefile \
        --eval='$(CORE_SHELL): $(SRCS); $(CC) $(CFLAGS) $(LVGL_CFLAGS) -I../system -o $@ $(SRCS) -lm -lrt' \
        CORE_SHELL="$OUT/gw-shell" CC="$CC" CFLAGS="$COMMON -D_GNU_SOURCE -static" \
        GW_UI_LIFECYCLE_SPIKE=1 GW_UI_EPDC_ACCEPTED=0 GW_UI_TOUCH_ACCEPTED=0 \
        "$OUT/gw-shell"
    "$STRIP" "$OUT/gw-shell"
fi
cp "$KEY" "$OUT/release.pem"
(cd "$OUT" && sha256sum gw-system gw-appctl gwctl release.pem > SHA256SUMS
    for name in gw-shell vm-shell; do
        [ ! -f "$name" ] || sha256sum "$name" >> SHA256SUMS
    done)

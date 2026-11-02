#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Boot bootstrap.c as PID1 in QEMU with no /proc, forcing its early-mount
# halt path.  Proves the failure path requests reboot and does not pause
# indefinitely.
#
#   PW3_QEMU_KERNEL=/path/zImage \
#   PW3_TEST_BOOTSTRAP=/path/bootstrap \
#     ./vm/qemu-system/bootstrap-failure-test.sh
set -eu

KERNEL=${PW3_QEMU_KERNEL:-}
BOOTSTRAP=${PW3_TEST_BOOTSTRAP:-}
QEMU=${QEMU_SYSTEM_ARM:-$(command -v qemu-system-arm || true)}
OUT=${OUT:-$(mktemp -d /tmp/pw3-bootstrap-fail.XXXXXX)}

[ -f "$KERNEL" ] || { echo "ERROR: set PW3_QEMU_KERNEL" >&2; exit 1; }
[ -x "$BOOTSTRAP" ] || { echo "ERROR: set PW3_TEST_BOOTSTRAP" >&2; exit 1; }
[ -n "$QEMU" ] || { echo "ERROR: qemu-system-arm not found" >&2; exit 1; }

ROOT="$OUT/initramfs"
rm -rf "$ROOT"
mkdir -p "$ROOT"
cp "$BOOTSTRAP" "$ROOT/init"
chmod 755 "$ROOT/init"
( cd "$ROOT" && find . -print0 | cpio --null -o -H newc 2>/dev/null | gzip -9 ) \
    > "$OUT/initramfs.cpio.gz"

LOG="$OUT/boot.log"
set +e
timeout 30 "$QEMU" -M virt -cpu cortex-a15 -m 128M -nographic -no-reboot \
    -kernel "$KERNEL" -initrd "$OUT/initramfs.cpio.gz" \
    -append "console=ttyAMA0 rdinit=/init panic=-1 quiet" > "$LOG" 2>&1
status=$?
set -e
cat "$LOG"

if grep -q 'GW_BOOTSTRAP_HALT' "$LOG" &&
   grep -q 'Restarting system' "$LOG" && [ "$status" != 124 ]; then
    echo "QEMU BOOTSTRAP-FAILURE TEST: PASS ($OUT)"
    exit 0
fi
echo "QEMU BOOTSTRAP-FAILURE TEST: FAIL (qemu exit $status)" >&2
exit 1

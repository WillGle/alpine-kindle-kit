#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Full-system QEMU boot test for os/system/gleworks-device-prepare.sh.
#
# This boots a minimal Linux initramfs on QEMU virt and runs the same
# device-preparation functions used at boot.  It simulates one touch module and
# one USB gadget module as already loaded to prove both loaders are idempotent.
# It needs a QEMU-bootable 32-bit ARM kernel supplied by the caller.
#
#   PW3_QEMU_KERNEL=/path/to/zImage ./vm/qemu-system/device-prep-test.sh
set -eu

REPO=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
KERNEL=${PW3_QEMU_KERNEL:-"$REPO/vm/images/zImage"}
BUSYBOX=${PW3_TEST_BUSYBOX:-"$REPO/work/build/pw3-reader-storage-20261101/root/bin/busybox"}
QEMU=${QEMU_SYSTEM_ARM:-$(command -v qemu-system-arm || true)}
OUT=${OUT:-$(mktemp -d /tmp/pw3-qemu-boot.XXXXXX)}
TIMEOUT=${PW3_QEMU_BOOT_TIMEOUT:-60}

[ -f "$KERNEL" ] || { echo "ERROR: set PW3_QEMU_KERNEL (missing $KERNEL)" >&2; exit 1; }
[ -f "$BUSYBOX" ] || { echo "ERROR: ARM busybox not found: $BUSYBOX" >&2; exit 1; }
[ -n "$QEMU" ] || { echo "ERROR: qemu-system-arm not found" >&2; exit 1; }
command -v cpio >/dev/null 2>&1 || { echo "ERROR: cpio not found" >&2; exit 1; }
command -v gzip >/dev/null 2>&1 || { echo "ERROR: gzip not found" >&2; exit 1; }

ROOT="$OUT/initramfs"
rm -rf "$ROOT"
mkdir -p "$ROOT/bin" "$ROOT/dev" "$ROOT/etc" "$ROOT/proc" "$ROOT/run" \
         "$ROOT/sys" "$ROOT/tmp" "$ROOT/var/tmp" "$ROOT/dev/input" "$ROOT/lib"
cp "$BUSYBOX" "$ROOT/bin/busybox"
chmod 755 "$ROOT/bin/busybox"
LOADER=${PW3_TEST_LOADER:-$(dirname "$BUSYBOX")/../lib/ld-musl-armhf.so.1}
[ -f "$LOADER" ] || { echo "ERROR: musl loader not found: $LOADER" >&2; exit 1; }
cp "$LOADER" "$ROOT/lib/ld-musl-armhf.so.1"
chmod 755 "$ROOT/lib/ld-musl-armhf.so.1"
cp "$REPO/os/system/gleworks-device-prepare.sh" "$ROOT/etc/gleworks-device-prepare.sh"

# One module from each loader is already loaded: only the other two touch
# modules and the other USB gadget module may be insmod'ed.
printf 'cyttsp4_i2c 1 0 - Live 0x00000000\nfsl_otg_arc 1 0 - Live 0x00000000\n' \
    > "$ROOT/etc/fake-modules"
cat > "$ROOT/etc/fake-insmod" <<'EOF'
#!/bin/busybox sh
echo "$1" >> /run/insmod.log
name=${1##*/}
name=${name%.ko}
echo "$name 1 0 - Live 0x00000000" >> /etc/fake-modules
EOF
chmod 755 "$ROOT/etc/fake-insmod"

cat > "$ROOT/init" <<'EOF'
#!/bin/busybox sh
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t devtmpfs devtmpfs /dev
/bin/busybox mkdir -p /run /tmp /dev/input /var/tmp
export GW_MODULES_FILE=/etc/fake-modules
export GW_INSMOD=/etc/fake-insmod
. /etc/gleworks-device-prepare.sh
echo GLEWORKS_DEVICE_PREPARE_START
mount_early_filesystems
create_core_devices
create_core_devices
load_touch_modules
load_touch_modules
load_usb_gadget
load_usb_gadget
create_input_nodes
count=$(/bin/busybox wc -l < /run/insmod.log)
if [ "$count" != 3 ]; then
    echo "GLEWORKS_DEVICE_PREPARE_FAIL insmod_count=$count"
    /bin/busybox poweroff -f
fi
if [ ! -c /dev/null ] || [ ! -c /dev/fb0 ]; then
    echo "GLEWORKS_DEVICE_PREPARE_FAIL missing_core_nodes"
    /bin/busybox poweroff -f
fi
echo "GLEWORKS_DEVICE_PREPARE_PASS insmod_count=$count"
/bin/busybox poweroff -f
EOF
chmod 755 "$ROOT/init"

( cd "$ROOT" && find . -print0 | cpio --null -o -H newc 2>/dev/null | gzip -9 ) \
    > "$OUT/initramfs.cpio.gz"
[ -s "$OUT/initramfs.cpio.gz" ] || { echo "ERROR: initramfs build failed" >&2; exit 1; }

LOG="$OUT/boot.log"
set +e
timeout "$TIMEOUT" "$QEMU" -M virt -cpu cortex-a15 -m 256M -nographic \
    -no-reboot -kernel "$KERNEL" -initrd "$OUT/initramfs.cpio.gz" \
    -append "console=ttyAMA0 rdinit=/init panic=-1 quiet" > "$LOG" 2>&1
status=$?
set -e

cat "$LOG"
if grep -q 'GLEWORKS_DEVICE_PREPARE_PASS' "$LOG" &&
   ! grep -q 'GLEWORKS_DEVICE_PREPARE_FAIL' "$LOG"; then
    echo "QEMU DEVICE-PREP BOOT: PASS ($OUT)"
    exit 0
fi
echo "QEMU DEVICE-PREP BOOT: FAIL (qemu exit $status, log $LOG)" >&2
exit 1

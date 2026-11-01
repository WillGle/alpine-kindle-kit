#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Optional full-system ARM VM. Requires a self-supplied QEMU-bootable kernel
# and rootfs image under vm/images/ (ignored). This is not required for the
# mandatory host/user-mode gates.
set -eu
REPO=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
KERNEL=${PW3_QEMU_KERNEL:-"$REPO/vm/images/zImage"}
ROOTFS=${PW3_QEMU_ROOTFS:-"$REPO/vm/images/rootfs.ext3"}
QEMU=${QEMU_SYSTEM_ARM:-$(command -v qemu-system-arm || true)}
[ "$(uname -s)" = Linux ] || { echo "ERROR: Linux host required" >&2; exit 1; }
case "$(uname -m)" in x86_64|amd64) ;; *) echo "ERROR: x86-64/AMD64 host required" >&2; exit 1;; esac
[ -n "$QEMU" ] || { echo "ERROR: qemu-system-arm not found" >&2; exit 1; }
[ -f "$KERNEL" ] || { echo "ERROR: set PW3_QEMU_KERNEL (missing $KERNEL)" >&2; exit 1; }
[ -f "$ROOTFS" ] || { echo "ERROR: set PW3_QEMU_ROOTFS (missing $ROOTFS)" >&2; exit 1; }
exec "$QEMU" -M virt -m 512M -nographic \
    -kernel "$KERNEL" \
    -drive file="$ROOTFS",format=raw,if=virtio \
    -append "root=/dev/vda rw console=ttyAMA0 ${PW3_QEMU_APPEND:-}"

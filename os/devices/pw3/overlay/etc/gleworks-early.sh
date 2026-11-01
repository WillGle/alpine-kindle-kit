#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
bb=/bin/busybox
$bb mount -t proc proc /proc
echo /sbin/hotplug > /proc/sys/kernel/hotplug
$bb mount -t sysfs sysfs /sys
$bb mount -t tmpfs -o mode=755 tmpfs /dev
$bb mknod /dev/null c 1 3
$bb chmod 666 /dev/null
$bb mknod /dev/console c 5 1
$bb chmod 600 /dev/console
$bb mkdir -p /dev/pts /dev/shm /sys/kernel/debug
$bb mount -t devpts devpts /dev/pts
$bb mount -t tmpfs -o mode=1777 tmpfs /dev/shm
$bb mount -t tmpfs -o mode=755 tmpfs /run
$bb mount -t tmpfs -o mode=1777 tmpfs /tmp
$bb cp /gleworks/shutdown /run/gleworks-shutdown
$bb chmod 700 /run/gleworks-shutdown
trap 'rc=$?; if [ "$rc" != 0 ]; then echo GW_PERSISTENT_STARTUP_FAILED; /bin/busybox kill -QUIT 1; fi' EXIT
# Every startup retains its deadline until the console setup completes.
if $bb grep -q '^gleworks_pid1_reset ' /proc/modules; then
 minor=$($bb awk '$2=="gleworks_pid1_reset" {print $1}' /proc/misc)
 case "$minor" in ''|*[!0-9]*) exit 1;; esac
 $bb mknod /dev/gleworks_pid1_reset c 10 "$minor"
 /gleworks/guard-control status > /run/guardian.log
 $bb grep -q '^state=00000002$' /run/guardian.log
fi
module=/lib/modules/3.0.35-lab126/kernel/drivers/usb
$bb insmod "$module/otg/fsl_otg_arc.ko"
$bb insmod "$module/gadget/arcotg_udc.ko"
# The first bind establishes the already-present cable session on PW3.
$bb insmod /gleworks/g_file_storage.ko vendor=6473 product=4 release=256 removable=1 recovery_mode=1
[ -z "$($bb cat /sys/devices/platform/fsl-usb2-udc/gadget/gadget-lun0/file)" ]
$bb sleep 4
$bb rmmod g_file_storage
$bb insmod "$module/gadget/g_serial.ko" use_acm=1 n_ports=1
major=$($bb awk '$2=="ttyGS" {print $1}' /proc/devices)
case "$major" in ''|*[!0-9]*) exit 1;; esac
$bb mknod /dev/ttyGS0 c "$major" 0
$bb chmod 600 /dev/ttyGS0
$bb stty -F /dev/ttyGS0 raw -echo
{
 echo GLEWORKS_ALPINE_PERSISTENT_START
 $bb cat /etc/alpine-release /proc/sys/kernel/random/boot_id
 $bb readlink /proc/1/exe
 $bb cat /proc/mounts
 $bb ps
} > /run/persistent-boot.log
$bb sync
echo ready > /run/usb-ready
if [ "${GLEWORKS_BOOT_MODE:-trial}" = persistent ]; then
 [ "$($bb cat /boot/gleworks-control/gleworks-alpine/boot.pending)" = gleworks-alpine-v1 ]
 /gleworks/guard-control cancel > /run/guardian-cancel.log
 $bb grep -q '^state=00000000$' /run/guardian-cancel.log
 # Writes start only after the emergency reset deadline is gone.
 $bb mount -o remount,rw /
 # The proven shutdown helper leaves one handoff record, never a history.
 $bb rm -f /boot/gleworks-control/gleworks-alpine/shutdown.log
 # Successful guarded startup acknowledges the single offline-repair attempt.
 $bb rm -f /boot/gleworks-control/gleworks-alpine/boot.repairing
 # Retain boot.pending so the next startup can detect an unclean shutdown.
 echo persistent > /run/persistent-boot
 $bb sync
fi
# Tailscale (and any other early userspace crypto) needs the kernel random
# devices before it starts.  Do not rely on the later OpenRC start_pre hook to
# create them: that ordering caused tailscaled to panic on a fresh boot.
[ -e /dev/urandom ] || $bb mknod /dev/urandom c 1 9
[ -e /dev/random ] || $bb mknod /dev/random c 1 8
[ -e /dev/zero ] || $bb mknod /dev/zero c 1 5
$bb chmod 644 /dev/urandom /dev/random
$bb chmod 666 /dev/zero

# Start Tailscale userspace daemon if available (best-effort, non-fatal).
if [ -x /usr/sbin/tailscaled ]; then
  nohup /usr/sbin/tailscaled --tun=userspace-networking >/run/tailscaled.log 2>&1 &
fi

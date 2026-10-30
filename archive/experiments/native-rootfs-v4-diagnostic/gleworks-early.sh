#!/bin/sh
set -eux
exec >> /boot/gleworks-control/gleworks-alpine/coldboot.log 2>&1
echo GW_PHYSICAL_COLDSTART_NONCE=daa6482bb851cf5fc013e06adaaac0d9
/bin/busybox sync
snapshot() {
 bb=/bin/busybox
 echo GW_ALPINE_SNAPSHOT
 $bb cat /proc/uptime /proc/sys/kernel/random/boot_id /etc/alpine-release
 $bb readlink /proc/1/exe
 $bb readlink /proc/1/root
 /gleworks/guard-control status
 $bb cat /proc/modules /proc/mounts
 for path in /proc/asession /proc/driver/fsl_otg /sys/devices/platform/fsl-usb2-udc/gadget/connected; do
  [ ! -r "$path" ] || $bb cat "$path" || true
 done
 $bb ps
}
trap 'rc=$?; set +e; echo GW_SYSINIT_RESULT=$rc; snapshot; /bin/busybox sync' EXIT
set -eu
bb=/bin/busybox
$bb mount -t proc proc /proc
$bb mount -t sysfs sysfs /sys
$bb mount -t tmpfs -o mode=755 tmpfs /dev
$bb mknod /dev/null c 1 3
$bb chmod 666 /dev/null
$bb mknod /dev/console c 5 1
$bb chmod 600 /dev/console
$bb mkdir -p /dev/pts /dev/shm
$bb mount -t devpts devpts /dev/pts
$bb mount -t tmpfs -o mode=1777 tmpfs /dev/shm
$bb mount -t tmpfs -o mode=755 tmpfs /run
$bb mount -t tmpfs -o mode=1777 tmpfs /tmp
# Arm before USB setup: failure below still leaves the stock reset deadline.
if ! $bb grep -q '^gleworks_pid1_reset ' /proc/modules; then
    $bb insmod /gleworks/gleworks_pid1_reset.ko
fi
minor=$($bb awk '$2=="gleworks_pid1_reset" {print $1}' /proc/misc)
case "$minor" in ''|*[!0-9]*) exit 1;; esac
$bb mknod /dev/gleworks_pid1_reset c 10 "$minor"
$bb chmod 600 /dev/gleworks_pid1_reset
/gleworks/guard-control status > /run/guardian.log
if ! $bb grep -q '^state=00000002$' /run/guardian.log; then
    /gleworks/guard-control arm > /run/guardian.log
fi
snapshot
$bb sync
module=/lib/modules/3.0.35-lab126/kernel/drivers/usb
$bb insmod "$module/otg/fsl_otg_arc.ko"
$bb insmod "$module/gadget/arcotg_udc.ko"
$bb insmod "$module/gadget/g_serial.ko" use_acm=1 n_ports=1
major=$($bb awk '$2=="ttyGS" {print $1}' /proc/devices)
case "$major" in ''|*[!0-9]*) exit 1;; esac
$bb mknod /dev/ttyGS0 c "$major" 0
$bb chmod 600 /dev/ttyGS0
$bb stty -F /dev/ttyGS0 raw -echo
echo ready > /run/usb-ready
snapshot
/bin/busybox sync
/bin/busybox sh /etc/gleworks-usb-retry.sh >> /boot/gleworks-control/gleworks-alpine/coldboot.log 2>&1 &

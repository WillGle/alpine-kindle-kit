#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
# Set the recovery trap before any risky operation: a failure in mount, node
# creation or the shutdown copy must still return to the guardian instead of
# leaving a half-started boot.
trap 'rc=$?; if [ "$rc" != 0 ]; then echo GW_PERSISTENT_STARTUP_FAILED; /bin/busybox kill -QUIT 1; fi' EXIT
. /etc/gleworks-device-prepare.sh
mount_early_filesystems
echo /sbin/hotplug > /proc/sys/kernel/hotplug
create_core_devices
$bb cp /gleworks/shutdown /run/gleworks-shutdown
$bb chmod 700 /run/gleworks-shutdown
# Every startup retains its deadline until the console setup completes.
if $bb grep -q '^gleworks_pid1_reset ' /proc/modules; then
 minor=$($bb awk '$2=="gleworks_pid1_reset" {print $1}' /proc/misc)
 case "$minor" in ''|*[!0-9]*) exit 1;; esac
 $bb mknod /dev/gleworks_pid1_reset c 10 "$minor"
 /gleworks/guard-control status > /run/guardian.log
 $bb grep -q '^state=00000002$' /run/guardian.log
fi
load_usb_gadget
# The first bind establishes the already-present cable session on PW3.
$bb insmod /gleworks/g_file_storage.ko vendor=6473 product=4 release=256 removable=1 recovery_mode=1
[ -z "$($bb cat /sys/devices/platform/fsl-usb2-udc/gadget/gadget-lun0/file)" ]
$bb sleep 4
# A busy gadget must not reset-loop the whole boot.  Retry the switch, then
# continue without the USB console if the kernel still holds the old function.
i=0
while $bb grep -q '^g_file_storage ' /proc/modules && [ "$i" -lt 10 ]; do
    $bb rmmod g_file_storage 2>/dev/null || true
    i=$((i + 1))
    if $bb grep -q '^g_file_storage ' /proc/modules; then $bb sleep 1; fi
done
usb_console=0
if ! $bb grep -q '^g_file_storage ' /proc/modules; then
    if $bb insmod "$usb_module_dir/gadget/g_serial.ko" use_acm=1 n_ports=1; then
        major=$($bb awk '$2=="ttyGS" {print $1}' /proc/devices)
        case "$major" in
            ''|*[!0-9]*) ;;
            *)
                $bb mknod /dev/ttyGS0 c "$major" 0
                $bb chmod 600 /dev/ttyGS0
                $bb stty -F /dev/ttyGS0 raw -echo
                usb_console=1
                ;;
        esac
    fi
fi
[ "$usb_console" = 1 ] || echo GW_USB_CONSOLE_UNAVAILABLE >&2
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
# devices before it starts; create_core_devices already made them idempotently.

# Tailscale is not started here.  It is started on demand by
# /usr/sbin/gw-tailscale (on|off|status) so an idle device does not keep the
# userspace daemon and Wi-Fi radio awake.

# Lock-screen image importer.  Drop a PNG/JPEG into
# /mnt/us/gleworks/upload; it is converted to the panel's raw 8 bpp format
# and the source is deleted so the FAT partition never holds two copies.
if [ -x /usr/sbin/gw-lockscreen-import ] && [ -x /opt/koreader/luajit ] &&
   [ ! -e /run/gw-lockscreen-import.loop ]; then
    : > /run/gw-lockscreen-import.loop
    setsid sh -c 'while :; do /usr/sbin/gw-lockscreen-import; /bin/busybox sleep 10; done' >/dev/null 2>&1 &
fi

# Vault puller: fetch new files from the homelab HTTP server every five
# minutes.  With no url= in /mnt/us/gleworks/vault.conf it exits immediately.
if [ -x /usr/sbin/gw-vault-pull ] && [ ! -e /run/gw-vault-pull.loop ]; then
    : > /run/gw-vault-pull.loop
    setsid sh -c 'while :; do /usr/sbin/gw-vault-pull; /bin/busybox sleep 300; done' >/dev/null 2>&1 &
fi

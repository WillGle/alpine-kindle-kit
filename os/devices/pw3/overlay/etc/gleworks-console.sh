#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
bb=/bin/busybox
[ -f /run/usb-ready ] || { $bb sleep 2; exit 1; }
exec 3<>/dev/ttyGS0
while IFS= read -r command <&3; do
 case "$command" in
  STATUS)
   printf 'GLEWORKS_ALPINE_STATUS release=%s kernel=%s pid1=%s boot=%s\n' "$($bb cat /etc/alpine-release)" "$($bb uname -r)" "$($bb readlink /proc/1/exe)" "$($bb cat /proc/sys/kernel/random/boot_id)" >&3
  ;;
  SHELL)
   # Physical USB is the initial administrator console, never a network shell.
   export TERM=dumb PATH=/sbin:/usr/sbin:/bin:/usr/bin
   # Keep command history off the ext3 root and bound it. The runtime tmpfs is
   # cleared on reboot; losing old console history is preferable to unbounded
   # flash writes on a nearly full system image.
   export HISTFILE=/run/gw-console-history
   export HISTSIZE=200
   exec $bb sh -i <&3 >&3 2>&3
  ;;
  REBOOT) $bb kill -QUIT 1; $bb sleep 30;;
  RESCUE)
   $bb rm -f /boot/gleworks-control/gleworks-alpine/boot.enabled
   $bb sync
   $bb kill -QUIT 1
   $bb sleep 30
  ;;
  *)
   [ "${#command}" = 32 ] || continue
   case "$command" in *[!0-9a-f]*) continue;; esac
   [ "$($bb readlink /proc/1/exe)" = /bin/busybox ] || continue
   # Echo only: this is not authentication and grants no privilege.
   printf 'GLEWORKS_ALPINE_CONSOLE_ECHO release=3.24.3 kernel=%s pid1=/bin/busybox nonce=%s\n' "$($bb uname -r)" "$command" >&3
  ;;
 esac
done

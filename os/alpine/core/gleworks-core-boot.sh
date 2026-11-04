#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# QEMU profile: hardware initialization remains in the PW3 boot path.
set -eu
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts /dev/shm /run /tmp
mount -t devpts devpts /dev/pts
mount -t tmpfs -o mode=0755 tmpfs /run
mount -t tmpfs -o mode=1777 tmpfs /tmp
mkdir -p /var/lib/gleworks/apps /var/lib/gleworks/appdata
hostname alpine-reader-qemu
case " $(cat /proc/cmdline) " in
    *" gleworks.recovery=1 "*) touch /run/core-recovery ;;
esac
touch /run/core-boot-ready
echo GW_CORE_BOOT_READY

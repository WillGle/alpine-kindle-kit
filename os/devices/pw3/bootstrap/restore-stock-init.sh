#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Restore the exact wrapper atomically while the stock OS is running.
set -eu
original=ad624cbad3090bf15e65c8420a4e451ee10fd067a40b1bfe662965e82d932d0a
[ "$(id -u)" = 0 ]
[ "$(readlink /proc/1/exe)" = /sbin/init.exe ]
[ -f /sbin/init.gleworks-stock ] && [ ! -L /sbin/init.gleworks-stock ]
[ ! -e /sbin/init.gleworks-restore ] && [ ! -L /sbin/init.gleworks-restore ]
[ "$(sha256sum /sbin/init.gleworks-stock | awk '{print $1}')" = "$original" ]
cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    /bin/sync
    /bin/mount -o remount,ro / || exit 1
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
/bin/mount -o remount,rw /
cp -p /sbin/init.gleworks-stock /sbin/init.gleworks-restore
/bin/sync
[ "$(sha256sum /sbin/init.gleworks-restore | awk '{print $1}')" = "$original" ]
/bin/mv /sbin/init.gleworks-restore /sbin/init
/bin/sync
[ "$(sha256sum /sbin/init | awk '{print $1}')" = "$original" ]
rm -f /etc/upstart/gleworks-coldboot-control.conf /var/local/gleworks-coldboot-control.once
echo GW_STOCK_INIT_RESTORED

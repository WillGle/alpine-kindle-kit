#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Firmware requests needed by the retained PW3 Wi-Fi driver.
set -u
[ "${ACTION:-}" = add ] && [ "${SUBSYSTEM:-}" = firmware ] || exit 0
case "${DEVPATH:-}" in /devices/*) ;; *) exit 1;; esac
case "$DEVPATH" in *..*|*[!a-zA-Z0-9_./:!+,-]*) exit 1;; esac
loading=/sys$DEVPATH/loading
data=/sys$DEVPATH/data
case "${FIRMWARE:-}" in
    /opt/ar6k/target/AR6003/hw2.1.1/bin/athwlan.bin|\
    /opt/ar6k/target/AR6003/hw2.1.1/bin/otp.bin|\
    /opt/ar6k/target/AR6003/hw2.1.1/bin/bdata.bin|\
    /opt/ar6k/target/AR6003/hw2.1.1/bin/bdata.SD31.bin|\
    /opt/ar6k/target/AR6003/hw2.1.1/bin/AR6803_default.bin|\
    /opt/ar6k/target/AR6003/hw2.1.1/bin/fw-4.bin|\
    /opt/ar6k/target/AR6003/hw2.1.1/bin/data.patch.bin)
        source=$FIRMWARE ;;
    *) [ ! -e "$loading" ] || printf '%s\n' -1 | /bin/busybox dd of="$loading" 2>/dev/null; exit 1 ;;
esac
[ -f "$source" ] && [ ! -L "$source" ] || { printf '%s\n' -1 | /bin/busybox dd of="$loading" 2>/dev/null; exit 1; }
printf '%s\n' 1 | /bin/busybox dd of="$loading" 2>/dev/null || exit 1
if /bin/busybox dd if="$source" of="$data" bs=4096 2>/dev/null; then
    printf '%s\n' 0 | /bin/busybox dd of="$loading" 2>/dev/null
else
    printf '%s\n' -1 | /bin/busybox dd of="$loading" 2>/dev/null
    exit 1
fi

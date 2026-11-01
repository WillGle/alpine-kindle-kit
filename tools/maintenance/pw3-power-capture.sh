#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Device-side power acceptance capture for the physical button run.
#
# It runs independently of the USB serial shell, so it keeps sampling across
# cable removal and kernel mem suspend.  Start it before unplugging; after the
# manual press/wake/unlock and replug, read the log over the USB console.
#
#   sh pw3-power-capture.sh start
#   sh pw3-power-capture.sh stop
#   sh pw3-power-capture.sh dump
#   sh pw3-power-capture.sh clear
#
# Log: /var/lib/gleworks/power-capture.log (persistent root filesystem)
set -u
LOG=/var/lib/gleworks/power-capture.log
STOP=/run/gw-power-capture.stop
BOOT=/proc/sys/kernel/random/boot_id

case "${1:-}" in
start)
    mkdir -p /var/lib/gleworks
    rm -f "$STOP"
    : > "$LOG"
    printf 'START %s boot=%s\n' "$(date +%s)" "$(cat "$BOOT" 2>/dev/null)" >> "$LOG"
    while [ ! -f "$STOP" ]; do
        ts=$(date +%s)
        st=$(gwctl status 2>&1)
        scr=$(grep -E 'GW_SHELL: (event|lock screen|startup/status)|GW_UI_DIAG: screen' \
              /run/gw-system.log 2>/dev/null | tail -2 | tr '\n' ';')
        pm=$(dmesg 2>/dev/null | grep -E 'PM: (Entering mem sleep|Finishing wakeup)|max77696_rtc_resume' | tail -1)
        printf '%s | %s | %s | %s\n' "$ts" "$st" "$scr" "$pm" >> "$LOG"
        sleep 1
    done
    printf 'STOP %s\n' "$(date +%s)" >> "$LOG"
    ;;
stop)
    touch "$STOP"
    ;;
dump)
    cat "$LOG" 2>/dev/null || true
    ;;
clear)
    rm -f "$LOG" "$STOP"
    ;;
*)
    echo "usage: $0 start|dump|clear" >&2
    exit 2
    ;;
esac

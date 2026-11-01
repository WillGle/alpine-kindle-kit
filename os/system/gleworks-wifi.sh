#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Controlled PW3 Wi-Fi backend. No credentials appear in arguments or logs.
set -eu
bb=/bin/busybox
prepare_radio() {
    [ -x /sbin/hotplug ] && [ -x /sbin/wpa_supplicant ]
    base=/lib/modules/3.0.35-lab126/kernel/compat-wireless
    for item in net/wireless/cfg80211 drivers/net/wireless/ath/ath drivers/net/wireless/ath/ath6kl/ath6kl_sdio; do
        name=${item##*/}
        if ! $bb grep -q "^$name " /proc/modules; then $bb insmod "$base/$item.ko"; fi
    done
    i=0
    while [ ! -d /sys/class/net/wlan0 ]; do
        i=$((i + 1)); [ "$i" -le 60 ] || exit 1
        $bb sleep 1
    done
    $bb mkdir -p /run/wpa_supplicant
    $bb ifconfig wlan0 up
}
stop_dhcp() {
    # The DHCP client must stay alive to renew the lease.  Stop an old one
    # before starting a new client so reconnects cannot leave duplicates.
    [ -f /run/gw-udhcpc.pid ] || return 0
    pid=$($bb cat /run/gw-udhcpc.pid)
    case "$pid" in ''|*[!0-9]*) $bb rm -f /run/gw-udhcpc.pid; return 0;; esac
    if [ -r "/proc/$pid/cmdline" ] && $bb tr '\000' ' ' < "/proc/$pid/cmdline" | $bb grep -q 'udhcpc.*-i wlan0'; then
        $bb kill "$pid" 2>/dev/null || :
    fi
    $bb rm -f /run/gw-udhcpc.pid
}
case "${1:-}" in
start)
    [ -f /var/lib/gleworks/wifi.conf ] && [ ! -L /var/lib/gleworks/wifi.conf ]
    prepare_radio
    if [ -S /run/wpa_supplicant/wlan0 ]; then /sbin/wpa_cli -i wlan0 terminate >/dev/null 2>&1 || :; fi
    stop_dhcp
    /sbin/wpa_supplicant -B -D nl80211,wext -i wlan0 -c /var/lib/gleworks/wifi.conf -P /run/gw-wpa.pid >/dev/null 2>&1
    # Keep udhcpc running so it can renew the DHCP lease and re-acquire an
    # address after a link reconnect.  -q would exit immediately after the
    # first lease and leave the device with an unrenewable address.
    $bb udhcpc -b -i wlan0 -p /run/gw-udhcpc.pid -s /usr/share/udhcpc/default.script >/dev/null 2>&1 &
    ;;
scan)
    prepare_radio
    if [ ! -S /run/wpa_supplicant/wlan0 ]; then
        umask 077
        printf 'ctrl_interface=/run/wpa_supplicant\nupdate_config=0\n' > /run/gw-wifi-scan.conf
        /sbin/wpa_supplicant -B -D nl80211,wext -i wlan0 -c /run/gw-wifi-scan.conf -P /run/gw-wpa.pid >/dev/null 2>&1
    fi
    /sbin/wpa_cli -i wlan0 scan >/dev/null 2>&1
    $bb sleep 5
    /sbin/wpa_cli -i wlan0 scan_results > /run/gw-wifi-scan.tmp
    $bb mv /run/gw-wifi-scan.tmp /run/gw-wifi-scan
    ;;
stop)
    if [ -S /run/wpa_supplicant/wlan0 ]; then /sbin/wpa_cli -i wlan0 terminate >/dev/null 2>&1 || :; fi
    stop_dhcp
    if [ -d /sys/class/net/wlan0 ]; then $bb ifconfig wlan0 down || :; fi
    ;;
*) exit 2;;
esac

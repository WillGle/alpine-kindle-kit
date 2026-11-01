#!/bin/sh
# Reproducible PW3 Wi-Fi lifecycle checks. Safe by default: read-only status,
# scan, DNS and TCP reachability. --reconnect drops and restores the current
# association. --bad-credentials uses a temporary, non-existent SSID; it never
# touches /var/lib/gleworks/wifi.conf.
set -eu
bb=/bin/busybox
mode=${1:-status}
case "$mode" in status|--reconnect|--bad-credentials) ;; *) echo "usage: $0 [--reconnect|--bad-credentials]" >&2; exit 2;; esac

wpa_state() { /sbin/wpa_cli -i wlan0 status 2>/dev/null | $bb awk -F= '/^wpa_state=/{print $2}'; }
wait_state() {
    i=0
    while [ "$i" -lt 30 ]; do
        [ "$(wpa_state)" = COMPLETED ] && return 0
        i=$((i+1)); sleep 1
    done
    return 1
}
addr() { ifconfig wlan0 2>/dev/null | $bb awk '/inet addr/{print $2}' | $bb cut -d: -f2; }

echo "state=$(wpa_state) ip=$(addr || true)"
/sbin/wpa_cli -i wlan0 status 2>/dev/null | $bb grep -E '^(wpa_state|ip_address)=' || true
echo "scan_begin"
/etc/gleworks-wifi.sh scan
$bb head -6 /run/gw-wifi-scan 2>/dev/null | sed 's/\t/ | /g'
echo "dns_begin"
nslookup example.com 2>&1 | $bb grep -A2 'Name:' | $bb head -4 || true
echo "dhcp_proc=$(pidof udhcpc || echo none) wpa_proc=$(pidof wpa_supplicant || echo none)"

if [ "$mode" = --reconnect ]; then
    echo "disconnect"
    /sbin/wpa_cli -i wlan0 disconnect
    sleep 8
    echo "outage_state=$(wpa_state) ip=$(addr || true)"
    ping -c 1 -W 2 1.1.1.1 >/dev/null 2>&1 && echo "outage_ping=unexpected-success" || echo "outage_ping=failed-as-expected"
    echo "reconnect"
    /sbin/wpa_cli -i wlan0 reconnect
    wait_state
    sleep 5
    echo "restored_state=$(wpa_state) ip=$(addr || true)"
    ping -c 2 -W 2 1.1.1.1 2>&1 | $bb tail -3 || true
    if [ -x /usr/bin/tailscale ]; then
        echo "tailscale_status"
        /usr/bin/tailscale status 2>&1 | $bb head -3 || true
        echo "tailscale_ping"
        /usr/bin/tailscale ping -c 1 pve-mini 2>&1 | $bb head -3 || true
    fi
fi

if [ "$mode" = --bad-credentials ]; then
    echo "bad_credentials_test"
    /etc/gleworks-wifi.sh stop
    sleep 2
    ifconfig wlan0 up
    ifconfig wlan0 0.0.0.0 2>/dev/null || true
    umask 077
    printf 'ctrl_interface=/run/wpa_supplicant\nupdate_config=0\nnetwork={\n ssid="P2-NOT-A-REAL-AP"\n key_mgmt=NONE\n}\n' > /run/gw-wifi-bad.conf
    /sbin/wpa_supplicant -B -D nl80211,wext -i wlan0 -c /run/gw-wifi-bad.conf -P /run/gw-wpa-bad.pid >/dev/null 2>&1
    sleep 20
    echo "bad_state=$(wpa_state) ip=$(addr || echo none)"
    /sbin/wpa_cli -i wlan0 terminate >/dev/null 2>&1 || true
    rm -f /run/gw-wifi-bad.conf
    sleep 2
    echo "restore"
    /etc/gleworks-wifi.sh start
    wait_state
    i=0
    while [ "$i" -lt 30 ]; do [ -n "$(addr)" ] && break; i=$((i+1)); sleep 1; done
    echo "restored_state=$(wpa_state) ip=$(addr || true) dhcp_proc=$(pidof udhcpc || echo none)"
fi

---
status: current
updated: 2026-10-31
verified_build: none
supersedes: none
audience: maintainer
---

# PW3 — saved Wi-Fi and reconnect after reboot, 2026-10-31

Status: saved credentials were reused automatically after a clean reboot; the
service also supervises link loss with bounded backoff. Verified on the
reference PW3.

## Before reboot

| Item | Value |
| --- | --- |
| Boot ID | `9b2a013b-e04b-488f-81ef-2154d3fa7d46` |
| `wifi-enabled.conf` | `enabled=1` |
| Connection | `wpa_state=COMPLETED`, `ssid=Will`, `ip_address=192.168.1.200` |
| Service state | `net=connected`, `wifi-state=CONNECTED` |

The saved supplicant configuration is root-only (`0600`) and is stored outside
the USB partition:

```text
/var/lib/gleworks/wifi.conf        100 bytes, mode 0600
/var/lib/gleworks/wifi-enabled.conf 10 bytes, mode 0600
```

The SSID is written as a hex string because wpa_supplicant accepts both quoted
and hex SSIDs; the device reads `ssid=Will`, so the encoding is correct.

## Clean reboot

```text
$ python3 tools/maintenance/kindle-linux.py reboot
Clean reboot verified: 9b2a013b-e04b-488f-81ef-2154d3fa7d46 -> fd727e9a-cfe4-4f60-a2d4-98f530232789
```

## After reboot (no manual action)

```text
$ gwctl status
STATUS version=0.1-stub boot=fd727e9a-cfe4-4f60-a2d4-98f530232789 \
  power=battery battery=99 light=1 foreground=none net=connected \
  usb=host-class ... wifi-state=CONNECTED ... display=on

$ wpa_cli -i wlan0 status
ssid=Will
wpa_state=COMPLETED
ip_address=192.168.1.200
```

Service log on the new boot:

```text
GW_SHELL: STATUS ... net=disconnected ... wifi-state=CONNECTING ...
GW_SYSTEM: Wi-Fi reconnect attempt
GW_SYSTEM: Wi-Fi link up
```

## Supervision behaviour

- On boot, an existing link is adopted without restarting supplicant/DHCP.
- Otherwise, `wifi-enabled=1` plus a saved `wifi.conf` triggers a fixed `start`
  helper launch.
- While enabled, the service samples the link every 5 s; a drop moves the state
  to `FAILED`, broadcasts `NET DOWN`, and retries with 5 → 10 → 20 → 40 → 60 s
  backoff.
- Manual **Disconnect** stops the link and sets `enabled=0` (credentials are
  retained); **Connect** and **Enable** turn supervision back on.
- **Forget** removes `wifi.conf`; no credentials appear in arguments or logs.

## Remaining

A deliberate AP power-cycle / out-of-range walk is still useful to confirm the
backoff timing on RF, but the reboot path and the link-supervision code path are
verified above.

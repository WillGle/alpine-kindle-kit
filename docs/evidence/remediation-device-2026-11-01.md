---
status: current
updated: 2026-11-01
verified_build: none
supersedes: none
audience: maintainer
---

# Code-review remediation — device evidence (2026-11-01/02)

Host: x86-64 Linux. Device: PW3, kernel `3.0.35-lab126`, Alpine 3.24.3,
physical USB console `0525:a4a7` on `/dev/ttyACM0`.

Deployed binaries were built from this tree with
`GW_ENABLE_HW_TRIAL=1` (trial, not accepted):

| Binary | sha256 |
| --- | --- |
| gw-system | `8c545dd863adff8f6f607783fe551351a8f4622251a6364076af1000a1f867e1` |
| gwctl | `53f4b86bf73fc90ea86571e7bf09523178a5756b418e833b16267289e5a44c6c` |
| gw-appctl | `5cf590cd7147013262932dfb05699b7fdcda9150ceca080e71469b00dc0cb998` |

`gw-shell` was **not** re-verified on the device for the original A–E run; the
device continued to run the previously accepted binary (`80160c3a…`). The UI
runtime changes in A–E — translation-unit split, network row identity, selected
package install, batch/persistent IPC client, and EPDC/paint cleanup — were
host-tested only. A later trial build (`06027ba9…`, built with
`GW_ENABLE_HW_TRIAL=1`) exists, but new-UI device acceptance is still pending.
Until that acceptance is recorded, B23 means “A–E code complete; new UI device
verification pending”, not full device completion.

## C1 — command socket permissions

```text
root:   /usr/sbin/gwctl status         rc=0
nobody: su -s /bin/sh nobody -c ...    rc=1
srw------- root root /run/gw-system.cmd
srw------- root root /run/gw-system.evt
```

## A4 — selected USB package

Two signed packages (`remediation-demo` 1.0.0 and 1.1.0) were installed from
`/mnt/us/gleworks/apps` through the service command socket:

```text
CMD APP INSTALL USB 0    RESP OK 1 skipped=0
CMD APP INFO 0           RESP APP remediation-demo 1.0.0 ...
CMD APP INSTALL USB ALL  RESP OK 1 skipped=1
CMD APP INFO 0           RESP APP remediation-demo 1.1.0 ...
```

The selected older package installed only itself; ALL selected the newest.
Test packages and the installed app were removed afterwards.

## A2 — Wi-Fi

WPA2 (`Will`) connected and obtained DHCP:

```text
ssid=Will  pairwise_cipher=CCMP  wpa_state=COMPLETED
wlan0 inet 192.168.1.200/24
STATUS net=connected wifi-state=CONNECTED
```

Open network (`GW-Open`, host AP): association succeeded with
`key_mgmt=NONE`; a temporary privileged DHCP server and the corresponding
NixOS firewall accept rule (both removed afterwards) gave a real lease:

```text
udhcpc: lease of 10.42.0.217 obtained from 10.42.0.1
wlan0 inet 10.42.0.217/24
STATUS net=connected wifi-state=CONNECTED
```

Generated config was:

```text
network={
    ssid=47572d4f70656e
    key_mgmt=NONE
}
```

and mode was `0600`. Static link to the AP also worked (`ping 10.42.0.1`, 0%
loss). The NixOS firewall was dropping inbound DHCP until a temporary accept
rule was added for `wlp4s0` UDP/67; the rule and DHCP container were removed
after the lease was captured.

## A5 — uinput power event

A 16-byte `struct input_event` helper created `/dev/input/event2`
(`gw-test-power`, bytes `13:66`) and injected a power press:

```text
press event_size=16
STATUS ... lock=locked ... display=off
```

The service locked the panel on the injected short press.

## D1 — reader touch discovery with an extra input device

While `event2` (`gw-test-power`) existed:

```text
event0: max77696-onkey
event1: cyttsp4_mt
event2: gw-test-power
find_touch_node -> /dev/input/event1
reader /proc/<pid>/fd/5 -> /dev/input/event1
```

## D2 — reader lock polling

Reader was started through `/etc/gleworks-reader-service.sh`; one
`gwctl watch` helper ran for the reader lifetime.

```text
40 samples over 20 s: gwctl pids: 26586 (single PID), max_concurrent=1
lock → /run/gw-lock-state=locked, reader exited in 2 s
unlock → /run/gw-lock-state=unlocked
```

`GW_MANAGED_READER=1` was missing from the display launcher, so the plugin was
disabled until it was added; the display launcher also now refuses to start a
second watcher.

## C3 — guardian and shutdown

Guardian module was rebuilt with the device-paired token and tested live:

```text
status   state=00000000
test     state=00000001 remaining=2
wait 3s  state=00000004
arm      state=00000002 remaining=90
cancel   state=00000000
```

This exercises the post-test ARM path that was previously EBUSY. The module
source now permits ARM from state 4 after the test pin is released.

Shutdown helper was replaced in `/run/gleworks-shutdown` and rebooted twice:

| Case | boot ID before | boot ID after |
| --- | --- | --- |
| `boot.pending` present | `51d76510-…` | `bd729874-…` |
| `boot.pending` absent | `bd729874-…` | `bf8e4aed-…` |

Both boots returned to native Alpine with a clean ext3 root and the USB console
available. `boot.pending` was recreated by the normal persistent boot.

## Remaining scope

The service-side, IPC-permission, Wi-Fi, input, guardian, shutdown, and reader
device tests above pass. New-UI device verification is still pending: deploy the
current `gw-shell` trial build and repeat Apps (0/1/8/32 packages), Network
(0/1/16 APs), selected-Wi-Fi connect, lock/unlock reader handoff, and
page-snap/four-band plus EPDC checks. Host coverage for the same code paths
(including a 32-maximum-length app list and a multi-line IPC response that does
not wait for the socket timeout) is in `make check`.

D3 per-app isolation is deferred to the separate app-runtime project as stated
in the remediation plan. The guardian early-boot failure path was not perturbed
on the live device; its bounded halt path was exercised in QEMU:
`GW_BOOTSTRAP_HALT early mounts errno=2` → `reboot: Restarting system`.

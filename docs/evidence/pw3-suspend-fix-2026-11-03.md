---
status: current
updated: 2026-11-03
verified_build: 36aec9fe350e6bd82c416bef36428cdd61d72a379748a679f4b8243025418bb5
supersedes: none
audience: maintainer
---

# Battery drain RCA and suspend policy fix (2026-11-03)

Status: **fix deployed; battery verification pending.** The device now has the
USB-presence suspend policy. The one-hour on-battery measurement has not been
recorded yet.

## Symptom

- PW3 idle, screen locked, Wi-Fi connected, **not plugged in**.
- Battery dropped about **40% in 5 hours** (≈8%/h).
- Expected for this state: about **1–2%/h**.

## Evidence

From `/run/gw-system.log` after the cable was removed:

```text
GW_SYSTEM: USB state 2 -> 0
GW_SYSTEM: suspend scheduled in 3000 ms (lock=1 display=0)
GW_SYSTEM: suspend cancelled
GW_SYSTEM: suspend skipped (display=0 usb=0 charging=1)
```

There is **no** `GW_SYSTEM: entering mem suspend` and no kernel
`PM: suspend entry` line. The device never entered mem suspend.

`/proc/asession` reported no cable (`usb=0`), while the PMIC still reported
`POWER_SUPPLY_ONLINE=1` / charging. The stale charger bit won the policy check.

## Root cause

`os/system/gw-power-policy.c` checked `charging != 0` before looking at the USB
state:

```c
if (state->charging != 0) return 0;   /* stale bit blocks suspend */
if (state->usb == GW_USB_NONE) return 1;
```

The comment in the same file already said to ignore the charger bit when
`/proc/asession` proves there is no cable. Code and comment disagreed.

While awake, Wi-Fi, `tailscaled --tun=userspace-networking`, `gw-shell`,
`gw-system`, `wpa_supplicant` and `udhcpc` all kept running. Tailscale was a
contributor, not the primary cause.

## Fix

`suspend_power_ok()` now trusts cable presence only:

```c
return state->usb == GW_USB_NONE;
```

- No cable: allow suspend, ignore stale `charging`.
- Cable present or unknown: keep the device awake and the console available.

Regression tests updated in `os/system/test-power-policy.c`:

- `usb=NONE, charging=1` and `charging=-1` may suspend.
- `usb=POWER/HOST/UNKNOWN` may not suspend.

Commit: `bb9614e fix(system): allow suspend on battery when charger bit is stale`.

## Deployment

| Item | Value |
| --- | --- |
| Boot ID | `21147088-d697-4632-9d95-523b6827a997` |
| `gw-system` | `36aec9fe350e6bd82c416bef36428cdd61d72a379748a679f4b8243025418bb5` |
| `gw-shell` | `ebb637a4bdcd419b6bdd2fc61c21b99c9f700faeaaa8fd66a55e39989946ff98` |
| Rollback system | `/usr/sbin/gw-system.before-suspendfix-20261103` = `8c545dd8…` |
| Host gate | `make check CC=cc` **PASS** |
| ARM build | **PASS** |
| Lock/unlock after deploy | **PASS** |

## Pending verification

1. Unplug USB, lock the device, leave Wi-Fi connected.
2. Wait one hour; replug USB to wake and read:
   - `/run/gw-system.log` must contain `entering mem suspend`;
   - `dmesg` must contain a suspend/resume pair;
   - battery delta must be about 1–2%.
3. If it still drains, inspect wakeup sources, Wi-Fi power save and
   `tailscaled` before blaming the battery.

## Follow-up

Tailscale on-demand remains the next active-use power improvement. It is not a
substitute for fixing suspend.

---
status: current
updated: 2026-11-03
verified_build: 608eed8d8ab9b4c003fa70090cb6a72c49d62ccfdb09550b8ba6105d077823c0
supersedes: none
audience: maintainer
---

# Battery drain RCA and suspend policy fix (2026-11-03)

Status: **fix deployed and verified.** One hour on battery, locked with Wi-Fi
connected and no USB, dropped 1 percentage point (≈1%/h). The device entered
mem suspend five times with no `suspend skipped` line.

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
| Boot ID | `9b572fb8-3a31-4e3a-b25e-6739bbe1af4e` |
| `gw-system` | `608eed8d8ab9b4c003fa70090cb6a72c49d62ccfdb09550b8ba6105d077823c0` |
| `gw-shell` | `ebb637a4bdcd419b6bdd2fc61c21b99c9f700faeaaa8fd66a55e39989946ff98` |
| Rollback system | `/usr/sbin/gw-system.before-logtime-20261103` = `36aec9fe…` |
| Earlier rollback | `/usr/sbin/gw-system.before-suspendfix-20261103` = `8c545dd8…` |
| Host gate | `make check CC=cc` **PASS** |
| ARM build | **PASS** |
| Lock/unlock after deploy | **PASS** |

## Wall-clock logging

`gw-system` now prefixes display and suspend events with wall time:

```text
GW_SYSTEM: display off wall=2026-11-03 07:49:13 (saved light=4)
GW_SYSTEM: display on wall=2026-11-03 07:49:15 (light=4 lock=locked)
```

Suspend/resume additionally log the elapsed `CLOCK_BOOTTIME` duration, which
includes suspend time:

```text
GW_SYSTEM: entering mem suspend wall=... (usb=0 charging=0)
GW_SYSTEM: resumed without button wall=... slept=464.2s
```

`CLOCK_MONOTONIC` and `dmesg` timestamps stop during suspend, so they cannot be
used to measure an unplugged interval. The wall stamp and `slept` field are the
authoritative record. The display timestamps are verified on the device; the
first unplug/replug cycle still needs to record a real `slept` value.

## Battery verification (1 hour)

| Item | Result |
| --- | --- |
| State | locked, display off, Wi-Fi connected, no USB |
| Duration | 1 hour |
| Battery | 99% → 98% (≈1%/h) |
| `entering mem suspend` | 5 |
| `resumed without button` | 5 |
| `suspend skipped` | 0 |
| Kernel | suspend/resume pairs present in `dmesg` |

The previous behaviour was ≈8%/h with the device never suspending. The
USB-presence policy is the difference.

The device still wakes every few minutes from an RTC/wakeup source and stays
awake for the 30-second retry window. That is a further optimisation target,
not a regression of this fix.

## Remaining optimisation

The device still wakes every few minutes from an RTC/wakeup source and stays
awake for the 30-second retry window. Further work:

- identify the wake source (RTC, Wi-Fi or USB PHY);
- shorten the 30-second retry window if the wake is spurious;
- move Tailscale to on-demand for active use.

## Follow-up

Tailscale on-demand remains the next active-use power improvement. It is not a
substitute for fixing suspend.

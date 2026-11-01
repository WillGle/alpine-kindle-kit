# PW3 — physical power-button run, 2026-10-31

Status: physical button run **confirmed** on 2026-10-31. The lock screen was
shown, the device entered `mem` suspend, and the next physical press woke it
with the light restored and the lock screen still active. Unlock returned to
Home. The second run below is the accepted one; the earlier run that exposed the
stale charger flag is retained as the RCA.

## Run 1 (user, physical button)

Capture was running on the device and survived cable removal. Relevant samples
from `/var/lib/gleworks/power-capture.log` (epoch | STATUS | shell | dmesg):

```text
1793477484 | ... usb=none ... lock=unlocked display=on
1793477485 | ... GW_SHELL: event USB DISCONNECTED
1793477489 | ... power=charging battery=99 light=0 ... usb=none
             lock=locked display=off
           | GW_SHELL: event POWER LOCK; GW_SHELL: lock screen engaged
1793477495 | ... light=9 ... usb=none lock=locked display=on
           | GW_SHELL: event POWER BUTTON SHORT; POWER WAKE
1793477498 | ... lock=unlocked display=on
           | GW_SHELL: event POWER WAKE; POWER UNLOCK
1793477501 | ... usb=host-class ...   (cable reinserted)
```

So the physical press did show the lock screen and turn the frontlight off, and
the second press woke the light while keeping the lock. Unlock returned to
Home.

No suspension occurred. `dmesg` had no new `PM: Entering mem sleep` between
these samples, and STATUS kept reporting `power=charging` even after the cable
was gone.

## Root cause

`gw_power_should_suspend()` required both `usb == none` **and**
`charging == 0`. The PMIC `max77696-charger/online` node can stay `1` for
several seconds after unplug; that stale value blocked suspend. The user pressed
the wake button before the flag cleared.

Fix:

- A proven `usb=none` from `/proc/asession` is authoritative; suspend no longer
  waits for the lagging charger bit.
- `STATUS` reports `power=battery` when `usb=none`, even if the PMIC bit still
  lags.
- Cable-present and unknown-cable states still never suspend.
- Host policy tests updated for the stale-charging case.

## Post-fix verification (same kernel, simulated absent cable)

The service was started with the diagnostic `GW_POWER_USB_OVERRIDE=0` while the
charger still reported online, and the uinput `KEY_POWER` path was used:

```text
GW_SYSTEM: entering mem suspend (usb=0 charging=1)
GW_SHELL: event POWER SUSPEND
[13364.154339] PM: Entering mem sleep
[13364.682444] max77696-rtc ... resume_time1793477705
[13365.237693] PM: Finishing wakeup.
GW_SHELL: event POWER RESUME
GW_SYSTEM: resumed without button (RTC/wakeup source)
```

This is the exact old failure condition (`usb=0`, `charging=1`) and it now
suspends. Host tests:

```text
Stage 3 tests: PASS
Power policy tests: PASS
Power I/O tests: PASS
App manager tests: PASS
```

## Run 2 (user, physical button, after the gate fix)

Capture (full log kept at `pw3-power-physical-button-2026-10-31.log`):

```text
1793477812 | power=battery ... usb=none        # cable removed
1793477818 | light=0 ... lock=locked display=off
1793477819 | light=0 ... lock=locked display=off
1793477820 | ... (capture blocked inside STATUS)
1793477896 | light=9 ... lock=locked display=on
           | GW_SHELL: event POWER RESUME; GW_SHELL: event POWER WAKE
           | [13481.858506] PM: Finishing wakeup.
1793477902 | ... lock=unlocked display=on
1793477905 | ... usb=host-class             # cable reinserted
```

The capture process froze between 7819 and 7896; the STATUS command that was
in flight returned with `time=1793477895`, i.e. 76 seconds of suspended time.
On resume the service emitted `POWER RESUME` and `POWER WAKE`, restored the
frontlight to 9 and kept `lock=locked`; the next samples show Unlock
(`lock=unlocked`) and cable reinsertion. This is the full physical sequence:
lock screen → real kernel suspend → button wake → Unlock.

The user then reset the device to recover USB enumeration (known hotplug
issue); the current boot is `186d6571-e619-460e-b1f9-b3fcacb696a5`. Because of
the reset, the old kernel ring buffer is gone, but the persistent capture log
retains the `PM: Finishing wakeup.` record and the suspend-time gap.

---
status: current
updated: 2026-10-31
verified_build: none
supersedes: none
audience: maintainer
---

# PW3 — real suspend/wake acceptance, 2026-10-31

Status: kernel `mem` suspend, RTC wake, cable/charging gate and the full
power-button event path were exercised on the reference PW3. The physical
membrane button was not pressed by a person in this run; a root-only uinput
`KEY_POWER` device drove the same evdev/poll path, and the real RTC alarm woke
the device. Recorded so a later manual press can confirm the membrane/PMIC
layer independently.

## Device and boot

| Item | Value |
| --- | --- |
| Model | Paperwhite 3 `G090KB`, Alpine 3.24.3 |
| Kernel | `3.0.35-lab126`, bootloader unchanged |
| Boot ID | `fd727e9a-cfe4-4f60-a2d4-98f530232789` |
| Service | `/usr/sbin/gw-system --supervise` |
| Build flags | `GW_LIFECYCLE_SPIKE=1`, `GW_WIFI_SPIKE=1`, `GW_POWER_LIGHT_ACCEPTED=1`, `GW_POWER_SUSPEND_ACCEPTED=1` |

## Host tests (no device)

```text
$ make -C os/system CC=gcc test
Stage 3 tests: PASS
Power policy tests: PASS
Power I/O tests: PASS
```

`test-power-policy.c` covers short-press lock/wake/suspend decisions, the
cable/charging gate and unknown-state refusal. `test-power-io.c` covers the
RTC alarm write/readback/clear, the `wakeup_count` handshake before writing
`mem`, and `gw_power_drain_button` with real `input_event` records.

## Real mem suspend + RTC wake

Command path: `gwctl suspend-test` (root, supervised service) forces the
display off, arms a 10-second RTC fallback, writes `mem`, and resumes on the
alarm. Kernel log from the device:

```text
PM: Syncing filesystems ... done.
PM: Preparing system for mem sleep
PM: Entering mem sleep
Suspending console(s) (use no_console_suspend to debug)
PM: suspend of devices complete after 103.304 msecs
PM: late suspend of devices complete after 0.363 msecs
max77696-rtc max77696-rtc.0: max77696_rtc_resume: alarm_time:1793464155 resume_time1793464155
PM: resume of devices complete after 932.431 msecs
PM: Finishing wakeup.
```

Service log:

```text
GW_SYSTEM: display off (saved light=1)
GW_SYSTEM: entering mem suspend (usb=2 charging=1)
GW_SYSTEM: resumed without button (RTC/wakeup source)
```

After the RTC wake the panel stayed dark and locked; the service scheduled a
bounded retry instead of leaving the device awake indefinitely. `gwctl status`
reported `light=0`, `lock=locked`, `display=off`.

## Full button path (uinput evdev, real suspend)

A root-only uinput device named `gw-test-power` was created and the service was
started with the documented diagnostic overrides:

```sh
GW_POWER_BUTTON_DEV=/dev/input/event2 \
GW_POWER_USB_OVERRIDE=0 \
GW_SUSPEND_FALLBACK_SECONDS=10 \
/usr/sbin/gw-system --supervise
```

`GW_POWER_USB_OVERRIDE` only substitutes the cable classification for this run;
the suspend itself is the real kernel transition. Injecting one short press:

```text
GW_SYSTEM: power button fd=5
GW_SYSTEM: power button key down
GW_SYSTEM: display off (saved light=1)
GW_SYSTEM: suspend scheduled in 1500 ms (lock=1 display=0)
GW_SYSTEM: suspend cancelled
GW_SYSTEM: entering mem suspend (usb=0 charging=0)
GW_SYSTEM: resumed without button (RTC/wakeup source)
GW_SYSTEM: suspend scheduled in 30000 ms (lock=1 display=0)
```

Kernel log for the same transition:

```text
[ 1274.761130] PM: Entering mem sleep
[ 1275.289236] max77696-rtc max77696-rtc.0: max77696_rtc_resume: alarm_time:1793465597 resume_time1793465597
[ 1275.798724] PM: Finishing wakeup.
```

Injecting a second short press after the RTC wake:

```text
GW_SYSTEM: power button key down
GW_SYSTEM: display on (light=1 lock=locked)
```

`gwctl status` then reported `light=1`, `display=on`, `lock=locked`; the lock
screen still requires Unlock, as designed.

## Lock screen follows the power button (fix, 2026-10-31)

The first build only turned the frontlight off: `gw-system` set its internal
`lock=locked` but the shell kept showing Home. The service now broadcasts
`POWER LOCK`, `POWER WAKE` and `POWER UNLOCK` on `/run/gw-system.evt`, and the
shell consumes that stream in its render loop.

Verified with the same uinput `KEY_POWER` path while the cable was attached
(so the press locks without suspending):

```text
GW_SYSTEM: power button key down
GW_SHELL: event POWER LOCK
GW_SHELL: lock screen engaged
GW_UI_DIAG: screen=lock          # gwctl ui-screen
STATUS ... light=0 ... lock=locked display=off
```

Second press:

```text
STATUS ... light=1 ... lock=locked display=on
GW_SHELL: event POWER WAKE
GW_UI_DIAG: screen=lock          # still locked, as designed
```

Unlock:

```text
GW_SHELL: event POWER UNLOCK
GW_UI_DIAG: screen=home
STATUS ... lock=unlocked display=on
```

The shell also syncs `lock=` from STATUS on startup, so a shell restart while
the service is already locked shows the lock screen instead of Home. `gwctl
lock` / `gwctl unlock` use the same event path and were verified separately.

Actual panel proof: the device framebuffer captured with
`tools/maintenance/capture-screen.py` while `lock=locked light=0` shows the
lock screen, not the previous page:
`docs/evidence/pw3-app-ui-2026-10-31/lock-screen-device.png`.

## Lock while KOReader is foreground

This was the other half of the reported symptom: with the reader on screen
there is no shell listening to the event stream. The reader adapter polls
`gwctl status` for `lock=locked`; when it sees the service lock it now calls
`exitOrRestart(function() UIManager:quit() end)` instead of restarting its own
UI. The service then starts a fresh shell, which syncs STATUS and shows the OS
lock screen.

```text
before: foreground=koreader ... lock=unlocked display=on light=9
$ gwctl lock          # same display_off path as a short power press
after:  foreground=none ... lock=locked display=off light=0

GW_SHELL: event POWER LOCK
GW_SHELL: startup/status lock engaged
GW_UI_DIAG: screen=lock
ps: /usr/sbin/gw-shell
```

Unlock:

```text
GW_SHELL: event POWER UNLOCK
GW_UI_DIAG: screen=home
foreground=none ... lock=unlocked display=on light=9
```

## Cable/charging gate

With the cable attached, the non-forced command was rejected without touching
the panel or `mem`:

```text
$ gwctl suspend
ERR cable-or-charging
```

The policy tests also cover USB power-only, USB host, charging and unknown
states; only `usb=none` **and** `charging=0` permits suspend.

## Cleanup

The uinput process and node were removed, the service was restarted without
diagnostic overrides, and `/dev/input/` contains only `event0` and `event1`.
Final status reported `usb=host-class`, `lock=unlocked`, `display=on`.

## Remaining manual check

Press the physical power button once while unplugged, then press again to wake.
That confirms the membrane/PMIC button path; the service decision, evdev read,
lock, kernel suspend, RTC wake, light restore and resume path are already
verified above.

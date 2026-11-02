---
diataxis: explanation
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Boot and recovery

The boot path is designed so that a failed trial or dirty root falls back to an
independent USB rescue instead of depending on the installed root image.

## Normal boot

1. The stock bootloader loads the retained kernel.
2. The static bootstrap on p1 starts, prepares the early devices and opens the
   p4 FAT control volume.
3. Bootstrap checks `boot.enabled`, `boot.pending` and the root image state.
   A clean, enabled image is mounted and Alpine becomes PID1.
4. `gw-system --supervise` starts and owns power, Wi-Fi, apps and the command
   socket. It launches `gw-shell`; the shell paints Home and starts reader
   autostart when enabled.
5. A short power press locks the screen, turns off the frontlight and enters
   `mem` suspend when no cable or charger is attached. RTC wake is a fallback;
   an unattended RTC wake keeps the display dark and sleeps again.

## Failure paths

| Condition | Result |
| --- | --- |
| `boot.pending` remains after a crash | Bootstrap selects independent USB rescue |
| Root image is dirty or fails checks | Rescue path; repair or replace offline |
| Guardian deadline expires before promotion | Reset and return to stock entrypoint |
| Reader or UI fails but PID1 is alive | USB serial console remains available |
| Kernel, p1 or eMMC is damaged | p4 rescue cannot recover it |

The rescue gadget is `0525:a4a5` with bcdDevice `0200` and is writable once the
host accepts the handshake. Native serial is `0525:a4a7`. An empty `a4a6` or
stock `1949:0004` is not writable rescue.

## Design consequences

- Network fallback, a second OS slot and a generic recovery menu are not
  implemented. Recovery is physical USB plus the retained stock init.
- `boot.enabled` and `boot.pending` are boot-safety state, not logs. They stay on
  FAT; the kernel and bootloader are never rewritten by normal maintenance.
- A successful host build or QEMU run does not prove boot, rescue or panel
  behaviour. Those require device evidence.

Operational steps are in [`../how-to/recover-usb.md`](../how-to/recover-usb.md)
and [`../how-to/operate-reader.md`](../how-to/operate-reader.md). Historical
investigations are indexed in [`../history/README.md`](../history/README.md).

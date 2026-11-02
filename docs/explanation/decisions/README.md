---
diataxis: explanation
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Decisions

Architecture decisions are recorded here when they constrain current work.
Superseded designs and investigation reports belong in
[`../history/specs/`](../../history/specs/) and are not current instructions.

| ID | Decision | Status |
| --- | --- | --- |
| D1 | Keep the installed PW3 kernel and bootloader; replace the userspace with Alpine | accepted |
| D2 | No X11/Wayland/compositor; LVGL renders directly to EPDC | accepted |
| D3 | `gw-system` is the single hardware owner; `gw-shell` is a client | accepted |
| D4 | One signed `.gwapp` package format with a release key on the device | accepted |
| D5 | Recovery is independent USB rescue plus retained stock init | accepted |
| D6 | UI copy is English; user content remains multilingual | accepted |
| D7 | Display policy uses `GC16_FAST` for partial updates and `GC16` for deep cleans | accepted |
| D8 | Generic app launch, per-app UIDs and fd handoff remain open | pending |

## D1 — native Alpine on retained firmware

The stock kernel already contains the display, touch, power and storage drivers
for this exact device. A mainline kernel or bootloader port adds risk without
improving the reader experience. The accepted trade-off is a narrow hardware
profile and dependence on captured vendor modules.

## D2 — direct EPDC rendering

A compositor adds latency and power cost on a bistable display. LVGL plus one
EPDC submission layer keeps the dirty-region and waveform policy explicit. The
cost is that every screen must follow the mandatory display rules.

## D3 — single hardware owner

Power, Wi-Fi, app installation and reader lifecycle share one supervisor.
This prevents two processes from fighting over the panel, input or radio. The
command socket is a fixed-command contract, not a general shell.

## D5 — physical recovery

Network recovery and a recovery menu would depend on the very services that
may be broken. The accepted path is a physical USB rescue that does not use the
installed root image. p1/kernel damage is explicitly outside that guarantee.

## D7 — waveform policy

`GC16_FAST` gives acceptable partial-update quality without the ghosting of
`DU`/`A2`/`DU4`; full `GC16` is reserved for lock/unlock, first Home after boot
and explicit deep clean. Measurements and rejected alternatives are in
[`../history/technical/pw3-paint-policy-2026-10-31.md`](../../history/technical/pw3-paint-policy-2026-10-31.md)
and [`eink-display.md`](../eink-display.md).

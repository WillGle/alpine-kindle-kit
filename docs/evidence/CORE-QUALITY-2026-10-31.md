# Core code quality review (2026-10-31)

## Scope
Core files reviewed (kept, in use):
- os/system/gw-system.c, gwctl.c, gw-network.c, gw-power.h, gw-ipc.h, gw-app-manifest.*
- os/ui/gw-shell.c, lv_port_epdc.h, lv_port_input.h
- os/system/gleworks-wifi.sh, gleworks-openrc.sh, os/devices/pw3/overlay/etc/gleworks-early.sh
- os/alpine/overlay/*, os/devices/pw3/...

Spike/trial files marked (kept, not accepted):
- os/system/gw-app-manifest.c/h (app manager, accepted=0)
- os/ui/* (LVGL spike, accepted=0)
- gw-data-mount.c is not present; Makefile already records it removed.

## Actions taken
- Added `STATUS: CORE (in use)` / `STATUS: SPIKE (accepted=0)` markers to source headers.
- Re-ran host syntax checks for system sources under all flag combinations: PASS (no warnings with -Werror).

## Findings
- Core C sources compile cleanly under -Wall -Wextra -Werror for both base and full spike flag sets.
- gw-data-mount.c does not exist in tree; no action needed beyond Makefile note.
- Shell segfault (HANDOFF) remains unresolved; LVGL spike stays accepted=0.

## Limits
- No new device qualification; host syntax checks only.
- Spike files remain experimental and must not be shipped as production.

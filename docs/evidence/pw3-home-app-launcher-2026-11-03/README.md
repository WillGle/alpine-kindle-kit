---
status: current
updated: 2026-11-03
verified_build: 990061d1630da5de49bd4c5df3fdd6b00c62731115147e8b321e93b67ce2b069
supersedes: none
audience: maintainer
---

# Home app launcher — host checks and PW3 trial (2026-11-03)

Status: **UI and third-party app launch verified on the PW3 trial shell.**
This is not full app-runtime acceptance: distinct UIDs, resource limits,
display/input fd handoff, crash/OOM handling and the `AS-*` checklist remain
open (B07).

## Scope

- Home has an **APPS** band below the Continue Reading and widget cards.
- Core tiles: **Reader**, **Kindle Hub**, **App manager** and **Settings**.
- Installed third-party bundles get a separate **INSTALLED** tile row.
  Tapping a tile sends `APP LAUNCH <id>`; the shell exits, the service starts
  the app entrypoint, and restarts the shell when the app exits.
- The bottom bar is removed. Settings, Calendar and To-dos are standalone
  screens with Back.
- Settings has one **Reboot** action; duplicate Library, Kindle Hub and App
  manager rows are gone.
- Home shows the date as `DD/MM/YY` in the calendar card and no longer repeats
  a full date line under the status bar.

## Build

| Binary | SHA-256 |
| --- | --- |
| `gw_shell` | `990061d1630da5de49bd4c5df3fdd6b00c62731115147e8b321e93b67ce2b069` |
| `gw_system` | `db2b54185ce3eeeec744786e8a86e66fa42dc7b7dfcf341ff45344f6f9556fd0` |
| `gw_appctl` | `60d6aff592f78532ec1ee4265b9eaba7904ad9b8669d179b3fba11fc1bc412ff` |

Deployed in place over the running PW3 root; the previous `gw-shell` and
`gw-system` are retained in `bin-backup/` on the control partition. Boot ID
`6986b82c-62b7-4ef5-ae98-390e55cf339f` did not change.

## Results

| Check | Result |
| --- | --- |
| `make check` | **CHECK PASSED** |
| Host geometry | **2711 checks, 0 failed** |
| App tile count / size | 4 core tiles, 226×144 px each |
| Device Home render | **PASS** (screenshot) |
| Device Settings/Calendar render | **PASS** (screenshots) |
| Third-party tile install | **PASS** — test bundle appeared in an `INSTALLED` row |
| Third-party launch | **PASS** — tile tap ran the entrypoint, then returned to Home |
| Theme adapter check | **up to date** |
| Docs contract | **DOCS CHECK PASSED** |

The launch test used a signed `test-launcher` bundle whose entrypoint wrote
`test-launcher ran` to `/run/test-launcher.log`, slept two seconds and exited
0. The app was removed after the test. Physical tap input was injected through
the touch event node; the screenshots are framebuffer captures.

## Not proven

- Real finger touch, ghosting and label legibility on the panel.
- App runtime isolation: distinct UID, `RLIMIT_*`, fd handoff, crash/OOM,
  watchdog and the rest of B07.
- The Reboot button was rendered and scrolled into view but not pressed
  during this run.
- Reader open/close, Kindle Hub tunnel shutdown on app exit and long-running
  app lifecycle.

## Reproduction

```sh
nix-shell tools/build/nix-shell.nix --run 'make check'
nix-shell tools/build/nix-shell.nix --run \
  'unset CC STRIP CFLAGS; GW_ENABLE_HW_TRIAL=1 bash tools/build/build-arm-binaries.sh'
```

Host capture: [`screenshots/home.png`](screenshots/home.png).
Device captures: [`home-device.png`](screenshots/home-device.png),
[`home-device-app-tile.png`](screenshots/home-device-app-tile.png),
[`settings-device-actions.png`](screenshots/settings-device-actions.png),
[`calendar-device.png`](screenshots/calendar-device.png).

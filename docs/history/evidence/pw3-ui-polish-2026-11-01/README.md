---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# PW3 shell UI polish — host evidence (2026-11-01)

Scope: Phases 0–7 from
[PW3-UI-POLISH-PLAN.md](../../technical/PW3-UI-POLISH-PLAN.md). Host simulator
and ARM build only. No device write, deployment or physical-panel acceptance
happened in this run.

## Result

| Check | Baseline (2026-10-31) | This run |
| --- | ---: | ---: |
| Host geometry/behaviour checks | 789 / 0 failed | **2504 / 0 failed** |
| LVGL heap used | 173256 / 262144 (67%) | **182792 / 262144 (69.8%)** |
| LVGL heap max used | 69356 | 72659 |
| ARM `gw-shell` bytes | 2,626,560 | **3,064,832** (+438,272, +0.418 MiB) |
| Paint policy | `quality` | `quality` (unchanged) |
| ARM sha256 | — | `6020c6fe4633b74ac1ad95e0e5db3ac5e0d27e54dc94a178ecc31f43314b91df` |

Baseline tag: `ui-polish-baseline`.

Commands:

```sh
python3 theme/generate.py --check     # theme adapters are up to date
./os/ui/sim/run-preview.sh docs/evidence/pw3-ui-polish-2026-11-01
./os/ui/build-arm.sh                  # 3064832 bytes, verified static ARM build
make -C os/system check               # syntax check passed
make -C os/system test                # stage3/power/power-io/apps tests PASS
```

QEMU smoke:

```sh
GW_UI_SMOKE_FB=1 qemu-arm os/ui/gw-shell
# GW_UI: EPDC port init 1072x1448 pitch=1088 accepted=0 (QEMU smoke, no /dev/fb0)
# GW_UI: QEMU smoke input (no evdev)
# GW_SHELL: QEMU smoke screens built
```

Smoke mode is env-gated: it never opens `/dev/fb0` or evdev, builds every
screen and exits 0. The normal device path is unchanged.

## Screens

24 grayscale 1072×1448 captures:

`home`, `home-empty`, `settings`, `settings-scrolled`,
`calendar`, `calendar-next-month`, `calendar-selected`, `calendar-empty`,
`todos`, `todos-history`, `todos-empty`, `todos-undo`,
`network`, `network-connected`, `network-list`, `network-password`,
`network-confirm`, `network-empty`, `apps`, `apps-long`, `update`, `lock`,
`confirm-remove`, `kit`.

The `kit` capture proves primary, secondary, pressed, disabled, busy,
checked/unchecked checkbox and signal-bar states.

## Behaviour checks added

- Five nav tabs at `1072/5 = 214` px with correct offsets on top-level
  screens; zero tabs on Network/Applications/App updates/Lock; Lock has no
  Back.
- Every clickable control is at least 96×96; no clickable nested inside a
  clickable.
- Explicit primary map: Home/Settings/Calendar 0, To-dos 1, Network 1,
  Applications 1, App updates 1, Lock 1.
- Home has no `QUICK ACCESS` and no `Today.` label; Wi-Fi, Light, Lock and
  battery are 96 px status-bar targets.
- Home calendar card renders two 72 px upcoming rows (event + next due task);
  the empty state shows one message and hides the second row.
- Every clickable card has 2 px normal / 4 px pressed border feedback.
- Network connection card is 220 px and every network list row carries the
  four-rectangle signal-bar widget; signal levels match the dBm mapping.
- English copy catalogue is ASCII-only; user content remains multilingual.
- Active view excludes done/deleted; history carries DONE/DELETED and
  Restore; delete → Undo restores the task.
- Due-date round-trip plus legacy five-field and new six-field TSV parsing.
- Calendar month navigation, leap year, Monday-first offset, selected-day
  partial update, empty-day message, 96 px cells.
- `NET SSID` old plain and new hex forms, malformed/odd hex rejection, signal
  thresholds, confirm-before-action, busy rejection, one full paint per modal
  lifecycle.
- 8-app fixture renders and scrolls; unchanged refresh does not rebuild the
  panel; a single-row change updates only that row.
- 10 rapid status updates with unchanged text produce zero label writes.
- Quality policy sends one content update per scene; typing keeps the A2/DU
  fast path.
- Every new icon codepoint (`f02d f073 f14a f023 f012 f2ed f06e f070 f053
  f054 f067 f068`) resolves in the generated font, and 32 px regular/bold
  fonts exist.

## IPC change

`NET RESULT <n>` now returns `NET SSID <ssid_hex> <signal_dbm> <secure 0|1>`;
`NET END` still terminates the list. The shell accepts the old plain-SSID
form and rejects malformed hex. `gw-network.c`, `gw-system.c`, `gw-ipc.h` and
`os/system/IPC.md` were updated together.

## Device deployment (2026-11-01, device clock 00:56)

The verified ARM binary was pushed over the physical USB console and installed:

- `/usr/sbin/gw-shell` sha256
  `6020c6fe4633b74ac1ad95e0e5db3ac5e0d27e54dc94a178ecc31f43314b91df`
  (3,064,832 bytes).
- Rollbacks retained:
  `/usr/sbin/gw-shell.before-ui-polish-20261101` (original baseline
  `0163000eb9e8e45e3b34434fd7ea2712e7caa7d067fcfe11ff3755f5262b9fcd`) and
  `/usr/sbin/gw-shell.before-tap-flash` (previous polished build).
- `/etc/init.d/gw-system restart` started the new supervisor and shell
  (PIDs 10867/10869); the process stayed up.
- Framebuffer capture [device-home.png](device-home.png) shows the new
  five-tab Home, 96 px status targets, week strip and empty reading state.

Follow-up fixes in this build: frontlight +/- steps are **1%** (user decision),
Settings and all list pages receive drag events so they scroll, an unchanged
Wi-Fi scan no longer rebuilds/reflashes the network list, and navigation/modal
repaints use a whole-screen **DU4 partial** instead of GC16. Taps no longer
invert the panel black/white; drag scrolling is coalesced into one partial
DU4 update.

This is a deployment record, not physical visual/paint acceptance. Tap
behaviour, ghosting and the paint-quality gate remain to be checked by hand.

## Open release gate

Device deployment and visual/paint acceptance remain a separate, human-run
step. The paint-quality regression from
`docs/technical/superpowers/plans/2026-10-31-eink-paint-quality.md` is still
mandatory before release.

---
status: current
updated: 2026-11-02
verified_build: 1804ea8fcf32d427ba715e431de79e8cf2f671b6a4413f1d6f3ff2b359fcddaf
supersedes: none
audience: maintainer
---

# Workstream F — UI/UX hierarchy evidence (2026-11-02)

Status: **complete on host and physical PW3 (trial build), re-verified after
the 2026-11-02 review.**

Plan: [`docs/history/plans/2026-11-02-ui-ux-hierarchy.md`](../../history/plans/2026-11-02-ui-ux-hierarchy.md)
Review: [`REVIEW.md`](REVIEW.md)
Backlog: [`B24`](../../status/BACKLOG.md).

## Result

| Gate | Result |
| --- | --- |
| Host gate `nix-shell tools/build/nix-shell.nix --run "make check CC=cc"` | **CHECK PASSED** |
| Host-sim geometry | **2856 checks, 0 failed** |
| Host-sim screenshots | `screenshots/` (1072 × 1448, grayscale) |
| Physical PW3 framebuffer captures | `device/` (1072 × 1448) |
| Deployed trial shell | `sha256 1804ea8fcf32d427ba715e431de79e8cf2f671b6a4413f1d6f3ff2b359fcddaf` |
| Rollback shell on device | `/usr/sbin/gw-shell.before-hierarchy-20261102` = `80160c3a0704ebd6857176564c38397b357c92adf8d3eac5f1d7d6fb63115ff4` |
| Device boot used for captures | `5801ea5d-b710-4ee1-b07e-3181ab2c6e6d` |
| Device build flags | `EPDC_accepted=1 touch_accepted=1 GW_ENABLE_HW_TRIAL=1 GW_UI_DIAG=1` |

The device build is a **trial** build (explicit hardware-trial flag), not the
default hardware-stub build. The default build remains the accepted policy.

## Review follow-up (2026-11-02)

The first completion pass was reviewed and these items were closed:

- **F9 P0:** the calendar was mixing a Sunday-first header with a Monday-first
  offset. The offset is now `tm_wday` (Sunday-first) and the test checks the
  weekday header and day label in the same column for 2026-10-01 (Thu),
  2026-11-01 (Sun) and 2027-02-01 (Mon).
- **F3 duplicate actions:** switched to the selected-package model. There is
  one Scan USB action, one “Install selected” action, and no install-all
  action. Tests assert the visible label counts.
- **F2 duplicate actions/messages:** the network empty state owns the only Scan
  action (the top Scan/header row is hidden), its hint is different from the
  status line, and tests assert one Scan and one “Select a network.” label.
- **F4 invalid scrolled capture:** re-taken with `gwctl ui-scroll -700`; the
  image now shows the Software section, all ABOUT DEVICE rows and Lock now.
  The host test asserts the Date row is inside the viewport after scrolling.
- **F1 tests:** now assert content-bounded card heights and that the To-dos
  card shrinks when its rows disappear, instead of only checking top
  alignment.
- **F5 wording:** recorded as an all-visible chip row, not a dropdown.

## Per-item evidence

### F1 — Home empty state and card hierarchy
- Empty Continue Reading card collapses to `GW_UI_HOME_EMPTY_CARD_H` (160 px)
  and hides the redundant “CONTINUE READING / Tap to resume” header.
- Populated card uses `GW_UI_HOME_READING_CARD_H` (320 px).
- Calendar/To-dos use content height with token min-heights instead of equal
  stretch; actions live in the card headers.
- Host: `home.png`, `home-empty.png`; geometry `empty_reading_height`,
  `reading_height`, `widgets_content_sized` (both cards bounded below 600 px),
  `notes_height_tracks_content` (To-dos card shrinks when rows disappear) and
  `reading_full_margin`.
- Device: `device/home.png`.

### F2 — Network action placement and state flow
- Sticky bottom action row for Connect/Disconnect/Forget; the list is the
  scroll region.
- Connect is disabled until a row is selected and is hidden when the selected
  network is already connected.
- Contextual message covers “Select a network”, “Enter password for …”,
  “Connecting…”, “Connected”.
- Empty state is centred and owns the only Scan action; the top Scan/header
  row is hidden while empty, and its hint no longer repeats the status line.
- Device rows are never seeded from invented data; the list starts empty and
  is filled by real `NET SCAN RESULTS`.
- Host: `network.png`, `network-empty.png`, `network-password.png`,
  `network-connected.png`, `network-connecting.png`, `network-list.png`.
- Device: `device/network-empty.png` (real no-selection state),
  `device/network-list.png`, `device/network-selected.png`,
  `device/network-open.png`. The populated device list uses the shell’s
  documented `NETFILL` diagnostic fixture because the attached AP returned no
  second network on this boot; the empty/no-selection capture is real device
  state.
- Geometry: `connect_disabled_without_selection`,
  `empty_clears_stale_selection`, `empty_scan_actions` (exactly one Scan),
  `empty_select_messages` (one “Select a network.”), `secondary_status_visible`.
- The device no longer shows a blank second row; hidden/blank/non-printable
  SSIDs are filtered.

### F3 — Applications / App updates information architecture
- Installed list fills the available height; the only row action is Refresh.
- App updates are a section on App manager, not a second destination.
- Selected-package model: one Scan USB action, one “Install selected” action,
  and no install-all button.
- Update order is package list → Scan/Install selected → result/status.
- OS identity moved to Settings → About device; screen is labelled
  “App manager”.
- When the installed list is empty, the empty state owns the single Refresh
  action and the duplicate row is hidden. When no USB package is present, the
  USB empty state owns the single Scan action and the button row hides Scan.
- Host: `apps.png`, `apps-empty.png`, `apps-long.png`.
- Device: `device/apps.png` (empty state + no USB packages).
- Geometry: `single_update_path`, `app_manager_title`, `usb_scan_actions`
  (exactly one), `usb_install_actions` (one Install selected, no install-all),
  `status_next_to_action`, `empty_hides_sticky_actions`, `empty_one_refresh`,
  `one_primary_after_selection`, `long_list_rows` (8 apps).

### F4 — Settings row pattern and grouping
- Whole-row navigation targets with a chevron; no “Open” buttons.
- Applications and App updates are one App manager entry.
- Battery/Storage/Uptime/Version/Date grouped under “ABOUT DEVICE”.
- Frontlight remains a direct control row.
- Content uses an always-visible scrollbar; the first viewport ends between
  full rows, not through one.
- Host: `settings.png`, `settings-scrolled.png`; geometry
  `no_legacy_text` (“Open”), `secondary_status_visible`,
  `scroll_reaches_last_row`, `scrolled_about_device_visible` (the Date row is
  inside the viewport after scrolling).
- Device: `device/settings.png`, `device/settings-scrolled.png`. The device
  scrolled capture was re-taken with `gwctl ui-scroll -700`; it shows the
  Software section, all ABOUT DEVICE rows and Lock now.

### F5 — To-dos due-date selector
- Five bordered due chips in one row; all five fit the 1072 px content width
  with token width and no edge contact. This is an all-visible chip row, not a
  dropdown.
- Active/History remain the primary filter.
- Empty state is centred and the add field stays visible.
- Host: `todos.png`, `todos-empty.png`, `todos-history.png`;
  geometry `due_chips_all_visible`, `due_button_font`.
- Device: `device/todos.png`.

### F6 — Lock
- One affordance: clock + date + “Tap to unlock”; the whole surface is the
  touch target.
- Host: `lock.png`; geometry `one_unlock_affordance`,
  `whole_screen_unlock_target`, `one_primary_map` (0 primary buttons).
- Device: `device/lock.png`.

### F7 — Secondary status strip
- Every title/app bar carries a compact time + battery strip in its own row,
  with no collision with the title or between labels.
- App bar remains `GW_UI_APP_BAR_H` (136 px), above the 96 px touch minimum.
- Host and device screenshots show time/battery on Settings, Calendar,
  To-dos, Network and App manager.
- Geometry: `secondary_status_visible` on Settings, Network and Apps.

### F8 — Shared empty state
- One shared component (`gw_ui_kit_empty_state_create`) is used for no book,
  no networks, no tasks, no apps and no USB packages.
- Each empty state has one action; the App manager empty state does not
  duplicate the sticky install action.
- Host: `home-empty.png`, `network-empty.png`, `todos-empty.png`,
  `apps-empty.png`; device: `device/home.png`, `device/network-empty.png`,
  `device/apps.png`.
- Geometry: `empty_one_primary`, `empty_hides_sticky_install`.

### F9 — Calendar
- Fixed the date alignment regression: the Sun..Sat header and the
  `tm_wday` offset are both Sunday-first. The test matrix checks the weekday
  label and day label in the same column for 2026-10-01 (Thu), 2026-11-01
  (Sun) and 2027-02-01 (Mon).
- Removed the repeated “Today” right label; up to five due rows are rendered
  and the rest are reported as “+N more”.
- Host: `calendar.png`, `calendar-selected.png`; device:
  `device/calendar.png`.

### F10 — Global hierarchy and tokens
- One primary action per screen is asserted by `one_primary_map` on Home,
  Settings, Calendar, To-dos, Network, Apps and Lock.
- New layout values are in `theme/tokens.json` (`layout` map) and generated
  through `theme/generate.py`; the generated adapters pass
  `theme/generate.py --check`.
- Cards remain for primary surfaces; section labels and separators carry
  secondary information.

## Device acceptance notes

- The panel was captured through `/dev/fb0` over the physical USB console.
- Screen navigation used `gwctl ui-nav` and `gwctl ui-tap`; lock used
  `gwctl lock` / `gwctl unlock`.
- No separate human design-review signature is stored in the repository; the
  acceptance basis is the physical framebuffer captures in `device/` plus the
  geometry suite above.

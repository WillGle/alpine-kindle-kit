---
status: current
updated: 2026-11-02
verified_build: 1804ea8fcf32d427ba715e431de79e8cf2f671b6a4413f1d6f3ff2b359fcddaf
supersedes: none
audience: maintainer
---

# Phase F review — UI/UX hierarchy (2026-11-02)

Status: **PASS, with the documented residuals below.**

Plan: [`docs/history/plans/2026-11-02-ui-ux-hierarchy.md`](../../history/plans/2026-11-02-ui-ux-hierarchy.md)
Evidence: [`README.md`](README.md) · `screenshots/` · `device/` · `geometry.json`
Backlog: [`B24`](../../status/BACKLOG.md)

## Scope

This review covers Workstream F only (F1–F10), not the A–E remediation. The
review verified:

- the plan checklist and its evidence claims;
- the F-specific code paths and shared kit changes;
- theme-token usage for layout values added by F;
- host geometry and screenshots;
- physical PW3 framebuffer captures and build identity;
- the review findings raised after the first completion pass.

## Verdict

| Gate | Result |
| --- | --- |
| Plan checklist | **58 checked / 0 unchecked** |
| Host gate `make check CC=cc` | **CHECK PASSED** |
| Docs check | **DOCS CHECK PASSED** |
| Host-sim geometry | **2856 checks / 0 failed** |
| Host screenshots | 25 × 1072×1448 |
| Device captures | 12 × 1072×1448 |
| Geometry artifact | `geometry.json` sha256 `a8be0bab1e73c106e1e4e7b34dbec76eda28ff51672c8042c8e75f0bfd668eea` |
| Deployed trial shell | `1804ea8fcf32d427ba715e431de79e8cf2f671b6a4413f1d6f3ff2b359fcddaf` |
| Capture boot | `5801ea5d-b710-4ee1-b07e-3181ab2c6e6d` |
| Device state after review | clean reboot, `f29917d9-b633-46ae-98b8-a7c6214c9a92` |

## Per-item review

### F1 — Home empty state and card hierarchy
- Empty card uses `GW_UI_HOME_EMPTY_CARD_H`; the redundant Continue Reading
  header is hidden while empty.
- Populated card uses `GW_UI_HOME_READING_CARD_H`; Calendar/To-dos use
  content height with token min-heights.
- Card actions are in the headers.
- Tests now check content-bounded heights and that the To-dos card shrinks
  when its rows disappear, not just top alignment.
- Evidence: `home.png`, `home-empty.png`, `device/home.png`.

### F2 — Network action placement and state flow
- Sticky Connect/Disconnect/Forget row; list is the main scroll region.
- Connect is disabled without a selection and hidden when already connected.
- Contextual status covers select/password/connecting/connected.
- Empty state owns the only Scan action; the top Scan/header row is hidden
  while empty. The empty hint is distinct from the status line.
- Tests assert one Scan and one “Select a network.” label, and that stale
  selections are cleared when the list is rebuilt.
- Device rows are never seeded from invented APs.
- Evidence: `network*.png`, `device/network*.png`.

### F3 — Applications / App updates information architecture
- One screen, “App manager”; updates are a section.
- Selected-package model: one `Scan USB`, one `Install selected`, no
  install-all action; `Refresh` is the only installed-list row action.
- Empty states own the single action for their own list.
- Tests assert visible action-label counts, status adjacency, and one primary
  action after package selection.
- Evidence: `apps.png`, `apps-empty.png`, `apps-long.png`,
  `device/apps.png`.

### F4 — Settings row pattern and grouping
- Whole-row navigation targets with chevrons; no “Open” buttons.
- App manager is one entry; About device groups Battery/Storage/Uptime/
  Version/Date; Frontlight remains a direct control.
- Always-visible scrollbar; the scrolled view shows the complete About device
  section and Lock now. The device capture was re-taken with
  `gwctl ui-scroll -700`.
- Host test asserts the Date row is inside the viewport after scrolling.
- Evidence: `settings.png`, `settings-scrolled.png`,
  `device/settings.png`, `device/settings-scrolled.png`.

### F5 — To-dos due-date selector
- One bordered chip row, all five options visible at 1072 px; due date remains
  secondary to Active/History.
- Empty state is centred and the add field stays visible.
- Tests assert all five chips fit the content width.
- Evidence: `todos*.png`, `device/todos.png`.

### F6 — Lock
- Clock + date + “Tap to unlock”; the whole screen is the touch target.
- Tests assert one unlock affordance, no Locked label, no Back, and no primary
  button.
- Evidence: `lock.png`, `device/lock.png`.

### F7 — Secondary status strip
- Every title/app bar has a compact time + battery strip in its own row.
- No collision between time, battery, and title; app bar height remains above
  the 96 px touch minimum.
- Tests assert visible time and battery on Settings, Network and Apps.
- Evidence: all secondary-screen host/device captures.

### F8 — Shared empty state
- One shared component is used for no book, no networks, no tasks, no apps and
  no USB packages.
- Each state has exactly one action; duplicate list actions are hidden.
- Evidence: the empty-state screenshots and action-count tests.

### F9 — Calendar
- Sunday-first header and `tm_wday` offset agree.
- Matrix test checks the weekday label and day label in the same column for
  2026-10-01 (Thu), 2026-11-01 (Sun), 2027-02-01 (Mon).
- No repeated “Today” row label; up to five due rows, then “+N more”.
- Evidence: `calendar*.png`, `device/calendar.png`.

### F10 — Global hierarchy and tokens
- One primary action per screen is asserted where an action exists.
- New F layout values come from `theme/tokens.json` and generated adapters
  pass `theme/generate.py --check`.
- Pre-existing widget-internal dimensions (e.g. slider track/text-area sizing)
  remain legacy literals; they were not introduced by F and are out of scope
  for this pass.

## Review findings closed

- **F9 P0 date alignment:** fixed to Sunday-first; matrix test added.
- **F3 duplicate USB actions:** switched to selected-package model; action
  counts asserted.
- **F2 duplicate Scan/message:** empty state owns Scan; hint no longer repeats
  the status line.
- **F4 invalid scrolled capture:** re-captured with the correct scroll sign;
  host test checks a visible About device row.
- **Weak F1/F5 tests/claims:** content-size and shrink checks added; F5
  documented as an all-visible chip row rather than a dropdown.

## Residual risks / caveats

1. **Human design sign-off** is not a machine-checkable artifact. The physical
   framebuffer captures in `device/` are the stored acceptance basis; a
   separate human signature is not in the repository.
2. **Populated device network captures** use the shell’s documented
   `NETFILL` diagnostic fixture because the attached AP returned no second
   network on the capture boot. The real device empty/no-selection state is
   genuine; the fixture only exercises row/selection rendering.
3. **Apps list viewport** can clip a row at the scroll boundary; this is normal
   for the scrollable 8-app list and actions remain visible.
4. **Settings first viewport** relies on the always-visible scrollbar rather
   than ending exactly on a section boundary; the scrolled view is verified.
5. **Device capture boot vs current boot:** captures were taken on
   `5801ea5d-…`; the device was subsequently rebooted clean to `f29917d9-…`
   with the same shell hash. The temporary `NETFILL` helper and injected APs
   were removed.

## Conclusion

Phase F is complete and documented. No remaining correctness issue is known;
the residuals above are scope or evidence-format limitations, not open
implementation defects.

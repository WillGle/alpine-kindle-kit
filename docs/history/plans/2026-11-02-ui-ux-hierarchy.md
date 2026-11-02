---
status: historical
date: 2026-11-02
superseded_by: ../../status/CURRENT.md
---

# UI/UX hierarchy pass — 2026-11-02

- Status: **complete (2026-11-02, re-verified after review).** Host gate +
  physical PW3 trial acceptance recorded in
  [pw3-ui-hierarchy-2026-11-02](../../evidence/pw3-ui-hierarchy-2026-11-02/README.md);
  review: [REVIEW.md](../../evidence/pw3-ui-hierarchy-2026-11-02/REVIEW.md).
- This is a separate product/design pass, not part of the A–E remediation.
- Backlog entry: [../../status/BACKLOG.md](../../status/BACKLOG.md), item **B24**.
- It requires design sign-off and physical PW3 acceptance before completion.

## Workstream F — UI/UX hierarchy

Status: **complete.** This was separate product/design work, not part of A–E
acceptance. It depends on the UI now being real translation units
(`gw_screen_*.c`) and was not mixed with remediation commits.

### F0. Rules and non-goals

- Fixed PW3 canvas (1072 × 1448); this is **not** a responsive-reflow task.
- One primary action per screen. Selection flows use a sticky action area.
  Navigation rows are whole-row tappable with a chevron, not repeated
  “Open” buttons.
- Empty states are explicit states with one primary action, not leftover
  blank space.
- Keep e-ink constraints: no shadows, no animation-driven layout, borders only
  where they communicate grouping.
- No new features, no app runtime, no paint-policy redesign. Those stay in
  their own projects.
- Evidence: host-sim screenshots + geometry for every change; physical PW3
  acceptance for layout/readability before declaring the pass complete.

### F1. Home: empty state and card hierarchy

**Current issue**
- The “Continue Reading” card is roughly 380 px tall even when no book is
  open. It contains one icon, “No book open”, and one sentence.
- Calendar and To-dos are forced to equal height, so low-content widgets become
  large empty boxes.
- “View all” / “View calendar” actions sit at the bottom of cards.

**Changes**
- [x] Collapse the empty Continue Reading card to 120–160 px, or replace it
      with a “Recent books / Open Library” entry point.
- [x] Keep the populated Continue Reading card as the primary card.
- [x] Use a min-height for Calendar/To-dos instead of equal stretch; let
      content determine height within the grid.
- [x] Move card actions (“View all”, “View calendar”) into the card header or
      right-aligned chevron.
- [x] Confirm the date hierarchy: keep the hero date, or make it a smaller
      line inside the header. Do not do both.

**Tests / done when**
- [x] Empty Home shows real information above the fold, not a large blank card.
- [x] Populated Home still has one clear primary action.
- [x] Host-sim geometry passes and device screenshot is accepted.

### F2. Network: action placement and state flow

**Current issue**
- Connect is a bottom-left button detached from the selected row; on a long
  list it can be below the fold.
- Connect can be visible/enabled with no selection and only reports “Select a
  network.” after tapping.
- The prompt can remain generic after a row is selected.
- Empty state sits top-left of a large blank area.

**Changes**
- [x] Move Connect/Disconnect/Forget into a sticky bottom action area, or make
      the selected row expose the primary action.
- [x] Disable or hide Connect until a network is selected.
- [x] Make the message contextual: “Select a network”, “Enter password for
      Home Wi-Fi”, “Connecting…”, “Connected”.
- [x] Center the empty state, make its Scan the only Scan action, and keep a
      single contextual status line.
- [x] Keep the list as the main scroll region.

**Tests / done when**
- [x] No-selection, selected-secure, selected-open, connecting, connected, and
      empty states are covered by host-sim screenshots and device acceptance.
- [x] Connect cannot be tapped without a selection.

### F3. Applications / App updates: information architecture

**Current issue**
- Installed list is a short fixed-height box; only about four rows fit while
  actions and Built-in consume the rest of the screen.
- “Install from USB”, “Refresh”, and “App updates” have inconsistent hierarchy.
- “App updates” is a separate destination but is also an action inside
  Applications and Settings.
- The update screen puts system identity first, actions second, package name
  third, status last.

**Changes**
- [x] Let the installed list fill available height; use a compact sticky action
      bar for Install/Refresh.
- [x] Merge “App updates” into the Applications screen (section or tab).
- [x] Use one install path: scan USB packages, select one, then “Install
      selected”. Refresh stays secondary and there is no install-all action.
- [x] In the update section, order content as: selectable package list →
      Scan/Install actions → result/status.
- [x] Move “GleWorks OS 0.1” identity to Settings → About.
- [x] Until app runtime exists, label the screen “App manager” or
      “Installed packages”, not “Applications” as if launch exists.

**Tests / done when**
- [x] With 8 installed apps, the list remains usable and actions stay visible.
- [x] Exactly one Scan USB and one Install selected action; no install-all
      path.
- [x] The status text is adjacent to the action that produced it.

### F4. Settings: row pattern and grouping

**Current issue**
- Every navigation row ends with an “Open” button; the whole row is already a
  touch target.
- “Applications” and “App updates” duplicate the Applications screen.
- Device info is a long static list.
- Initial viewport clips the Battery row; scrolling is not obvious.

**Changes**
- [x] Make navigation rows whole-row tappable and replace “Open” with a
      chevron.
- [x] Merge Applications/App updates into one entry.
- [x] Group Battery/Storage/Uptime/Version/Date under “About device”.
- [x] Keep Frontlight as a direct control row.
- [x] Add a subtle scroll affordance or adjust spacing so the first viewport
      ends at a section boundary.

**Tests / done when**
- [x] No repeated “Open” buttons remain.
- [x] Settings first viewport does not cut a row mid-height; the scrolled view
      shows the About device rows and Lock now.
- [x] Device touch acceptance confirms whole-row targets.

### F5. To-dos: due-date selector

**Current issue**
- Five equal-width due buttons are cramped and compete with Active/History.
- Empty state is left-aligned under the filters.

**Changes**
- [x] Replace the five equal-width buttons with one bordered chip row that
      fits all five options at 1072 px; due date stays a secondary attribute.
- [x] Keep Active/History as the primary filter; due date is a secondary
      attribute.
- [x] Center the empty state and keep the add input visible.

**Tests / done when**
- [x] All five due options remain reachable with one tap after opening the
      selector.
- [x] Text does not touch button edges at 1072 px.
- [x] Empty state is visually centered.

### F6. Lock: remove redundant copy

**Current issue**
- “Locked”, “Tap to unlock”, and an “Unlock” button all say the same thing.

**Changes**
- [x] Keep clock + date + Unlock button, or clock + date + “Tap anywhere to
      unlock” without a button. Do not keep all three.
- [x] Confirm the whole screen or button is the intended touch target.

**Tests / done when**
- [x] One unlock affordance, one touch target, device screenshot accepted.

### F7. Secondary screens: keep status visible

**Current issue**
- Home shows time, Wi-Fi, light, lock, battery. Network/Applications/App
  updates show only Back + title.
- On an e-reader, battery/time should remain visible while navigating.

**Changes**
- [x] Add a compact status strip to secondary app bars, or retain the same
      status bar and reduce the app-bar height.
- [x] Do not duplicate the full status bar if it competes with the title.

**Tests / done when**
- [x] Time and battery are visible on every primary/secondary screen.
- [x] App bar height still respects the 96 px touch minimum.

### F8. Empty-state component

**Current issue**
- Home, Network, To-dos, Applications and Updates all express empty states
  differently.

**Changes**
- [x] Add one shared empty-state component: icon (optional), title, one-line
      explanation, one primary action.
- [x] Use it for no book, no networks, no tasks, no apps, no packages.
- [x] Keep it content-sized and centered in the available area.

**Tests / done when**
- [x] All empty states share the same structure and spacing.
- [x] Each has exactly one primary action.

### F9. Calendar: minor polish only

**Current issue**
- “Today” repeats on every row under a “DUE TODAY” heading.
- Large empty space below the due list.

**Changes**
- [x] Remove the redundant right-side “Today” label or replace it with a time.
- [x] Show more due rows if they exist; otherwise leave the space quiet.

**Tests / done when**
- [x] Calendar layout remains unchanged otherwise; a Sunday-first header and
      `tm_wday` offset agree for 2026-10-01 (Thu), 2026-11-01 (Sun) and
      2027-02-01 (Mon); device screenshot accepted.

### F10. Global hierarchy/card usage

**Current issue**
- Home and Settings use bordered panels for nearly everything; on e-ink this
  can look like a wireframe.
- Primary-action placement is inconsistent across screens.

**Changes**
- [x] Standardize: cards for primary actions, section labels + separators for
      secondary information.
- [x] One primary action per screen; consistent sticky action area for
      selection flows.
- [x] Recheck borders, padding and section spacing against
      `theme/tokens.json`; add tokens rather than literals.

**Tests / done when**
- [x] Home/Settings/Network/Apps/Updates use the same action-placement rules.
- [x] No new hardcoded spacing/colour values outside the theme tokens.

### F priority order

1. F1 Home empty state and card sizing.
2. F2 Network action/state flow.
3. F3 Applications/App updates IA.
4. F4 Settings row pattern.
5. F5 To-dos due selector.
6. F6 Lock simplification.
7. F7 Secondary status bar.
8. F8 Empty-state component.
9. F10 Global hierarchy/card usage.
10. F9 Calendar minor polish (optional/last).

### F dependencies and evidence

- Start only after A–E is frozen; do not mix F layout changes with remediation
  behavior fixes.
- For each F item, produce host-sim `geometry.json` plus before/after
  screenshots, then physical PW3 acceptance. The older
  `pw3-ui-polish-2026-11-01` screenshots are stale (for example they show
  per-row Remove buttons); use the newest `pw3-ui-refresh` evidence as the
  starting point.


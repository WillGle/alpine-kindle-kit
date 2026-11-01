> **Cập nhật 2026-11-01:** chính sách scroll đang chạy là 6 preview/160 ms,
> release GC16 partial trên union, và một full GC16 sau khi dừng cuộn 1.2 giây
> (xem [PW3-DISPLAY-PROGRAMMING-RULES.md](PW3-DISPLAY-PROGRAMMING-RULES.md)).
> Các mô tả REAGL/4 preview bên dưới là lịch sử của build cũ `149ae024…`.

# PW3 paint policy — transitions, waveform and section isolation

Status: implemented and measured on the reference PW3 on 2026-10-31. Applies
to every OS screen: Home, Settings, Calendar, Notes, Network, Applications,
App updates and the lock screen.

> **Regression note (2026-10-31, user feedback):** DU4 transition + deferred
> full REAGL is visibly ghosted and has a second flash after the transition.
> Source now supports `quality` (default: one GC16 per scene), `balanced`
> (DU4 + GC16 every 4 scenes) and `fast` (the policy below). See
> [EINK-UI-RENDERING.vi.md](EINK-UI-RENDERING.vi.md) for the mechanism,
> trade-offs and test protocol. The measurements below remain valid for the
> fast path; they are not evidence that it is the best default.

## Problem

The first interactive build used the same slow waveform for taps and for
screen changes:

- a tap's pressed state was submitted as REAGL and waited for the previous
  EPDC update, so a press+release could be separated by ~690 ms;
- every `gw_ui_show()` forced a full-screen GC16 refresh, so a Home shortcut
  looked unresponsive and flashed the whole panel;
- any touch set `g_eink_du_used`, and the deferred cleanup then forced a
  screen-wide GC16 even when only one small row changed;
- 12 small REAGL updates could accumulate into a full-screen flash.

Measured before the fix (uinput touch, same evdev path):

```text
input press t=1650966219
input release t=1651658423          # 692 ms later, although hold = 80 ms
epdc full transition input_to_submit_us=654987
```

## Policy

### 1. Section isolation

- LVGL still tracks per-object dirty rectangles; `gw_epdc_add_dirty()` keeps
  distant regions separate (8 px merge gap) instead of one bounding box.
- Only medium/large regions (at least 5% of the panel) count toward the
  partial-refresh budget. A clock, battery or single-row update cannot push
  the UI into a full flash.
- Small button feedback never schedules a screen-wide cleanup. Only large
  fast updates do (`g_eink_du_large`).
- Status-bar and section labels use `label_set_if_changed()`; unchanged text
  produces no dirty area.

### 2. Waveform selection

| Situation | Waveform | Update | Character |
| --- | --- | --- | --- |
| Screen transition | `DU4` (7) | partial | four-level, immediate, minimal flash |
| Keyboard key / text typing | `DU` / `A2` | partial | fastest, monochrome |
| Popup / dropdown | `DU` | partial | fast transient |
| Normal small update | `REAGL` (8) | partial | ghost-compensated |
| Deferred transition cleanup | `REAGL` (8) | full | no GC16 white flash |
| Explicit residue reset | `GC16` (2) | full | flashing, only when requested |

### 3. Transition sequence

`gw_ui_show()` sets `gw_eink_hold_partial(700)`, `gw_eink_request_fast(700)`
and `gw_eink_request_transition(700)`, loads the LVGL screen, then calls
`lv_refr_now()` plus `gw_epdc_present()` in the same event cycle. The new
screen is visible immediately via DU4; no full GC16 precedes it.

After 600 ms without input (and with no keyboard/popup), the main loop runs
one full REAGL pass over the already-correct framebuffer. That removes residue
without the GC16 flash.

### 4. Application to every screen

The policy lives in the shared path, not in individual screens:

- `os/ui/gw-shell.c` → `gw_ui_show()`, status timer, lock/lock-screen events;
- `os/ui/lv_port_epdc.h` → `gw_epdc_present()`, `gw_epdc_request_full_*()`,
  `gw_eink_request_fast/transition()`;
- all screen builders use the same EPDC port and `label_set_if_changed()`.

No screen can opt back into a full flash on every repaint.

## After the fix

Same uinput tap (Network tile), production build:

```text
input press  t=5225588787
input release t=5225669561       # 80.8 ms, matching the 80 ms hold
epdc submit marker=7 waveform=7 area=1552256 fast=1
             input_to_submit_us=75890          # transition visible at ~76 ms
epdc submit marker=8 waveform=8 area=1552256 fast=0
             input_to_submit_us=719634          # deferred REAGL cleanup, no flash
```

- Release is read at the expected 80 ms instead of ~690 ms.
- The new screen is painted at ~76 ms with DU4 (four gray levels, not 1-bit).
- Cleanup is a full REAGL ghost-compensating pass, not a GC16 flash.
- Small updates no longer cause a screen-wide refresh.

## Verification

```text
$ make -C os/system CC=gcc check test
Stage 3 tests: PASS
Power policy tests: PASS
Power I/O tests: PASS
App manager tests: PASS

$ ./os/ui/host-sim /tmp/gw-ui-final
geometry checks: 760 total, 0 failed
```

Device build hash: `gw-shell` `434b090f3199ee7193486d126c1acd81912a55fe5ab446c32590b4abaeedc2b1`.

## Policy-mode build (2026-10-31, deployed; visual acceptance pending)

Source adds `quality` (default), `balanced` and `fast`, selected by
`/etc/gleworks/paint-policy` or `GW_UI_PAINT_POLICY`. The transition is one
whole-screen submit in every mode; keyboard feedback uses A2 and large text
fields no longer use DU. ARM `gw-shell` SHA-256:
`0163000eb9e8e45e3b34434fd7ea2712e7caa7d067fcfe11ff3755f5262b9fcd`.
It is deployed on the reference PW3 with policy `quality`; measured logs are in
`docs/evidence/paint-quality/`. Ghost/flash acceptance still requires the user
to judge the physical panel.

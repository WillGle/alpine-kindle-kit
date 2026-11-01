# PW3 virtual-keyboard typing responsiveness

Date: 2026-10-31  
Scope: host-only source change and ARM build. No physical Kindle was touched.  
Related: [Home UI redesign](PW3-HOME-UI-REDESIGN.md),
[E-ink performance](PW3-EINK-PERF.md).

## Reported symptom

Typing a Wi-Fi password with the on-screen keyboard produced a visible flash on
every key and roughly one second of delay per character. Entering a short
password took at least 15 seconds.

## Measured / source-level causes

1. **Delayed password reveal**
   LVGL's default `LV_TEXTAREA_DEF_PWD_SHOW_TIME` is 1500 ms. Every inserted
   password character schedules a `pwd_char_hider` animation that later rewrites
   the whole text area and invalidates it again. On E-ink this creates a second
   full text-area panel update roughly 1.5 s after each key.
2. **Blinking cursor**
   The text cursor used the default 400 ms blink animation, producing periodic
   partial updates even while the user was thinking rather than typing.
3. **Inverted key feedback**
   The default theme applies a dark colour filter to a keyboard key in
   `LV_STATE_PRESSED`. Each tap therefore paints a dark key frame, then a white
   release frame. On E-ink both frames can become separate panel updates.
4. **Slow waveform**
   Normal interactive partial updates use `WAVEFORM_MODE_REAGL`. On this panel
   that is visually slow (~1 s class), so one update per key feels like one
   second per character. `GW_UI_EINK_FAST_MODE` (DU waveform) existed but was
   disabled.
5. **Press/release submitted separately**
   A tap can produce a pressed frame and a released frame. Submitting both
   multiplies panel work by two.
6. **Screen-sized dirty bounding box**
   The text field and keyboard are far apart. Merging every dirty rectangle into
   one bounding box made each key look like a ~1000 px tall update, which the
   panel promoted to a full GC16 flash. This was the dominant cause of the
   flicker and the 1 s-class delay.
7. **Missing keyboard symbols**
   Keyboard special keys use LVGL symbol glyphs. The Vietnamese-only font did
   not contain them, so backspace/enter/arrows rendered as empty boxes.

## Changes

### Text-area policy

- `lv_textarea_set_password_show_time(g_wifi_password, 0)` — password
  characters hide immediately; no delayed whole-field repaint.
- `lv_obj_set_style_anim_time(..., 0, LV_PART_CURSOR)` for both the Wi-Fi
  password field and the To-dos input — no blinking cursor.
- To-dos and password edits now mark a `g_text_input_active` state used by the
  render loop.

### Keyboard appearance and feedback

`gw_ui_style_keyboard()` now:

- inverts a pressed key to black with white text, so the user sees exactly which
  key was pressed;
- removes the theme colour filter and focus/edited outlines;
- uses a light `#E8E8E8` fill for latched keys (Shift/Caps);
- keeps a thin border and no shadow.

The pressed-key highlight is submitted as a small A2 update on its own region —
the same fast monochrome waveform KOReader uses for keyboard highlights.

### Dirty-region separation

`lv_port_epdc.h` now keeps up to eight dirty rectangles instead of one
bounding box. Rectangles merge only when they overlap or are within 8 px.
During typing this yields two small regions:

- key highlight: roughly 12k px, `WAVEFORM_MODE_A2`;
- password text field: roughly 65k px, `WAVEFORM_MODE_DU`.

No full-screen GC16 is triggered per key.  Keyboard and popup dismissal no
longer schedule a full-screen cleanup; only screen transitions do.  The text
field receives a localized REAGL cleanup after typing pauses.

### Fast waveform only for text entry

- `GW_UI_EINK_FAST_MODE` now defaults to `1`.
- DU is used only while a text field is focused and the interaction is
  `INTERACTIVE`. Other taps keep the existing REAGL path.
- KOReader-style A2 is used for key highlights; DU is used for the text field
  only after the key burst settles.
- Keyboard/popup dismissal does not request full-screen GC16.  This avoids the
  occasional whole-screen black/white flash seen during input transitions.

### Screen and keyboard layout

- Screen changes dismiss both keyboards, reset the fast-text flag and restore
  the To-dos layout. This prevents a stale visible keyboard or REAGL typing path
  after returning to To-dos/Network.
- While the To-dos keyboard is open, the input stays above the list and the
  keyboard checkmark adds the current task. The list scrolls independently.

### Input feedback and update priority

- Touch frames are processed **one per render cycle**.  Previously LVGL drained
  press and release in the same cycle, so the pressed key was overwritten in
  the framebuffer before it could be shown.
- Each key press/release refreshes only that key's small rectangle with A2.
  Far-apart keys are never unioned into a keyboard-sized region; that union was
  causing near whole-keyboard flashes.
- Keyboard/popup dismissal sets a 500 ms partial-only guard, so the hiding
  frame cannot be promoted to full-screen GC16.  A real screen transition
  clears the guard and is allowed to use a full refresh.
- Each cycle submits key A2 highlights and popup DU regions first, then the
  text field. Key feedback cannot queue behind text rendering.
- During an active key burst (idle < 250 ms), text-field updates are re-queued
  and the panel is reserved for key A2 feedback.  The text field catches up
  when typing pauses.
- After a 350 ms pause, the text field receives a localized REAGL full update
  to remove DU ghosting.
- Fast text updates are not counted toward the slow partial-update promotion
  limit; the deferred cleanup handles grayscale restoration.
- A2/DU updates do not wait for the previous completion barrier unless the
  previous update was a full GC16.
- The EPDC completion barrier and conservative REAGL path for non-text UI
  remain in place.

### Popup redraw

- The network dropdown list is styled once at build with a white background,
  thin border, no shadow, no animation and a light selected row.
- An open popup gets its own screen-space rectangle. Updates inside it use DU
  partial updates and are exempt from the >50 % full-screen promotion.
- Opening the dropdown dismisses the keyboard first; closing it schedules one
  deferred GC16 cleanup.
- Identical scan results do not call `lv_dropdown_set_options()` again.

### Rollback

The conservative path remains available:

```sh
GW_UI_EINK_FAST_MODE=0 ./os/ui/build-arm.sh
```

With fast mode off, text entry uses REAGL, has no DU residue, and no deferred
GC16 cleanup. The password/cursor/keyboard-style fixes remain active.

## Expected effect

- Immediate inverted key feedback, with no full-screen flash.
- No cursor blink and no 1.5 s password re-render.
- Two small updates per key (A2 key region + DU text field) instead of one
  screen-sized GC16.
- A GC16 cleanup happens once after the keyboard or popup is dismissed, or the
  screen is left, restoring grayscale quality.

Exact milliseconds remain unmeasured on hardware. The change is expected to
remove the dominant 1 s-class per-key cost, but physical acceptance must still
measure it.

## Required physical verification

After the Phase 4D battery session, deploy separately and record:

- [ ] Time to type a 12-character password; compare with the reported ≥15 s.
- [ ] Slow-motion touch-to-visible latency for 20 consecutive keys, median/P95.
- [ ] Number of EPDC updates per 10 keys.
- [ ] No password character is shown in clear text.
- [ ] No cursor blink while focused.
- [ ] Ghosting after 12–20 characters; check readability before and after the
      deferred GC16 cleanup.
- [ ] No 1 s stall when resuming after a 2 s pause (cleanup must not run during
      active text entry).
- [ ] Keyboard dismissal and screen change still produce a clean grayscale page.
- [ ] To-dos input has the same behavior.
- [ ] Idle EPDC remains ~1/min with no focused text field.

Do not mark typing acceptance PASS until these measurements exist.

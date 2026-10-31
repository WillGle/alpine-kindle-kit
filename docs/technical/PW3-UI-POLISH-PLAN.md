# PW3 shell UI polish — detailed implementation plan

Date: 2026-10-31 (implemented 2026-11-01)  
Status: **Phases 0–7 implemented and host-validated.** Direction from the
1.1–3.12 review is accepted; the audit corrections landed before the screen
work. Device deployment and visual/paint acceptance remain the separate
release gate described in §12.  
Scope: `os/ui`, `theme/tokens.json`, small `os/system` IPC extensions  
Target: Kindle Paperwhite 3, 1072 × 1448, 8 bpp E-ink  
Policy: host simulator and ARM build only in this plan. Physical deployment
and visual acceptance remain a separate, human-controlled step. The plan must
not regress the current `quality` paint policy or the measured PQ results in
`docs/technical/superpowers/plans/2026-10-31-eink-paint-quality.md`.

## Audit baseline — source reality (2026-10-31)

Đo bằng chính source hiện tại, không phải giả định:

| Hạng mục | Hiện tại | Plan yêu cầu |
| --- | --- | --- |
| Host preview | `789` check, `0` fail; heap `173256/262144` (67%), peak `69356` | Giữ/gate; không được tăng fail |
| ARM `gw-shell` | `2,626,560` byte | Ghi delta sau khi thêm font/icon |
| Tokens | `body 28`, `caption 24`, `card_padding 28`, `status_bar_h 88` | `body 32`, `caption 28`, padding 32, status 96 |
| Bottom nav | Chỉ 3 callback `home/books/settings`; mỗi item rộng `PANEL_W/3`; hiện diện cả Network/Apps/Update/Calendar | 5 tab top-level, mỗi item `PANEL_W/5`; secondary không nav |
| Status bar | Label Wi-Fi/battery là label tĩnh, không có target 96 px | Wi-Fi/Light/Lock/battery đều tappable 96 px |
| Home | Còn `Today.`, Quick Access 4 ô, nút lồng `Browse Books`/`New task` | Bỏ hero/Quick Access; card là action duy nhất |
| Apps list | `lv_obj_clean()` rồi rebuild, cap 8 item | 8+ item scroll được; không repaint cả panel |
| Settings | Còn slider, nút Books/Continue, chưa có Battery/Storage/Version/Uptime/Date | Stepper + các row thật; bỏ dead code |
| Calendar | Tiếng Việt, cell 78/70 px, chưa có month nav/due dot | English, cell 96, month nav, due dot |
| To-dos | TSV 5 field; chưa có due/undo | 6 field + due + Undo |
| Network | Dropdown + password; chưa signal/security | List, signal/security, confirm, busy |
| Apps/Update/Lock | Apps có 2 nút đen (Install/Open); Update/Lock 1 nút | Map primary rõ, tối đa 1 primary/màn |
| Font symbols | Thiếu `f02d f073 f14a f023 f012`; `f2ed` đã có | Thêm và verify glyph |
| Repo | `os/ui` untracked, có `gw-shell` 2.4 MB, `host-sim` 5.4 MB, ~49 MB core dump | `.gitignore` + dọn trước commit baseline |
| Paint policy | `quality` default, shell `0163000e…`, 20 GC16/20 scene, ghost 1/5; balanced rejected (màn bể) | Không tạo policy thứ hai; regression gate PQ |

Audit command used:

```sh
./os/ui/host-sim /tmp/opencode/pw3-ui-baseline
# lvgl heap: total=262144 used=173256 max_used=69356 ...
# geometry checks: 789 total, 0 failed
python3 theme/generate.py --check   # theme adapters are up to date
```

Host audit evidence after adding the 8-app/rapid-status checks:
`docs/evidence/pw3-ui-audit-2026-10-31/`.

### Implementation status (2026-11-01)

| Phase | Status | Evidence |
|---|---|---|
| 0 Baseline | **Done** — artifacts ignored, source+evidence committed, tag `ui-polish-baseline`, baseline reproduced 789/0 | `docs/evidence/pw3-ui-polish-2026-11-01/README.md` |
| 1 Design system/kit | **Done** — tokens, 32 px fonts, icon glyphs, ASCII strings, pressed/disabled/busy/confirm/undo/signal | kit preview + `fonts` checks |
| 2 Navigation/Home | **Done** — 5 tabs at 214 px, tappable status targets, no Quick Access/hero | `home`, `home-empty` |
| 3 Settings | **Done** — stepper, real Battery/Storage/Version/Uptime/Date, App updates row | `settings`, `settings-scrolled` |
| 4 To-dos | **Done** — active-only list, history, due dates, delete→Undo, 6-field TSV | `todos*`, `todos` checks |
| 5 Calendar | **Done** — month nav, 96 px cells, dots, due list, English dates | `calendar*` |
| 6 Network + IPC | **Done** — list/signal/security, password, confirm, busy, hex `NET RESULT` | `network*`, `os/system/IPC.md` |
| 7 Audit/build/docs | **Done** — 2497/0 host checks, heap 69.8%, ARM 3,064,832 bytes, QEMU smoke builds all screens | this evidence directory |

The English-only decision (D1) is treated as confirmed by the request to clear
this plan. Device acceptance is **not** claimed: physical visual, ghosting and
paint-quality acceptance remain a separate human-controlled gate.

### Corrections bắt buộc trước khi code

1. **ASCII strings:** dùng `...`, `>`, `|` trong UI copy; `…`, `›`, `·`
   không được nằm trong `gw_ui_strings.h` nếu header phải ASCII.
2. **Modal không flash hai lần:** một GC16 khi mở; đóng bằng partial/action
   update. Không ép GC16 cả open và close.
3. **Primary mapping:** Apps/Update/Lock phải có bảng primary rõ; hiện Apps
   đang có hai nút đen.
4. **Light/Lock:** bỏ Quick Access thì phải có target 96 px trên status bar
   hoặc Home; nav không có Light/Lock.
5. **Lock navigation:** Lock không có Back; D2 chỉ áp cho Network/Apps/Update.
6. **Baseline commit:** ignore binary/core/`__pycache__`; chỉ commit source +
   preview evidence.
7. **Test count:** plan cũ ghi 633; sau audit+test mới hiện tại là **789**.
8. **NET encoding:** `NET RESULT` hiện trả SSID plain; format mới dùng hex.
   Phải test cả plain cũ và hex mới, không chỉ thêm signal/secure.
9. **Font/icon:** verify từng codepoint sau generate; đừng giả định
   FontAwesome có glyph.
10. **English-only:** đây là thay đổi sản phẩm lớn (UI hiện có tiếng Việt);
    cần xác nhận lại trước khi xoá chuỗi tiếng Việt.
11. **Nav không chỉ là callback:** `kit_nav_item` đang đặt item rộng
    `GW_UI_PANEL_W / 3` và offset theo `/3`; đổi sang 5 tab phải sửa cả width,
    offset, font label và underline.
12. **Long list chưa incremental:** `apps_rebuild_list()` gọi `lv_obj_clean()`
    và dựng lại toàn bộ list, cap 8. Host test 8 app chỉ chứng minh render;
    muốn “không repaint cả panel” phải sửa cách cập nhật hoặc ghi rõ giới hạn.


### Source touch points discovered by audit

| File | Phải sửa |
| --- | --- |
| `os/ui/gw_ui_kit.h` | `gw_ui_nav_cbs_t` hiện chỉ có home/books/settings; thêm calendar/todos |
| `os/ui/host_sim.c` | `check_home_geometry` còn `g_hero_title`/`g_quick_tiles`; phải bỏ hoặc thay bằng date/card checks |
| `os/ui/gw-shell.c` | `build_hero`, `build_quick_access`, nested Browse/New task; Settings slider; Calendar VN; Network dropdown; Apps primary; `build_status_bar` labels chưa clickable; `apps_rebuild_list` full rebuild |
| `os/ui/gw_ui_kit.c` | nav item width `/3` + offset; app bar `< Back` text; button chưa có pressed/disabled; thêm title bar, busy/confirm/undo/signal/row-trailing |
| `theme/generate.py` | thêm `space_1..`, `section`, body/caption mới; giữ `--check` |
| `os/ui/fonts/generate-fonts.sh` | thêm `gw_font_32*` và 5 codepoint còn thiếu; verify glyph |
| `os/ui/sim/run-preview.sh` | thêm `apps-long`, các state mới vào danh sách copy/kiểm tra |
| `.gitignore` | binary, `*.core`, `__pycache__`, debug artifacts |

**Go/no-go:** do not start Phase 1 until corrections 1–9 are reflected in the
source/tests and English-only (item 10) is confirmed by the user. Starting
Phase 1 before that risks rework and a paint-policy regression.

> This plan intentionally unfreezes the UI for the approved issues. The
> previous "cosmetic changes are frozen" note in `PW3-UI-KIT.md` remains true
> for unrelated work.

## 0. Locked decisions

| # | Decision |
|---|---|
| D1 | **English-only UI copy.** User content (book titles, tasks, SSIDs) may stay in any language. Vietnamese font coverage is retained. |
| D2 | **One navigation system:** a five-tab bottom bar — Home, Library, Calendar, To-dos, Settings. Secondary screens (Network, Applications, App updates) use Back only and have no bottom bar. **Lock has neither Back nor bottom bar**; it has one Unlock action. |
| D3 | **No Quick Access section.** Destinations are covered by nav, Home cards, and tappable status-bar items. **Light and Lock must remain one-tap**: add 96 px Light and Lock targets to the status bar (or an equally explicit Home header action), because the five-tab nav has neither. |
| D4 | **Maximum one solid-black primary action per screen.** Active tabs and selected days use bold text plus underline/border, not solid fills. |
| D5 | **Home hierarchy is reading-first:** date line, then a large Continue Reading card, then Calendar and To-dos previews. The oversized "Today." headline is removed. |
| D6 | **Type scale:** hero 72, title 44, body 32, section/caption 28, clock 36. This adds 32 px regular and bold fonts. |
| D7 | **Spacing scale:** 8 / 16 / 24 / 32 / 48 px only. |
| D8 | **Icon set:** extend the generated fonts with the missing book, calendar, lock, check-square, signal and trash glyphs; use LVGL symbols for the rest. No ASCII `"< Back"`. |
| D9 | **Destructive actions are recoverable:** confirmations for Disconnect, Forget, Remove application; Undo bar for task deletion. |
| D10 | **Tasks may have an optional due date.** Calendar shows dots and a "Due this day" list from due tasks. General calendar event CRUD is deferred (see open decision). |
| D11 | **Network metadata:** extend `NET RESULT` with signal and security; use `NET STATUS` for SSID/IP. Keep the old response parseable. |
| D12 | **Host preview is the gate:** every phase must pass `run-preview.sh` and the geometry suite before the ARM build. |

## 1. Goals and non-goals

### Goals

- Resolve every issue numbered 1.1–3.12 from the review.
- Make Home clearly reading-first and remove duplicated navigation.
- Make every screen touch-safe (≥ 96 px), label-safe, and English-only.
- Reduce solid-black area, animation, and unnecessary redraws on E-ink.
- Give destructive actions confirmation or Undo.
- Make Calendar and To-dos useful rather than static.
- Keep the typing/EPDC performance work intact.

### Non-goals

- No change to boot, Guardian, power policy, KOReader internals or the OS
  updater.
- No physical device deployment in this plan.
- No general calendar event editor/store in this pass. Calendar integration is
  driven by task due dates plus the existing single system event. See §13.
- No dark mode, themes, animation, shadows, gradients or transparency.

## 2. Phase 0 — baseline and guardrails

Do this before any UI edit.

1. **Clean artifacts before commit.** Add to `.gitignore` at least:
   `os/ui/gw-shell`, `os/ui/gw-shell-debug`, `os/ui/host-sim`,
   `os/ui/qemu_*.core`, `os/ui/**/__pycache__/`. Move the ~49 MB of core dumps
   out of `os/ui` (or delete them after confirming they are not evidence).
2. Commit source + evidence, not binaries: `theme/`, `os/ui/*.c`,
   `os/ui/*.h`, `os/ui/fonts/*.c`, `os/ui/Makefile`, `os/ui/sim/`,
   `docs/evidence/pw3-ui-redesign-2026-10-31/`. Tag it as the rollback
   baseline.
3. Re-run the current host preview and archive the output:
   ```sh
   ./os/ui/sim/run-preview.sh /tmp/opencode/pw3-ui-baseline
   ```
   Current baseline after adding audit checks: **789 checks, 0 failed**, heap
   `173256/262144` (67%), peak `69356`, ARM `gw-shell` `2,626,560` bytes.
4. Reconcile the stale Settings preview: the checked-in `settings.png` shows
   Battery/Date/footer rows that the current source does not create. Decide the
   row list from §4.2, then regenerate the images so evidence matches source.
5. Extend `host_sim.c` with the new check families listed in §10 before
   changing layouts, so regressions fail loudly.
6. Record the current paint policy and shell hash (`quality`, `0163000e…`) and
   keep the PQ regression gate in §12.

Acceptance: clean baseline commit; preview reproduces 789/0; binary/heap
recorded; no binary or core dump staged.

## 3. Phase 1 — design system and shared kit

Files: `theme/tokens.json`, `theme/generate.py`, `os/ui/gw_theme_tokens.h`,
`os/ui/gw_ui_style.h`, `os/ui/gw_ui_kit.c/.h`, `os/ui/gw_fonts.h`,
`os/ui/fonts/generate-fonts.sh`, new `os/ui/gw_ui_strings.h`, new
`os/ui/gw_icons.h`.

### 3.1 Tokens

- `space`: keep `margin 48`, `gap 24`, `section_gap 32`; add
  `space_1 8`, `space_2 16`, `space_3 24`, `space_4 32`, `space_6 48`.
- `card_padding`: 28 → 32.
- `status_bar_h`: 88 → 96.
- `nav_bar_h`: 104 (unchanged).
- `app_bar_h`: 136 (unchanged).
- `touch_min`: 96 (unchanged).
- `type`: `hero 72`, `title 44`, `body 32`, `caption 28`, `section 28`,
  `clock 36`. **Current source is body 28 / caption 24**; add the `section`
  macro to `theme/generate.py` and audit every hard-coded `gw_font_*` use, not
  just the header.
- `radius`: card 10, textarea 4 (unchanged).
- `stroke.hairline`: 1; card border stays 2 px.
- Colours stay as-is for this pass; do not introduce new grays. Audit note:
  the current card fill is `#EEEEEE`; if ghosting increases after the layout
  change, switch cards to white + 2 px border rather than adding another gray.

Run `python3 theme/generate.py` and `python3 theme/generate.py --check`.
Update the KOReader adapter if tokens feed it.

### 3.2 Fonts and icons

- Add `gw_font_32` and `gw_font_32_bold` to `generate-fonts.sh` and
  `gw_fonts.h`.
- Extend the FontAwesome symbol range with:
  - `f02d` book, `f073` calendar, `f14a` check-square, `f023` lock,
    `f012` signal, `f2ed` trash (already present), `f06e` eye,
    `f070` eye-slash, `f053`/`f054` chevrons, `f067`/`f068` plus/minus.
  - Audit fact: current `generate-fonts.sh` has `f2ed` but **not**
    `f02d/f073/f14a/f023/f012`; add them and verify each renders.
- Add `gw_icons.h` with named UTF-8 string constants such as
  `GW_ICON_BOOK`, `GW_ICON_CALENDAR`, `GW_ICON_LOCK`, `GW_ICON_TRASH`,
  `GW_ICON_CHEVRON_RIGHT`, `GW_ICON_EYE`.
- Reuse existing `LV_SYMBOL_*` where it fits (home, settings, wifi, battery,
  check, close, refresh).
- Verify every glyph in the host preview; a missing glyph must fail review.
- Budget: expect roughly +0.4–0.8 MiB in the ARM binary. Record it.

### 3.3 English strings

- Add `gw_ui_strings.h`; replace all inline UI copy in `gw-shell.c` and
  `gw_ui_kit.c` with `GW_STR_*` constants.
- The header must be pure ASCII: use `...`, `>`, `|` instead of `…`, `›`, `·`. Icon glyphs live in `gw_icons.h`, not in the string catalogue.
- Keep user content mocks in Vietnamese; they are not UI copy.
- English date helpers:
  - `en_weekday[7] = Sunday … Saturday`
  - `en_month[12] = January … December`
  - Home: `Saturday, 31 October 2026`
  - Calendar: `October 2026`
  - Week header: `Mon Tue Wed Thu Fri Sat Sun`
- Remove `vi_weekday` / `vi_month` from UI copy; user content still renders with
  the Vietnamese-capable fonts.

### 3.4 Kit primitives

- `gw_ui_kit_page_create` unchanged.
- `gw_ui_kit_title_bar_create(screen, title)` — 136 px, no Back.
- `gw_ui_kit_app_bar_create(screen, title, back_cb)` — 136 px, icon Back,
  used only on secondary screens.
- `gw_ui_kit_bottom_nav_create(screen, active, cbs)` — five equal items,
  icon + label, 104 px, active = bold label + 4 px underline. Add To-dos and
  Calendar callbacks.
- `gw_ui_kit_button_create` — add an optional icon variant; all buttons get a
  pressed style:
  - secondary pressed: white background, 4 px black border, no gray fill;
  - primary pressed: white background, black text, 4 px black border;
  - disabled: muted text + hairline border, no fill;
  - transition time 0. Gray pressed fills are avoided because they leave
    residue on partial updates.
- `gw_ui_kit_checkbox_create` — visible indicator 56 × 56 inside a 96 × 96
  target, 2 px border, 32 px check glyph.
- `gw_ui_kit_busy_set(button, busy, busy_text)` — disables the button, swaps
  the label, restores it afterward. Used by Scan/Connect/Disconnect,
  Install/Remove/Refresh, Scan/Install.
- `gw_ui_kit_confirm_create(title, message, confirm_text, on_confirm)` — a
  full-screen white modal page with 2 px border, 44 px title, 28 px wrapped
  message, Cancel and Confirm buttons 240 × 96. **One GC16 on open only**;
  close through the action update or a partial/REAGL repaint, not a second
  full flash. No scrim, no shadow, no animation. Measure ghosting on close; if
  residue is unacceptable, use one GC16 on close instead of open, never both.
- `gw_ui_kit_undo_create(text, action_text, on_action)` — 976 × 96 strip
  above the nav, 5 s timer, one instance at a time.
- `gw_ui_kit_signal_bars_create(parent, level)` — four rectangles, levels 1–4;
  deterministic and dither-free.
- Row helpers support a trailing control so Settings/Network rows stop being
  re-implemented per screen.

Acceptance: kit compiles; a kit-only preview page renders all states
(primary/secondary/pressed/disabled/busy/checkbox/confirm/undo/signal); all
touch targets ≥ 96; no animation.

## 4. Screen specifications

### 4.1 Home

```
0    status bar 96            clock | Wi-Fi | Light | Lock | battery
96   content (48 px margins)
     date line 36            Saturday, 31 October 2026
     Continue Reading card   976 × 440 (min 360)
     Calendar card           476 × flex   |   To-dos card 476 × flex
1344 bottom nav 104            5 tabs: Home Library Calendar To-dos Settings
1448
```

- Remove `build_hero()`'s "Today." and the entire Quick Access section
  (`make_quick_tile`, `build_quick_access`, `g_quick_tiles`).
- Use a flex column for the content; no fixed `STATUS_BAR_H + 202/550/942`
  coordinates.
- Continue card:
  - eyebrow `CONTINUE READING` 28 bold muted;
  - book title 44–72 bold, wraps to at most two lines; the title is the visual
    hero now;
  - author 32; progress bar 12 px + `42% read` 28;
  - footer `Tap to resume  >`;
  - empty state: `No book open`, `Open Library to start reading.`, book icon;
  - whole card clickable; remove the nested `Browse Books` button.
- Calendar card:
  - eyebrow `CALENDAR`;
  - `31` 72 bold + `Saturday` 32 + `October 2026` 28;
  - one-week strip with today marked by bold + underline;
  - up to two upcoming rows at 72 px each, or `No upcoming events`;
  - footer `View calendar  >`;
  - whole card clickable.
- To-dos card:
  - eyebrow `TO-DOS`; `3 active` 28 muted;
  - up to five active task rows: 28 px checkbox glyph, 32 px text with
    ellipsis, optional due chip 28;
  - `+N more` line; footer `View all  >`;
  - whole card clickable; remove the `New task` button.
- Status bar:
  - height 96;
  - clock 36 bold left;
  - Wi-Fi area: icon + `Will` or `Offline`, 96 px tall, opens Network;
  - battery area: battery icon + `82%`, 96 px tall, opens Settings;
  - **Light** and **Lock** icon targets, each 96 × 96, because Quick Access is
    removed and the nav has neither; Light opens the frontlight stepper, Lock
    opens the lock screen (or a confirm if policy requires it);
  - no literal `"Wi-Fi"` label when connected; show SSID or `Online`.
- Remove all nested clickables; cards are the only action in their bounds.
- Press feedback: card border 2 → 4 px while pressed.

### 4.2 Settings

Top-level tab: title bar only, no Back, bottom nav active on Settings.
Content is vertically scrollable; disable scroll momentum, add 24 px bottom
padding and a 2 px scrollbar only while scrolling.

Rows, all 120 px unless noted:

| Section | Row | Control |
|---|---|---|
| READING | Library / KOReader reader | Open (secondary) |
| DISPLAY | Frontlight / Brightness | − 37% + stepper, 96 px each, plus a 12 px track |
| CONNECTIVITY | Wi-Fi / `Will | 192.168.1.200` or `Not connected` | Open |
| SOFTWARE | Applications / Install, update, remove | Open |
| SOFTWARE | App updates / Signed packages from USB | Open |
| DEVICE | Battery | 82% (72 px info row) |
| DEVICE | Storage | `20.1 GB free` |
| DEVICE | Version | `GleWorks OS 0.1` |
| DEVICE | Uptime | `2d 4h` |
| DEVICE | Date | `Saturday, 31 October 2026` |
| ACTIONS | Lock now | full-width secondary button |

- Remove the duplicate `Continue Reading` button.
- Create Battery/Date rows properly with `settings_info_row`; remove the dead
  `battery_value`, `label` and unused `g_settings_battery_label` pattern by
  assigning the label when the row is built.
- Add `App updates` directly in Settings (2.6). The Apps screen keeps its own
  entry.
- Frontlight:
  - remove the continuous slider; current source uses `g_light_slider` +
    `lv_slider` and `light_slider_cb`, so remove the widget and callback, not
    just hide it;
  - `−` and `+` steps of 10, clamp 0–100;
  - long-press repeats every 300 ms, coalesced to one in-flight `LIGHT SET`;
  - value text updates optimistically; one `LIGHT SET` per step;
  - thin 12 px track shows the value; no knob, no animation.
- Status rows hide themselves when data is unavailable instead of showing `--`.
- Data sources:
  - battery/version from `STATUS` (`version=`, `battery=`);
  - storage from `statvfs()` on `/var/lib/gleworks` (fallback `/`);
  - uptime from `/proc/uptime` (host sim returns a fixed value);
  - date from the English formatter.

### 4.3 To-dos

Top-level tab: title bar only, no Back, bottom nav active on To-dos.

```
title bar 136
input row 112      [ new task textarea ] [ Add ]       Add is the only primary
due row 96         Due: [No date] [Today] [Tomorrow]   status text right
filter row 96      [Active] [History]                  3 active | 1 done
list               rows 112, scrollable
nav 104
```

- Active view: active tasks only. Completed and deleted tasks never appear here.
- History view: done and deleted, newest first, with `DONE` / `DELETED` labels
  and a Restore button 150 × 96 secondary.
- Input:
  - textarea 96 px high, placeholder `New task`;
  - Add primary 160 × 96;
  - due chips are secondary buttons 160 × 96; the selected chip is bold with a
    4 px underline, not a black fill;
  - default due is `No date`; after Add, reset to `No date`.
- List row:
  - checkbox 56 px visible inside a 96 px target;
  - text 32 px, ellipsis, user content may be Vietnamese;
  - due chip 28 px (`Today`, `Tomorrow`, or `12 Nov`);
  - trash icon button 96 × 96, no border; tapping deletes immediately and
    shows Undo;
  - no per-row `Delete` bordered button.
- Undo bar: `Task deleted.` + `Undo`, 5 s, above the nav.
- Empty states:
  - active: `No active tasks. Add one above.`
  - history: `No completed or deleted tasks.`
- Keyboard:
  - 480 px keyboard, list gets 480 px bottom padding while the keyboard is
    open so the last row is never hidden;
  - existing typing/EPDC fast path is retained.
- Long task text is ellipsized; do not grow the row.

### 4.4 Calendar

Top-level tab: title bar only, no Back, bottom nav active on Calendar.

```
title bar 136
month row 96        [prev]  October 2026  [next]
weekday row 48      Mon Tue Wed Thu Fri Sat Sun
grid 6 × 96         day cells, 8 px event dot, today/selected states
detail 200          UPCOMING / DUE THIS DAY, up to 3 rows
nav 104
```

- Month state lives in `g_calendar_year`, `g_calendar_month`,
  `g_calendar_selected_day`.
- Prev/next buttons 96 × 96; moving months updates the existing 42 cells in
  place (no widget rebuild) and preserves selected day when valid, otherwise
  selects day 1, or today when returning to the current month.
- Day cells:
  - use a button-like object with a 96 px minimum height and ~139 px width;
  - date 32 px, centered;
  - event dot 8 × 8 px under the date;
  - today: bold + 2 px bottom bar;
  - selected: 3 px full border; not a solid black fill;
  - out-of-month cells are muted and non-interactive.
- `UPCOMING` / `DUE THIS DAY` lists items for the selected day:
  - tasks with `due == selected day` (checkbox read-only, title, due chip);
  - the existing single system event when it matches today;
  - empty: `No items for this day.`
- Tapping a due-task row opens To-dos. The row is the only click target.
- Month change may cause one full-screen GC16; day selection must be a partial
  update.
- General event creation is deferred (§13).

### 4.5 Network

Secondary screen: Back app bar, no bottom nav.

```
Back bar 136
connection card 220     Connected | Will | 192.168.1.200
                        [signal bars] [Disconnect] [Forget]
networks row 96         NETWORKS                     [Scan]
network list            up to 4 visible rows, scrollable
password row 112        [ password ] [eye]
actions row 96          [Connect]                     one primary
message 28
```

- On entry and after actions, call `NET STATUS`; decode the hex SSID and show
  `Connected | <ssid> | <ip>` or `Not connected`.
- Scan list replaces the dropdown:
  - row 104 px: SSID 32, signal bars + `-62 dBm` 28, lock icon if secured,
    check mark if connected;
  - tap selects; selected row gets a 3 px border;
  - long SSID is ellipsized;
  - empty: `No networks found.` + `Scan`.
- Password row appears only when the selected network is secured and not
  already connected. Placeholder `Password`; eye button toggles visibility;
  validate 8–63 characters with an inline message.
- Connect is the only primary. Scan and Disconnect/Forget are secondary.
- Disconnect and Forget open the confirm modal:
  - `Disconnect Wi-Fi?` / `You will lose network access until you reconnect.`
  - `Forget this network?` / `The saved password will be deleted.`
- Busy states: `Scanning...`, `Connecting...`, `Disconnecting...`; disable the
  related buttons until the response or timeout.
- After Connect, poll `NET STATUS` every 2 s for up to 15 s on this screen only.
- Message line is 28 px and ellipsized, not a technical dump.

### 4.6 Applications, App updates, Lock

Apply the global rules; no layout redesign beyond the agreed items. **Explicit
primary map** (this fixes the current Apps screen, which has two black buttons):

| Screen | Primary | Secondary |
| --- | --- | --- |
| Applications | Install from USB | Open Reader, Refresh, Remove |
| App updates | Install update | Scan USB |
| Lock | Unlock | — |
| Network | Connect | Scan, Disconnect, Forget |
| To-dos | Add | due chips, Active/History, Restore, trash |
| Settings | — (no primary) | Open rows, Lock now |

- English-only strings.
- Back-only navigation, no bottom nav. Lock has no Back.
- Remove-app confirmation modal.
- Busy states for Install/Refresh/Scan/Install update.
- One primary per screen, as mapped above.
- Pressed states and icons.
- Lock screen: English date/time, one primary Unlock.
- `App updates` remains reachable from Applications and now also from Settings.

## 5. Interaction rules

1. **One action per region.** A card that navigates contains no inner buttons.
2. **One primary per screen.** If a screen has a single action, it may be
   primary; otherwise reserve primary for the main forward action. The explicit
   map is in §4.6.
3. **Confirm or Undo.** Disconnect, Forget and Remove confirm; task delete
   offers Undo for 5 s.
4. **Press feedback within one refresh.** Every button/row has a pressed style.
5. **Busy means disabled.** No duplicate IPC requests from repeated taps.
6. **No animation.** All transition/animation times are 0.
7. **Full GC16 on screen transitions, month changes, and one edge of a modal
   lifecycle.** Typing keeps the existing DU/A2 path. A modal must not force
   GC16 on both open and close.
8. **No dead controls.** If a control cannot act, it is not rendered.

## 6. Data and IPC changes

### 6.1 To-do due dates

- `gw_todo_t` gains `char due[11]` (`YYYY-MM-DD`, empty = none).
- TSV format gains a sixth optional field:
  ```
  id <TAB> status <TAB> created <TAB> updated <TAB> due <TAB> text
  ```
  Old five-field lines load with an empty due date. Save always writes six
  fields. Existing data is preserved by the tolerant parser.
- `todo_add()` takes a due string from the selected chip.
- Helpers: `todo_due_today()`, `todo_due_on(y, m, d)`, `todo_due_label()`.
- Host mock gets two tasks due today and one due tomorrow for preview/dots.
- No due-date editing on existing tasks in this pass; note as a follow-up.

### 6.2 Network scan metadata

- Extend `gw_network_scan_result()` to optionally return signal dBm and a
  security flag, parsed from the existing `/run/gw-wifi-scan` columns.
- Current `NET RESULT <n>` returns **plain** `NET SSID <ssid>`; the new response
  is:
  ```
  NET SSID <ssid_hex> <signal_dbm> <secure 0|1>
  ```
  This changes SSID encoding, not just adding two fields. The UI must accept
  both old plain and new hex forms and must reject malformed hex; test both
  directions (old service/new UI and new service/old UI).
- `NET STATUS` already returns a hex SSID in `IPC.md`; decode it once and reuse
  the same helper. Do not conflate `NET RESULT` and `NET STATUS`.
- Define signal mapping now: `>= -55` = 4 bars, `-56..-65` = 3, `-66..-75` = 2,
  `<= -76` = 1, unknown = 0.
- Document the change in `os/system/IPC.md`.
- No change to `NET CONNECT`, `NET DISCONNECT`, `NET FORGET`, `NET STATUS`
  semantics.

### 6.3 Settings facts

- Parse `version=` from `STATUS`.
- Add bounded helpers for storage and uptime; no new long-running processes.
- Host sim returns fixed values for deterministic screenshots.

## 7. English copy catalogue

All strings live in `gw_ui_strings.h`. Minimum set:

- Navigation: `Home`, `Library`, `Calendar`, `To-dos`, `Settings`.
- Common: `Back`, `Cancel`, `Confirm`, `Open`, `View all`, `Today`,
  `Tomorrow`, `No date`, `Undo`.
- Home: `CONTINUE READING`, `No book open`, `Open Library to start reading.`,
  `Unknown author`, `%d%% read`, `Progress unavailable`, `No upcoming events`,
  `View calendar`, `TO-DOS`, `No tasks yet.`, `%d active`, `+%d more`.
- Settings: `Reading`, `Display`, `Frontlight`, `Brightness`, `Connectivity`,
  `Wi-Fi`, `Networks and connection`, `Software`, `Applications`,
  `Install, update, remove`, `App updates`, `Signed packages from USB`,
  `Device`, `Battery`, `Storage`, `Version`, `Uptime`, `Date`, `Lock now`.
- To-dos: `New task`, `Add`, `Due`, `Active`, `History`, `DONE`, `DELETED`,
  `Restore`, `Delete`, `Task deleted.`, `No active tasks. Add one above.`,
  `No completed or deleted tasks.`, `%d active | %d done | %d deleted`,
  `Task added`, `Task limit reached`, `Type a task first`, `Task deleted`.
- Calendar: month names, weekday names, `UPCOMING`, `DUE THIS DAY`,
  `No items for this day.`, `No upcoming events`.
- Network: `Network`, `CONNECTION`, `Connected`, `Not connected`, `Unknown`,
  `NETWORKS`, `Scan`, `Scanning...`, `Connect`, `Connecting...`, `Disconnect`,
  `Disconnecting...`, `Forget`, `Password`, `Show password`, `Hide password`,
  `No networks found.`, `Select a network.`, `Disconnect Wi-Fi?`,
  `You will lose network access until you reconnect.`,
  `Forget this network?`, `The saved password will be deleted.`,
  `Enter a password with 8 to 63 characters.`
- Applications: `Applications`, `INSTALLED`, `No user applications installed`,
  `Install from USB`, `Refresh`, `Remove`, `Remove application?`,
  `Application data will be kept.`, `BUILT-IN`, `Reader`,
  `KOReader - built into this OS`.
- Updates: `App updates`, `SYSTEM`, `USB PACKAGES`, `No signed packages found
  on USB.`, `Scanning USB...`, `Install`.
- Lock: `Unlocked`, `Locked`, `Tap to unlock`.

Date/time are formatted manually so no libc locale is required.

## 8. Implementation phases

| Phase | Work | Main files | Exit criteria |
|---|---|---|---|
| 0 | Baseline commit, preview rebuild, new geometry checks | `docs/`, `host_sim.c` | Baseline archived; checks run |
| 1 | Tokens, fonts/icons, strings, kit primitives, pressed/busy/confirm/undo | `theme/`, `gw_ui_kit.*`, `gw_ui_style.h`, `gw_fonts.h`, fonts | Kit preview passes; glyphs render |
| 2 | Navigation + Home rebuild | `gw-shell.c`, `gw_ui_kit.*`, `host_sim.c` | 5-tab nav (width/offset `/5`), status targets, no Quick Access, reading-first Home; remove `g_hero_title`/`g_quick_tiles` checks |
| 3 | Settings rebuild + status facts | `gw-shell.c` | Battery/Storage/Version/Uptime/Date real; stepper; update row |
| 4 | To-dos fixes, due dates, delete/undo | `gw-shell.c`, `host_sim.c` | Active excludes done; undo works; due stored/loaded; rapid-status no-rewrite check |
| 5 | Calendar month navigation, selection, dots, due list | `gw-shell.c`, `host_sim.c` | Prev/next works; 96 px cells; English dates |
| 6 | Network redesign + IPC metadata | `gw-shell.c`, `gw-system.c`, `gw-network.c`, `gw-ipc.h`, `IPC.md`, `host_sim.c` | List with signal/security; confirm/forget; busy states |
| 7 | Global audit, preview set, ARM build, docs | all above | Full suite green incl 8-app long list; ARM artifact; no device touch |

Suggested order is strict: 1 → 2 → 3 → 4 → 5 → 6 → 7. Phases 3–6 can overlap
only after Phase 1 is stable.

## 9. Traceability matrix

| Review item | Implementation |
|---|---|
| 1.1 Active includes done | §4.3: Active filters only `GW_TODO_ACTIVE`; History owns done/deleted |
| 1.2 Bottom nav on secondary screens | §4.1/§4.2: nav only on five top-level tabs; secondary screens Back only |
| 1.3 Settings/preview mismatch | Phase 0 rebuild + §4.2 real status rows; delete dead code |
| 1.4 Mixed language | §3.3 + §7 English strings |
| 1.5 No confirm/undo | §3.4 confirm/undo + §4.3/§4.5 |
| 2.1 Three nav layers | D2/D3: five-tab nav, no Quick Access, tappable status bar |
| 2.2 Hero priority | D5/§4.1: remove "Today.", book title is the hero |
| 2.3 Fixed card heights / dead space | §4.1 flex column; content-sized cards; widget row grows |
| 2.4 To-dos card preview | §4.1 five tasks, due chips, footer, whole-card action |
| 2.5 Calendar static | §4.4 month nav, selectable 96 px days, dots, due list |
| 2.6 Settings gaps | §4.2 Library, App updates, Battery/Storage/Version/Uptime/Date |
| 2.7 Calendar/To-dos link | §6.1 due dates + §4.4 dots/due list; event CRUD deferred |
| 3.1 Too many black blocks | D4 + §4 active/selected states |
| 3.2 Frontlight slider | §4.2 stepper with −/+ and track |
| 3.3 Per-row Delete | §4.3 trash icon + Undo |
| 3.4 Small checkbox | §3.4 56 px visible / 96 px target |
| 3.5 Calendar cells < 96 | §4.4 96 px cells |
| 3.6 No pressed feedback | §3.4 pressed styles |
| 3.7 No busy/disabled | §3.4 `gw_ui_kit_busy_set` + §4.5/§4.6 |
| 3.8 No icons | §3.2 icon set + §4.1 status bar |
| 3.9 Network weak | §4.5 + §6.2 |
| 3.10 Small type | D6 + §3.1 32 px body/28 px caption |
| 3.11 Nested click targets | §5 rule 1 + §4.1 card cleanup |
| 3.12 Inconsistent spacing | D7 + §3.1 tokens |

## 10. Test plan

### Host preview

Extend `run-preview.sh` and `host_sim.c` to render at least:

- `home`, `home-empty`
- `settings`, `settings-scrolled`
- `calendar`, `calendar-next-month`, `calendar-selected`, `calendar-empty`
- `todos`, `todos-history`, `todos-empty`, `todos-undo`
- `network-connected`, `network-list`, `network-password`,
  `network-confirm`, `network-empty`
- `apps`, `update`, `lock`, `confirm-remove`

Every image must be 1072 × 1448 and grayscale.

### Geometry and behaviour checks

Add these assertions to the **current 789-check** suite (the old plan said
633; source prints 789 today):

- exactly five nav tabs on top-level screens; zero on secondary screens
  (Network/Applications/App updates); Lock has no nav and no Back;
- each nav item is `1072/5` wide (214 px) with correct x offsets, not the
  current `/3` geometry; labels fit at 24/28 px;
- every clickable control >= 96 x 96, including calendar cells, due chips,
  trash icons, status-bar Light/Lock items and eye button;
- no clickable object inside another clickable object;
- at most one primary per screen, using the explicit map in §4.6;
- all labels fit or have an explicit ellipsis mode with usable line height;
- English UI catalogue is ASCII-only, allowing only `gw_icons.h` glyphs; the
  check scans `gw_ui_strings.h`, not user-content mocks or book/task data;
- Home has no `QUICK ACCESS` and no `Today.` label;
- Light and Lock remain reachable in one tap after Quick Access is removed;
- Wi-Fi, Light, Lock and battery each expose a >= 96 x 96 status-bar target;
- 8-app fixture renders 8 rows, scrolls to the last row, and does not rebuild
  the whole panel for a single-row change;
- 10 rapid status updates with unchanged text produce zero label writes;
- Active list contains no done/deleted task (behaviour test);
- History contains done and deleted with correct labels;
- task delete -> Undo restores the task;
- due-date round-trip and old five-field TSV migration;
- calendar month transitions, leap year, Monday-first offset, selected day;
- English date output for a fixed host clock;
- network plain/hex SSID, old/new `NET SSID` formats, signal-bar mapping;
- confirm modal blocks the underlying action until confirmed;
- modal causes one full update per lifecycle, not open+close GC16;
- busy button rejects a second click while pending;
- typing path still uses A2/DU and no full-screen promotion;
- `quality` scene sends one submit; balanced/fast are not release policies.

### Build and runtime

- `make -C os/ui check`
- `./os/ui/sim/run-preview.sh`
- `./os/ui/build-arm.sh`
- QEMU smoke: shell starts, screens build, no crash; no `/dev/fb0` write on
  host.
- LVGL heap peak < 70 % of 256 KiB after all screens and states are built.
- ARM binary size delta recorded; expected +0.5–1.0 MiB from the new fonts.

### Performance

- No animation, shadow, gradient or outline on new components.
- Month change and screen change: one GC16.
- Day selection, checkbox, undo bar, pressed state: partial only.
- Typing keeps the current DU/A2 policy; do not change input timing.
- No new 30 s or faster background timer except the bounded 2 s network
  status poll while connecting (≤ 15 s).

## 11. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Font regeneration adds size / missing glyphs | Regenerate deterministically, verify every glyph in preview, record size delta |
| Flex layout behaves differently from fixed coordinates | Build with host sim first; keep deterministic 1072×1448 comparisons |
| Settings content no longer fits | Scrollable content with momentum disabled; verify last row reachable |
| Modal/undo causes E-ink ghosting | One GC16 per modal lifecycle, not open+close; undo strip uses a small partial region; measure |
| Paint policy regression | Keep `quality` only (balanced/fast rejected); run PQ regression; one submit/scene |
| IPC extension breaks older shell/system pairing | UI accepts old plain and new hex `NET SSID`; update service+UI in one release |
| TSV migration loses tasks | Tolerant six-field parser; keep five-field fixtures; test round-trip |
| Five tabs crowd labels | Nav is full width (1072/5 = 214 px/tab); validate 24/28 px labels and ellipsis |
| English text changes widths | Re-run label-fit checks; English is shorter than Vietnamese in most cases |
| English-only is a product change | Confirm with user before deleting Vietnamese strings |
| Font/icon glyph missing | Extend symbol list; verify each glyph visually; record size delta |
| Apps has two primary buttons today | Apply the explicit primary map in §4.6 |
| Quick Access removal loses Light/Lock | Add 96 px status-bar Light/Lock targets |
| Binary/core dumps committed | `.gitignore` + clean before baseline commit |
| Scope creep from event CRUD/due editing | Explicit non-goals; defer to a later plan |
| UI tree is untracked | Phase 0 commit/tag source+evidence, not artifacts |

## 12. Rollout and rollback

1. Clean artifacts, commit baseline source+evidence, and tag it.
2. Land phases as separate commits with preview evidence.
3. Keep the previous ARM binary on the host and on device until acceptance.
4. Deploy only after the host suite and ARM build pass.
5. Run the paint-quality regression on the device: `quality` one submit/scene,
   no second flash, ghost not worse than the accepted 1/5; then check the
   manual UI checklist.
6. Rollback: restore the previous `gw-shell` binary and the tagged UI commit.

Phases 0–7 are host-only. The device step 5 above is a **separate release
gate** after Phase 7, not part of those phases. Because UI changes alter
redraw regions, the paint-quality gate from
`2026-10-31-eink-paint-quality.md` is mandatory before release, not optional.

## 13. Open decisions

**English-only UI copy.** Current UI contains Vietnamese strings. D1 removes
them; this is a product decision, not a technical fix. Confirm before Phase 1
so the string extraction is not done twice. User content remains multilingual.

**Calendar event CRUD.** The review noted that events cannot be created. This
plan links Calendar to tasks with due dates and keeps the existing single
system event, but does not add a general event store/editor. A full event
feature needs:

- a new `/var/lib/gleworks/events.tsv` store;
- an event editor (date picker, time, title, keyboard);
- add/edit/delete and recurrence rules;
- its own migration and tests.

Estimated additional effort: 3–5 developer days. Recommended: defer until
after this UI pass; ship due-date integration first. If event CRUD is required
in the same release, add it as Phase 6B before the final audit.

## 14. Estimated effort

| Phase | Estimate |
|---|---:|
| 0 Baseline | 0.5 day |
| 1 Design system/kit | 2 days |
| 2 Navigation/Home | 1.5 days |
| 3 Settings | 1 day |
| 4 To-dos | 1.5 days |
| 5 Calendar | 2 days |
| 6 Network + IPC | 2 days |
| 7 Audit/build/docs + paint regression | 2 days |
| **Total** | **~13 developer days** |

Estimates assume host-simulator iteration and no physical device debugging.

## 15. Definition of done

- Clean baseline commit exists; no binary, core dump or `__pycache__` staged.
- Host preview passes with **789+ checks, 0 failed**; new check families in §10
  are present.
- Kit preview proves primary/secondary/pressed/disabled/busy/checkbox/confirm/
  undo/signal states and every new glyph.
- All UI copy is English ASCII except `gw_icons.h`; user content remains
  multilingual.
- Top-level screens have the five-tab nav; Network/Applications/App updates
  have no nav; Lock has no nav and no Back.
- Every clickable target is at least 96 × 96; Home has no Quick Access/Today;
  Light and Lock remain one tap.
- Apps/Update/Lock follow the explicit primary map in §4.6.
- One GC16 per scene in `quality`; typing uses A2/DU with no full flash; modal
  uses one full update per lifecycle.
- ARM build passes, heap peak < 70 % of 256 KiB, binary size delta recorded.
- `CURRENT.vi.md`, `ACCEPTANCE.json`, `BACKLOG.md` updated; paint-quality and
  device visual acceptance are separate release gates.

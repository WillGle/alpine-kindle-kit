---
status: historical
date: 2026-10-31
superseded_by: ../../status/CURRENT.md
---

# Stage 0 and Stage 1 — Home layout and touch usability

Date: 2026-10-31  
Device: Kindle Paperwhite 3, boot `bedac7f1-b60c-40e3-93ce-3afee5fa0f93` (after final reboot)  
Scope: Stage 0 inspection/baseline and Stage 1 Home layout + touch usability only.

## Stage 0 — inspection and baseline

### Architecture inspected
- `os/ui/gw-shell.c`: Home, status bar, calendar/notes cards, Reader/Lock buttons, bottom navigation, menu/Wi-Fi panels, lock screen.
- `os/ui/lv_port_epdc.h`: PW3 geometry 1072x1448, pitch 1088, 8 bpp; 32-row draw buffer; dirty-rect coalescing; full GC16 cleanup after 60 partials.
- `os/ui/lv_port_input.h`: evdev node scan for `ABS_MT_POSITION_X`; touch forwarded only when `GW_UI_TOUCH_ACCEPTED` or `GW_UI_TOUCH_SPIKE`.
- `os/system/gw-system.c`: supervises `gw-shell`; shell restart policy has one automatic restart, then USB console remains available.
- Deployed flags observed on device: `GW_UI_EPDC_ACCEPTED=0`, `GW_UI_TOUCH_ACCEPTED=0`, `GW_UI_EPDC_SPIKE=1`, `GW_UI_TOUCH_SPIKE=1`, `GW_UI_LIFECYCLE_SPIKE=1`, `GW_UI_PERF=1`.

### Baseline measurements
- Baseline framebuffer capture: `/tmp/opencode/pw3-home-baseline.png` (1072x1448, 8 bpp).
- First full render after boot used GC16 full-screen update: `marker=1 waveform=2 mode=1 area=1552256 submit_us=1596`.
- Draw buffer is 32 rows; initial paint required 46 flush bands (45 x 32 rows + 8 rows), each copy about 840-917 us, about 40 ms total framebuffer copy before EPDC submission.
- Steady-state idle render handler time is about 9-22 us after first paint.
- Touch node detected as `/dev/input/event1` (`cyttsp4_mt`).

### Identified layout/touch issues
- Status bar was only 72 px and menu button was 60x48, below the 96 px touch target.
- Hero heading used 24 px left margin while cards used 24 px, but spacing and card geometry were inconsistent.
- Calendar card used `LV_HOR_RES - 48` with 24 px margins; notes card at fixed y=400 overlapped/underused vertical space depending on card heights.
- Week strip used `LV_HOR_RES - 80` and mixed 12 px offsets, causing uneven column widths.
- Notes card had only a `NOTES` label and no preview or empty state.
- Reader and Lock controls were small (120x80 and 180x80); Lock was placed at top right and overlapped the hero area.
- There were only two primary controls, not four; Settings access was hidden in the small menu button.
- Vietnamese/long text clipping risk in the 160 px notes card and 8-row week strip labels.

## Stage 1 — changes implemented

### Files changed
- `os/ui/gw_ui_style.h` (new): reusable LVGL style constants and card/panel helpers.
  - `GW_UI_MARGIN`, `GW_UI_GAP`, `GW_UI_SECTION_GAP`, `GW_UI_CARD_PADDING`, `GW_UI_TOUCH_MIN`
  - `GW_UI_STATUS_BAR_H`, `GW_UI_NAV_BAR_H`, `GW_UI_SHORTCUT_W`, `GW_UI_SHORTCUT_H`
  - `gw_ui_style_card()`, `gw_ui_style_plain_panel()`
- `os/ui/gw-shell.c`: Home layout rewrite and touch-target sizing.
  - Status bar height increased to 80 px; clock/status/Wi-Fi positions adjusted; menu button increased to 72x64.
  - Hero heading and subtitle use 48 px margin and 8 px spacing system.
  - Calendar card: 976x232, 48 px margins, `gw_ui_style_card`, date line via `strftime`, week strip with equal columns.
  - Notes card: 976x176, 48 px margins, shows saved note preview or `No notes yet.` empty state.
  - Four primary shortcuts: Reader, Library, Lock, Settings, each 224x96, with 24 px gaps.
  - Lock no longer overlaps the hero area.
  - Bottom navigation bar retained.
  - Non-lifecycle build path retained.

### Existing features preserved
- Status IPC, clock update, Wi-Fi label, menu panel, light slider, Wi-Fi panel, lock/unlock, calendar panel option, reader launch, bottom navigation.
- EPDC/touch flags and ports unchanged.
- No kernel, Guardian, rescue or boot changes.

### Build
Toolchain:
- Compiler: `work/toolchains/toolchain-gcc-4.9.4/.../arm-linux-gnueabi-gcc`
- Musl prefix: `work/toolchains/musl-arm-prefix`
- Linux headers: `/nix/store/0xlb57h8i6jjzx4k44z363ig29yxkwya-linux-headers-6.18.7/include`
- LVGL: `work/sources/lvgl` (8.3.11)

Final binary:
- Path: `os/ui/gw-shell`
- Size: 426984 bytes
- SHA256: `92c5eb3e92707f1eab2b6e6acbd554a2b378b6cad594275aeb1d0579cb2376a5`
- Flags: `GW_UI_LIFECYCLE_SPIKE=1`, `GW_UI_EPDC_SPIKE=1`, `GW_UI_TOUCH_SPIKE=1`, `GW_UI_EPDC_ACCEPTED=0`, `GW_UI_TOUCH_ACCEPTED=0`, `GW_UI_CALENDAR_SPIKE=0`
- Target: ARMv7 soft-float, static musl.

Build command used:
```sh
make clean && make
```
with the environment variables above and `CFLAGS` including `-marm -march=armv7-a -mfloat-abi=soft -nostdinc -isystem musl/include -idirafter linux-headers -L musl/lib -B musl/lib -static`.

### Automated/host tests
- `make` (default flags): PASS, warnings only from LVGL and pre-existing unused helper functions.
- `make` (lifecycle/EPDC/touch spike flags): PASS.
- QEMU smoke test: binary starts, prints `GW_UI_TRIAL_FLAGS=1`, registers touch node `/dev/input/event1`, initializes EPDC port, enters render loop. Full host run cannot open `/dev/fb0`; not a code failure.
- Syntax check: PASS.

### Device test
- Original binary backed up on device as `/usr/sbin/gw-shell.before-stage1-layout`.
- New binary uploaded and verified by SHA256.
- Device rebooted successfully; `gw-shell` process 935 running under `gw-system --supervise`.
- Framebuffer captured after reboot: `/tmp/opencode/pw3-home-stage1-final.png` (1072x1448, 8 bpp).
- No reboot loop or rescue entry observed; USB console remained available.

### Failures / limitations
- `LV_SYMBOL_LIST`, `LV_SYMBOL_POWER` and `LV_SYMBOL_SETTINGS` compile successfully, but their visual glyphs were not independently verified on the physical panel beyond framebuffer capture.
- Touch behavior was not manually exercised in this stage; touch acceptance remains 0 and the deployed spike path is unchanged.
- Notes preview reads `/root/note.txt` synchronously during Home construction. This is acceptable for the current init path but should move to a bounded/asynchronous load if notes grow.
- The menu panel still contains the 180x40 Wi-Fi button and 180x20 slider; these are below the 96 px primary target but are secondary controls. They should be revisited in Stage 3.
- No visual review by a human of the physical panel was performed; framebuffer capture only.
- Stage 2 (input/render/EPDC optimization) and Stage 3 (time, frontlight, Wi-Fi) are not addressed.

### Remaining verification
- Human confirms the new Home layout on the physical panel: no clipped labels, no overlapping text, readable in bright and low light.
- Tap each of the four 224x96 shortcuts and confirm correct behavior and no missed touches.
- Confirm calendar date and week strip render correctly, including today highlight.
- Confirm notes empty state and a saved note preview.
- Run repeated tap latency measurements against the Stage 0 baseline.
- Keep previous binary (`gw-shell.before-stage1-layout`) for rollback until acceptance.

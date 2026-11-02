---
status: historical
date: 2026-11-02
superseded_by: ../../status/CURRENT.md
---

# PW3 Integration and Hardware Acceptance Report (2026-10-31)

## 1. Deployed binaries and flags

- Source revision: working tree based on git HEAD `13272ca` (uncommitted changes included).
- Compiler/target: musl (Alpine armv7, soft-float) via clang + alpine ld.
- `/usr/sbin/gw-shell`: SHA256 `86b4328d075a115c083104ef95e93baeeee3137551a57eaba6944d4918b36b7e`
  - Runtime startup: `GW_UI_TRIAL_FLAGS=1`
  - `GW_SHELL: starting (EPDC_accepted=0 touch_accepted=0)`
  - `GW_UI: EPDC port init 1072x1448 pitch=1088 accepted=0`
  - `GW_UI: touch node /dev/input/event1`
  - Effective flags: `GW_UI_EPDC_SPIKE=1`, `GW_UI_TOUCH_SPIKE=1`, `GW_UI_LIFECYCLE_SPIKE=1`, `GW_UI_PERF=1`, `GW_UI_EINK_FAST_MODE=0`
- `/usr/sbin/gw-system`: SHA256 `7958bfd74e7585d7022d0f4b13d5e8d6591d8e451e3d10978f731aa8c3b67723`
  - `GW_POWER_LIGHT_SPIKE=1`, `GW_WIFI_SPIKE=1`, `GW_LIFECYCLE_SPIKE=1`
- `/usr/sbin/gwctl`: SHA256 `7361e6a1c1e6daf064bafa8fb2ad107f73fb7c96ddbe41940dad43f6b0d03c82`
- Service status after deploy: `gw-system [started]`, `gwctl status` returns OK.

## 2. Screenshots

- Home framebuffer captured: `/tmp/opencode/private/screens/screen-1a71eec1a7d0.png` (1072x1448, 8bpp converted to PNG).
- Settings, Calendar, Notes, and Wi-Fi configuration screens were not captured because no touch-injection path exists on the device (no `/dev/uinput`, shell holds exclusive evdev grab).
- Visual verification of those screens remains pending.

## 3. Touch benchmark

- Host idle render benchmark: median 10 us, P95 19 us (n=1194).
- Device log metrics: render median 14 us / P95 20 us; flush copy median 104.5 us / P95 130 us; EPDC submit median 1574 us / P95 1578.8 us; EPDC complete median 12 us / P95 13 us.
- `input_to_render_us` and `input_to_submit_us` are logged in device logs.
- The 12 us EPDC completion value is the time for `MXCFB_WAIT_FOR_UPDATE_COMPLETE` to return after the marker was already complete. It does **not** mean the E-ink refresh finishes in 12 us.
- Physical touch-to-visible latency was not measured (no slow-motion capture / no touch injection).

## 4. System control results

- Light: get/set works; min/max not exhaustively exercised; hardware response observed via gwctl.
- Time: timezone set/get works (`Asia/Ho_Chi_Minh`, `UTC`); time-sync OK; manual `time-set` expects epoch seconds.
- Wi-Fi: enable/disable and state transitions work (`OFF` -> `CONNECTING` -> `OFF`); scan/association/DHCP were not exercised in this pass.

## 5. Pass/fail

- Build/integration deploy: PASS (musl binaries deployed and service restarted).
- Service startup: PASS.
- gwctl control path: PASS.
- Home framebuffer capture: PASS.
- Settings/Calendar/Notes/Wi-Fi visual inspection: NOT TESTED.
- Physical touch latency: NOT TESTED.
- Wi-Fi scan/association/DHCP/DNS/persistence: NOT TESTED.
- Ghosting/text quality: NOT TESTED (fast mode disabled).

## 6. Regressions / unresolved

- Touch injection unavailable; UI navigation cannot be automated.
- EPDC accepted remains 0 (spike path active); no acceptance claim for production EPDC.
- Fast mode disabled; ghosting not evaluated.
- Wi-Fi connection flow not validated end-to-end.

## 7. Diagnostic build (2026-10-31)

- `/usr/sbin/gw-shell`: SHA256 `af52068780d03b60989b030170374b4b734b6806c1777856dfd353641fb83f26`
  - Added `GW_UI_DIAG=1`: local-only Unix socket `/run/gw-shell.diag` (mode 0600, root-only via SO_PEERCRED).
  - Commands: `NAV HOME|SETTINGS|CALENDAR|NOTES|WIFI`, `TAP x y`.
  - All LVGL operations execute in the render loop (UI thread).
- `/usr/sbin/gwctl`: SHA256 `6a79af5f9ac02acef25af16e3eee58ba8d4db3aab3e7b4bbcd4829e25e3e932b`
  - Added `ui-nav <screen>` and `ui-tap <x> <y>`.

Screenshots captured:
- Home: `/tmp/opencode/private/screens/screen-ae409067c71c.png`
- Settings/menu: `/tmp/opencode/private/screens/screen-d2260e62118f.png`
- Calendar: `/tmp/opencode/private/screens/screen-c6a6e5c28b5a.png`
- Notes: `/tmp/opencode/private/screens/screen-851732741350.png`
- Wi-Fi: `/tmp/opencode/private/screens/screen-028c4965d8ad.png`

## 8. Rollback

Restore previous musl binaries:
```
cp -a /usr/sbin/gw-shell.before-diag /usr/sbin/gw-shell
cp -a /usr/sbin/gwctl.before-diag /usr/sbin/gwctl
/etc/init.d/gw-system restart
```
Earlier backups (`.before-musl`, `.before-stage2`, etc.) remain available.
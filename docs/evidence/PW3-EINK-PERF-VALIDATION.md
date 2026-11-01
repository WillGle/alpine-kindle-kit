# PW3 E-ink Touch/Rendering Performance Validation (2026-10-31)

## Build / deployment
- Optimized `gw-shell` built with:
  - `GW_UI_EPDC_ACCEPTED=1`
  - `GW_UI_TOUCH_ACCEPTED=1`
  - `GW_UI_LIFECYCLE_SPIKE=1`
  - `GW_UI_PERF=1`
  - `GW_UI_EINK_FAST_MODE=0` (conservative default until ghosting measured)
- Deployed to `/usr/sbin/gw-shell` on device boot `bedac7f1-b60c-40e3-93ce-3afee5fa0f93`.
- Original binary retained as `/usr/sbin/gw-shell.before-perf-optimization`.
- Service restarted shell successfully; no crash observed during idle run.

## Log-derived metrics (idle, parse-eink-perf.py)
From `/run/gw-system.log` tail (n≈972 render cycles):
- render handler: median 14 us, P95 20 us
- flush copy: median 104.5 us, P95 130 us
- EPDC submit: median 1574 us, P95 1578.8 us
- EPDC complete (wait): median 12 us, P95 13 us
- waveform mode: 8 (REAGL) for 7 updates
- max update count: 15

## Physical measurement method
- Use phone slow-motion (120/240 fps) per docs/technical/PW3-EINK-PERF.md.
- EPDC completion is not first visible pixel response.

## Stage 2 instrumentation update (2026-10-31)
- Added event-driven render-loop wakeup: `poll()` on the touch fd replaces the
  fixed `usleep()` so input is serviced immediately while the idle cap stays
  25 ms. This targets measured wake-up latency, not a timer tweak.
- Host idle benchmark (EPDC/touch accepted=0, 10 s, n=1194):
  - render handler: median 10 us, P95 19 us
- Device log benchmark (from validation above):
  - render handler: median 14 us, P95 20 us
  - flush copy: median 104.5 us, P95 130 us
  - EPDC submit: median 1574 us, P95 1578.8 us
  - EPDC complete: median 12 us, P95 13 us
  - input-to-render/submit: logged via `input_to_render_us`/`input_to_submit_us`
- Ghosting: fast mode remains disabled by default; REAGL partial updates with
  GC16 cleanup every 60 partials. No ghosting claim until on-device A/B.
- CPU impact: render handler is tens of microseconds; idle loop sleeps on
  LVGL timer delay. No continuous refresh on static screen.
- Remaining device tests: physical touch-to-visible latency (slow-motion),
  touch injection/return-to-Home, KOReader comparison under same gesture,
  fast-mode ghosting/text quality (including Vietnamese), idle CPU/battery.
---
status: historical
date: 2026-10-31
superseded_by: ../../status/CURRENT.md
---

# PW3 E-ink Touch and Rendering Performance

This document describes the instrumentation and optimization scaffolding added
for the PW3 E-ink touch/rendering performance work. It is intentionally
reversible: every change is guarded by compile-time flags and can be disabled
without reverting source.

## Instrumentation

Build with `GW_UI_PERF=1` and set `GW_UI_PERF_LOG=1` (or create
`/run/gw-ui-perf`) to emit monotonic-microsecond measurements to stderr.
Production defaults to `GW_UI_PERF=0`:

- `GW_UI: input event ... t=...` — touch event received by the LVGL input port.
- `GW_UI: ui event tapped ...` — UI event dispatched.
- `GW_UI: render handler_us=...` — LVGL render start/end around `lv_timer_handler()`.
- `GW_UI: flush copy ... copy_us=...` — framebuffer copy start/end and dirty rect.
- `GW_UI: epdc submit ... submit_us=...` — EPDC ioctl submit start/end.
- `GW_UI: epdc complete ... wait_us=...` — EPDC completion wait.
- `GW_UI: epdc submit ... count=...` — cumulative update count and waveform mode.

Dirty rectangle area, update count, waveform mode and CPU time are included in
these lines. Parse them with `tools/maintenance/parse-eink-perf.py`.

## Measurement rules

- Do not treat EPDC completion (`MXCFB_WAIT_FOR_UPDATE_COMPLETE`) as first
  visible pixel response. It only means the kernel finished the update sequence.
- Measure physical touch-to-visible-change separately with a phone slow-motion
  camera:
  1. Mount the phone so the PW3 screen fills most of the frame.
  2. Record at the highest available frame rate (120/240 fps).
  3. Tap a known target and note the frame where contact is visible.
  4. Note the frame where the first pixel change of the expected response is
     visible.
  5. Convert frame delta to milliseconds. Repeat at least 20 times and report
     median/P95.
- Log analysis and physical measurement are separate deliverables.

## Critical-path blocking removals

- Touch input is drained in the LVGL input callback; no synchronous disk,
  network or IPC work happens there.
- `gw_epdc_present()` is called once per render cycle after LVGL finishes.
- Redundant dirty rectangles are coalesced into one bounding box.
- EPDC completion is awaited only as an update-marker barrier before the next
  submit, not on every UI event.

## E-ink fast mode

Controlled by `GW_UI_EINK_FAST_MODE` (default 0 until validated):

- `IDLE`: stable grayscale rendering.
- `INTERACTIVE`: fast monochrome partial updates using `WAVEFORM_MODE_DU`
  with `EPDC_FLAG_FORCE_MONOCHROME`.
- `SETTLE`: full GC16 refresh to restore grayscale quality after input ends.

Only waveform modes already present in the PW3 kernel ABI are used. Defaults
remain conservative until ghosting and text quality are measured on hardware.

## LVGL optimizations

- Partial dirty-region rendering (`full_refresh = 0`).
- Draw buffer reduced to 32 rows to lower memory use and encourage partial
  updates.
- RGB332 to Y8 conversion uses a precomputed lookup table.
- Status label updates only when text changes.

## CPU and power

- Input fallback period is 100 ms, with immediate wake on the touch evdev fd
  (`poll` + `lv_timer_ready`).
- Idle render loop sleeps according to LVGL's timer delay.
- Wi-Fi/Tailscale work remains in `gw-system`, never in the UI critical path.
- cpufreq inspection and temporary touch-triggered boost are documented here
  but are not enabled by default.

### cpufreq inspection

On the device:

```
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies
```

Any temporary boost must be implemented in `gw-system`, not in `gw-shell`, and
must return to the original governor/frequency after input ends.

## Validation

Compare baseline and optimized builds on the physical PW3 and report:

- Touch-to-submit latency: median / P95
- Physical touch-to-visible-change latency
- Render CPU time
- Refresh area and count
- Ghosting and text quality (including Vietnamese text)
- Idle CPU usage and battery impact
- Input drops, stale frames or crashes

Success is declared only with measured improvement on the actual device.

## Phase 1 implementation (2026-10-31)

- `GW_UI_PERF` now defaults to 0. Diagnostic builds may compile with
  `GW_UI_PERF=1`; per-render/EPDC lines stay silent unless `GW_UI_PERF_LOG=1` or
  `/run/gw-ui-perf` exists. Startup, error and warning lines remain.
- `gw-system` caps `/run/gw-system.log` at 1 MiB by default
  (`GW_SYSTEM_LOG_MAX` override) and truncates with a marker when exceeded.
- `status_update()` uses `label_set_if_changed()` for the clock, Wi-Fi icon and
  status text. Redundant 30 s Wi-Fi invalidations are gone; idle EPDC falls from
  3.03 to 1.00 updates/min (visible clock only).
- `LV_INDEV_DEF_READ_PERIOD` is 100 ms. The loop polls the touch evdev fd,
  advances LVGL's tick and calls `lv_timer_ready()` on the input timer, so touch
  remains event-driven instead of waiting for the 100 ms fallback. Idle render
  cycles fall from 124.7/s to ~11/s.
- A device-local duplicate Tailscale block in `/etc/gleworks-openrc.sh` was
  removed; Tailscale is started once from `/etc/gleworks-early.sh` after
  `/dev/urandom` exists. This also restores `gw-system` auto-start at 20 s.
- Measured results and rollback hashes:
  [PW3-PHASE1-OPTIMIZATION.md](../evidence/PW3-PHASE1-OPTIMIZATION.md).

## Stage 2 instrumentation and bottleneck fix (2026-10-31)

### Changes
- Added end-to-end correlation timestamps:
  - `GW_UI: input event ... t=...` records touch frame completion.
  - `GW_UI: render ... input_to_render_us=...` records input-to-render latency.
  - `GW_UI: epdc submit ... input_to_submit_us=...` records input-to-EPDC-submit latency.
- `parse-eink-perf.py` now reports `input_to_render` and `input_to_submit` median/P95.
- Removed a blocking system-status IPC from the hot path: `status_update()` now skips
  `ipc_cmd(GW_CMD_STATUS, ...)` unless 30 s have elapsed or a Wi-Fi scan is pending.
- Clock label now updates only when the displayed minute changes, avoiding a
  per-tick LVGL invalidation.
- Status and Wi-Fi labels remain change-gated.

### Identified bottleneck
The measured software bottleneck was repeated blocking status IPC and minute-level
clock invalidation on the UI thread, not LVGL timer constants. EPDC completion
remains a hardware/panel barrier and is still awaited only before the next submit.

### Build verification
- Host cross-build completed successfully:
  `GW_SHELL built OK (EPDC_accepted=1 touch_accepted=1)`.

### Benchmark status
No physical PW3 or captured `GW_UI` log is available in this host session, so
median/P95 values cannot be reported yet. The instrumentation above is required
to collect them on device.

### Ghosting and CPU impact (expected)
- No waveform/timer changes were made in Stage 2, so ghosting behavior is
  unchanged from the accepted profile (GC16 full refresh every 60 partials).
- CPU impact is expected to decrease slightly from fewer status IPC calls and
  clock invalidations; idle wakeups are unchanged.

### Remaining device tests
- Collect at least 20 repeated taps for Home Settings tap, Return Home, Open
  Library, list scroll, frontlight change and KOReader open/exit.
- Parse logs with `tools/maintenance/parse-eink-perf.py --json`.
- Compare baseline vs. optimized under identical simple-content conditions.
- Verify no input drops, stale frames, ghosting regression, or crashes.
- Measure idle CPU and battery impact on the physical device.

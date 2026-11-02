---
status: historical
date: 2026-11-02
superseded_by: ../../status/CURRENT.md
---

# PW3 Alpine — Phase 1 Optimization Report

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
OS: Alpine 3.24.3, musl, kernel `3.0.35-lab126`  
Final boot ID: `dffec059-64f9-46dc-93f6-82833658ba24`  
Access: physical USB CDC-ACM console (`0525:a4a7`)  
Scope: P0–P5, no bootloader/kernel/Guardian/recovery-partition changes.

**Result: P0, P1 and P2 pass on the physical device.** Idle log growth is
eliminated, EPDC refresh is cut to the visible clock rate, and idle CPU /
wakeups are sharply reduced while touch input remains event-driven. P3 fixes
Tailscale startup and the pre-existing service auto-start failure. P4 was an
investigation only. P5 was inconclusive for power and the baseline governor was
restored.

## 1. Summary of measured result

| Metric | Phase 0 baseline | Phase 1 final | Change |
|---|---:|---:|---:|
| `gw-shell` CPU | 1.40 % | **0.10 %** | **−92.9 %** |
| System busy CPU | 4.93 % | **0.28 %** | **−94.3 %** |
| Context switches | 277.8 /s | **64.4 /s** (180 s) | **−76.8 %** |
| Total interrupts | 306.0 /s | **114.1 /s** (60 s) | **−62.7 %** |
| Timer-tick IRQ | 223.5 /s | **40.1 /s** | **−82.1 %** |
| Render-loop cycles | 124.7 /s | **10.98 /s** (diag) | **−91.2 %** |
| Idle EPDC updates | 3.03 /min | **1.00 /min** | **−67.0 %** |
| GW_UI log growth | 10,625 B/s | **0 B/s** | **−100 %** |
| Root filesystem used | 210,764 kB (83 %) | **193,662 kB (76 %)** | **−17,102 kB** |
| `Shmem` (tmpfs) | 18,208 kB | **3,072 kB** | **−15,136 kB** |
| Boot-to-service | 1,368 s, manual | **20 s, automatic** | reliability fix |

The final idle EPDC rate is exactly one update per minute, i.e. the visible
minute clock. Clock updates are allowed by the Phase 1 requirements.

## 2. Changes by priority

### P0 — bounded logging and shell history

**Root cause (MEASURED).** The deployed `gw-shell` was built with
`GW_UI_PERF=1` and wrote one `render handler` line per render cycle to
`/run/gw-system.log`. At idle this produced 10.6–11.4 kB/s and 28.4 MB in
47 minutes. The file had no rotation and consumed tmpfs 1:1.

**Change.**
- `GW_UI_PERF` now defaults to **0**.
- A diagnostic build (`GW_UI_PERF=1`) gates every high-frequency line through
  `GW_PERF_LOG`, which is off unless `GW_UI_PERF_LOG=1` or `/run/gw-ui-perf`
  exists. Errors/warnings and one-time startup lines remain unconditional.
- `gw-system` opens its log with `O_APPEND`, enforces a default **1 MiB** cap
  (`GW_SYSTEM_LOG_MAX` override, 60 s check interval), and truncates with a
  single marker when exceeded.
- `/etc/gleworks-console.sh` now sets `HISTFILE=/run/gw-console-history` and
  `HISTSIZE=200`; interactive history no longer writes the ext3 root.
- The pre-existing `/.ash_history` was recorded (`5,544,907 B`, 9,677 lines,
  SHA-256 `19da8e01424c6f9cdf58d80e3c24fd3b6e37fde81e412c609aa2b953948ccf63`),
  confirmed to be only interactive shell history, then removed. No system
  evidence was stored there.

**Result (MEASURED, 90.09 s quiet window).**
- Log 702 B → 702 B: **0 B/s**; `/run` 124 kB at that point.
- `gw-shell` 1.40 % → **0.71 %**; total idle 99.1 %.
- A later 60 s final check: log **276,198 B → 276,198 B, 0 B/s**.

**Status: PASS.** Rollback scripts are the `.before-phase1-p0` files.

### P1 — redundant E-ink refresh

**Root cause (MEASURED + source).** With no input there were 12 EPDC submits in
237.6 s (3.03/min), 8 status IPC calls and 4 clock-minute changes. `status_update`
called `lv_label_set_text()` on the Wi-Fi label every 30 s even when the text was
identical, invalidating the label and causing a panel update. The status label
already used `strcmp`; the Wi-Fi label did not.

**Change.**
- Added `label_set_if_changed()` and used it for the clock, Wi-Fi icon and status
  text. The status IPC still runs every 30 s and still carries actual battery,
  network, frontlight and time state.
- No visible state was cached away; unchanged text simply no longer invalidates.

**Result (MEASURED, 180 s diagnostic run).**
- **3 EPDC submits / 180 s = 1.00/min** (was 3.03/min, −67 %).
- 6 status IPC calls / 180 s; zero caused a submit.
- Production confirmation without logs: `max77696-epd` GPIO IRQ +3 / 180 s and
  `fb_dma` +9 / 180 s, both consistent with 1 update/min.
- Status/frontlight/network updates still work (see regression section).

**Status: PASS.** The remaining update is the visible minute clock.

### P2 — LVGL idle scheduling

**Root cause (MEASURED).** `LV_INDEV_DEF_READ_PERIOD=8` made
`lv_timer_handler()` return an 8 ms deadline, and the loop capped idle sleep at
25 ms. With no input this produced 124.7 render cycles/s, 127.6 voluntary
`gw-shell` wakeups/s and 277.8 context switches/s.

**Change.**
- `LV_INDEV_DEF_READ_PERIOD` changed from **8 ms to 100 ms**.
- The loop's idle timeout is no longer capped at 25 ms; it follows the next real
  LVGL timer (max 1000 ms).
- The touch evdev fd remains the primary wake source. When `poll()` reports
  `POLLIN`, the loop advances LVGL's tick and marks the input read timer ready
  with `lv_timer_ready()`, so the event is processed on the next handler pass
  instead of waiting for the 100 ms fallback.
- Dirty frames still get the existing 8 ms cap.

**Result (MEASURED, 180 s diagnostic run).**
- Render cycles **10.98/s** (was 124.7/s, −91 %).
- Context switches **51.5/s** (was 273–278/s, −81 % in the diagnostic run).
- `gw-shell` **0.144 %** with diagnostic logging on.
- EPDC remains 1/min; no missed status/clock work.

**Production result (MEASURED, 180 s, no diagnostics).**
- `gw-shell` **0.089 %**, total busy **0.19 %**, 47.2 context switches/s.
- Final post-reboot sample with Wi-Fi + Tailscale: `gw-shell` **0.10 %**, total
  busy **0.28 %**, **64.4 context switches/s**, **114.1 IRQ/s** total,
  timer tick **40.1/s** (was 223.5/s), log **0 B/s**.

**Touch responsiveness (MEASURED, physical).**
- The real evdev path was validated with a physical tap: first render after
  input **1.32 ms**, second delivered tap **1.42 ms**, first EPDC submit
  **1.85 ms**.
- An injected uinput tap on a diagnostic build produced **19.67 ms** to render
  and **1.85 ms** to submit. This includes the test harness's settle timing and
  is not used as the physical figure.
- The top-row Reader/Library cards call `gw_ctl APP LAUNCH koreader`; taps after
  the first are intentionally handled by KOReader, not lost by the shell. This
  is existing UI behaviour, not a P2 regression. A 20-cycle `ui-nav
  SETTINGS/HOME` loop reported **0 failures**, and a forced reader exit returned
  to Home (`foreground=none`, `reader-exit=128`).

**Status: PASS for idle CPU/wakeups and first-touch responsiveness.** A full
multi-tap matrix on a non-launching control remains **PARTIAL / not statistically
powered** (see limitations).

### P3 — Tailscale startup dependency

**Root cause (MEASURED).** `/etc/gleworks-early.sh` started `tailscaled` before
`/dev/urandom` existed. `/run/tailscaled.log` showed:
`crypto/rand: open /dev/urandom: no such file or directory` and the Go panic.
`gleworks-hardware-prepare.sh` (called later from the `gw-system` OpenRC
`start_pre`) was the only creator of the random nodes.

**Change.**
- `/etc/gleworks-early.sh` creates `/dev/urandom` (1:9), `/dev/random` (1:8) and
  `/dev/zero` (1:5) before starting Tailscale. This is device-node
  initialisation, not a randomness workaround.
- A **device-local** `/etc/gleworks-openrc.sh` contained an unmanaged duplicate
  Tailscale block that started a second daemon and blocked OpenRC default on
  `tailscale up --ssh`. It was preserved as
  `/etc/gleworks-openrc.sh.before-phase1-p3` and replaced by a cleaned script
  that keeps the existing Wi-Fi hook but starts Tailscale only once in early
  boot.

**Result (MEASURED, two clean reboots).**
- Boot 1 (`89e8bc4c…`): Tailscale started at 10 s, one daemon, no panic; a
  second instance failed with `sock address already in use` because the
  duplicate block was still present.
- Boot 2 (`dffec059…`): Tailscale PID 715 at 10 s, exactly one daemon, no
  crypto error. `gw-system`/`gw-shell` started automatically at **20 s**
  (previously 1,368 s / manual).
- `tailscale status` reports **logged out / NoState**; there is no valid login
  credential for this boot, so control-plane **reconnection is NOT TESTED**.
  The daemon itself starts and remains stable.

**Status: PASS for startup and boot ordering; reconnection NOT TESTED.**

### P4 — UART interrupt investigation

**Findings (MEASURED).**
- IRQ **58**, driver `imx-uart.0`, device `ttymxc0` at MMIO `0x2020000`, used as
  the kernel console (`console=ttymxc0,115200`). No `/dev/ttymxc*` node and no
  `/proc/tty/driver/serial` on this kernel.
- Idle 30 s: **1,527 IRQ = 50.9/s**.
- USB console output for ~30 s: **1,519 IRQ = 51.8/s** (no dependence on USB).
- CPU load (`yes >/dev/null`) for 30 s: **2,460 IRQ = 81.9/s**.
- Final normal system: **58.0/s**.
- The rate is therefore not driven by the USB console. It correlates with system
  activity/noise, but the exact source (console hardware/UART line) is not
  established. No driver, clock or IRQ was disabled.

**Status: PASS as investigation; no change.** Not safely actionable from
userspace.

### P5 — CPU frequency experiment

**Findings (MEASURED, 30 × 1 s samples per state).**
- `performance`: **996 MHz** in all 30 samples; 45.8 context switches/s.
- `ondemand`: **396 MHz** in all 30 idle samples (60 % lower); 69.6 context
  switches/s (+52 % from governor sampling).
- Under `yes` load, ondemand rose to **996 MHz**; after load it returned to
  396 MHz.
- Battery `current_now` remained **0**; no energy measurement is possible.
- No valid touch-latency comparison was possible without a non-launching
  repeatable target.

The original `performance` / 996 MHz state was restored and verified. No power
claim is made from frequency alone.

**Status: PASS as experiment; no change deployed.**

## 3. Before/after measurement table

Method: physical USB console, `CLK_TCK=100`, quiet windows with no touch input.
Rates use measured `/proc/uptime` deltas. Baseline figures are from
`PW3-PERFORMANCE-BASELINE.md`; after figures from the Phase 1 raw files.

| Metric | Baseline | Final | Absolute | Percent | Method |
|---|---:|---:|---:|---:|---|
| `gw-shell` CPU | 1.40 % | 0.10 % | −1.30 pp | −92.9 % | 62.2 s / 180.1 s |
| Total busy CPU | 4.93 % | 0.28 % | −4.65 pp | −94.3 % | `/proc/stat`, 180.1 s |
| Idle CPU | 95.07 % | 99.72 % | +4.65 pp | — | `/proc/stat` |
| Context switches | 277.8 /s | 64.4 /s | −213.4 | −76.8 % | 180.1 s |
| Total IRQ | 306.0 /s | 114.1 /s | −191.9 | −62.7 % | 60 s |
| Timer-tick IRQ | 223.5 /s | 40.1 /s | −183.4 | −82.1 % | 60 s |
| UART IRQ | 72.9 /s | 58.0 /s | −14.9 | −20.4 % | 60 s; variable |
| Render cycles | 124.7 /s | 10.98 /s | −113.7 | −91.2 % | diag, 180 s |
| Idle EPDC updates | 3.03 /min | 1.00 /min | −2.03 | −67.0 % | 180 s + GPIO IRQ |
| GW_UI log | 10,625 B/s | 0 B/s | −10,625 | −100 % | 90 s |
| Root used | 210,764 kB | 193,662 kB | −17,102 kB | −8.1 % | `df -k` |
| Root free | 43,107 kB | 60,209 kB | +17,102 kB | +39.7 % | `df -k` |
| tmpfs `Shmem` | 18,208 kB | 3,072 kB | −15,136 kB | −83.1 % | `/proc/meminfo` |
| Boot to service | 1,368 s manual | 20 s automatic | — | — | `/proc/<pid>/stat` |

Not claimed as improvements: UART IRQ (unchanged source), CPU frequency
(restored), battery life (not measurable), physical touch statistics (n=2).

## 4. Deployed artifacts and rollback

All binaries are dynamically linked ARM musl (`/lib/ld-musl-armhf.so.1`).
Final source rebuilds were bit-for-bit identical to the deployed files.

| Artifact | SHA-256 | Size |
|---|---:|---:|
| `/usr/sbin/gw-shell` | `6cde57047385b5cec4fb2da06d7156b41a489df258778819805d1f446f861c38` | 370,004 B |
| `/usr/sbin/gw-system` | `54103f9bb6d260c57af20cc98d879af1d63fcebb56bae8634837923a21c54ae3` | 30,088 B |
| `/usr/sbin/gwctl` (unchanged) | `6a79af5f9ac02acef25af16e3eee58ba8d4db3aab3e7b4bbcd4829e25e3e932b` | — |
| `/etc/gleworks-early.sh` | `85f15ac82fe4d42f65ca59cb23f93c3397f35d1a829868888612854c2182b4a1` | — |
| `/etc/gleworks-openrc.sh` | `c86827517374cacf04a4d7fa403a500553f2c65d80f854e8e2a1e30fd758f5d7` | — |
| `/etc/gleworks-console.sh` | `f8030cd317b61f3020f4174c4700ce8d245d746dc713a28f55a63bebfc29de67` | — |

On-device rollback artifacts:

| Original / stage | Path | SHA-256 |
|---|---|---:|
| Pre-Phase-1 shell | `/usr/sbin/gw-shell.before-phase1-p0` | `af52068780d03b60989b030170374b4b734b6806c1777856dfd353641fb83f26` |
| P0 shell | `/usr/sbin/gw-shell.before-phase1-p1` | `22a9ccec95772f814ed7561ef7da0cc19561b490219d3ba6fa91186673a0fc52` |
| P1 shell | `/usr/sbin/gw-shell.before-phase1-p2` | `987dca40e3c5fabc37d0e71e95a378dc696901bb754a758acde422fbbaa33b54` |
| Pre-Phase-1 system | `/usr/sbin/gw-system.before-phase1-p0` | `7958bfd74e7585d7022d0f4b13d5e8d6591d8e451e3d10978f731aa8c3b67723` |
| Pre-Phase-1 console | `/etc/gleworks-console.sh.before-phase1-p0` | preserved |
| Pre-P3 early init | `/etc/gleworks-early.sh.before-phase1-p3` | `c236ecabcc443a9621eac55cdb59ad2e23e647d8cace4670e607b23fd12aeb01` |
| Pre-P3 OpenRC | `/etc/gleworks-openrc.sh.before-phase1-p3` | `bec8f18e15f5ebdc99c6009b47104958f064b01736585a340c958e6d30f15075` |

Build artifacts: `work/build/phase1/gw-shell-p2-final` and
`work/build/phase1/gw-system-final` match the deployed hashes above.

Build notes: ARMv7 soft-float `-Os`, dynamic musl
(`--dynamic-linker=/lib/ld-musl-armhf.so.1`), `GW_UI_PERF=0`,
`GW_UI_DIAG=1`, `GW_UI_EPDC_SPIKE=1`, `GW_UI_TOUCH_SPIKE=1`,
`GW_UI_LIFECYCLE_SPIKE=1`, `LV_INDEV_DEF_READ_PERIOD=100`; `gw-system`
with `-fstack-protector-strong` and the existing SPIKE flags. A diagnostic
`gw-shell` is the same source with `GW_UI_PERF=1`; runtime logging still
requires `GW_UI_PERF_LOG=1` or `/run/gw-ui-perf`.

## 5. Regression suite

| Test | Result | Evidence |
|---|---|---|
| Idle Home, 180 s | **PASS** | 99.72 % idle; 1 EPDC/min; log 0 B/s; Home render captured |
| Repeated UI navigation | **PASS** | 20 × SETTINGS/HOME, 0 failures, shell alive |
| Physical touch | **PASS (limited)** | 2 delivered taps: render 1.32/1.42 ms, submit 1.85 ms |
| Reader open | **PASS** | Physical tap launched KOReader; service reported `foreground=koreader` |
| Reader forced exit / crash recovery | **PASS** | KOReader TERM → shell restarted, `foreground=none`, `reader-exit=128` |
| Frontlight adjustment | **PASS** | 14→15→14: sysfs 573→614→573 |
| Wi-Fi connection | **PASS (state)** | `wpa_supplicant` running, `net=connected`, wlan0 traffic |
| Wi-Fi reconnection | **NOT TESTED** | Did not drop/rejoin the AP |
| Tailscale startup | **PASS** | One daemon at 10 s on fresh boot, no panic |
| Tailscale reconnection | **NOT TESTED** | Logged out / NoState; no credential |
| Suspend/resume | **NOT TESTED** | Not enabled; no wake-path test |
| Reboot / Guardian recovery | **PASS** | Two clean reboots, new boot IDs, no rescue |
| Service auto-start | **PASS** | `gw-system`/`gw-shell` at 20 s |
| Large document / library indexing | **NOT TESTED** | Not part of P0–P5 |
| Log cap enforcement | **PASS** | 2 MiB test file truncated to 64 KiB on test run |
| Flash write / shell history | **PASS** | `/.ash_history` absent; history in `/run` (2.69 MB tmpfs) |

## 6. Remaining bottlenecks (ranked by measured impact)

1. **UART IRQ 58 / `ttymxc0` — 50–82/s.** Not USB-driven; likely physical
   console/noise. No safe userspace fix; requires kernel/console investigation.
2. **No cpuidle, governor `performance`, 996 MHz.** P5 showed ondemand can idle
   at 396 MHz but adds ~24 context switches/s and energy is unmeasurable. Restore
   or evaluate only with a battery-only power meter and repeatable touch test.
3. **One EPDC update/min for the clock.** Allowed and expected; only worth
   changing if the clock is removed or less frequent.
4. **`gw-system` 1 s poll.** Not changed in Phase 1; a small remaining wakeup
   source (final 64.4 ctxt/s includes Wi-Fi/Tailscale/status timers).
5. **Storage still 76 % used.** `tailscaled` 36.5 MB, Python pycache/debug/gdb
   and CJK font remain. Not touched because reader/font/recovery correctness was
   not re-accepted in this phase.
6. **Console history bytes are bounded by line count, not size.** 200 long
   commands can still create a multi-MB tmpfs file; it is not on flash and is
   cleared on reboot. A byte cap would be a small follow-up.
7. **Ext3 `barrier=0,data=writeback`.** Recorded safety finding, unchanged.
8. **Physical touch repeat matrix** on a non-launching control is only PARTIAL;
   the top-row cards intentionally launch KOReader.

## 7. Safety and limitations

- No bootloader, kernel, Guardian, waveform/GUID data, USB recovery path or
  boot-critical Amazon partition was modified.
- Deployment used the existing USB console, on-device backups and clean reboots.
- High-frequency diagnostics are opt-in only; production logs are bounded at
  1 MiB and currently 276,198 B with zero idle growth.
- Battery energy could not be measured: `current_now` reports 0, the charger was
  connected and the battery was full.
- Physical touch statistics have n=2 delivered taps, not a full median/P95
  distribution. The top-row Reader/Library cards launch KOReader by design, so
  repeated taps after the first are handled by KOReader.
- Tailscale is running but logged out; reconnection is not claimed.
- Suspend/resume, Wi-Fi reconnection, large-document opening and library
  indexing were not re-tested in this phase.

## 8. Reproduction

Read-only audit on the final build:

```sh
python3 tools/maintenance/pw3-audit.py --sample-seconds 180 \
  --json docs/evidence/raw/PW3-PHASE1-FINAL.json \
  --raw  docs/evidence/raw/PW3-PHASE1-FINAL.txt
```

Diagnostic logging (delete the file to return to normal):

```sh
touch /run/gw-ui-perf        # checked once at gw-shell start
# restart gw-shell under gw-system, collect, then:
rm -f /run/gw-ui-perf
```

uinput touch harness (requires a diagnostic build with
`-DGW_UI_TOUCH_DEV_OVERRIDE=1`):

```sh
python3 tools/maintenance/pw3-uinput-touch.py --taps 3 --interval 5 --x 408 --y 864
```

Raw Phase 1 evidence is in `docs/evidence/raw/PW3-PHASE1-*.txt`.

## 9. Final status

- **P0:** PASS — unbounded log and root shell-history growth removed.
- **P1:** PASS — idle EPDC 3.03/min → 1.00/min, only the clock remains.
- **P2:** PASS for idle CPU/wakeups and first-touch latency; full repeat matrix
  PARTIAL due to intentional reader handoff.
- **P3:** PASS for entropy ordering/Tailscale startup and auto-start; reconnection
  NOT TESTED.
- **P4:** PASS as investigation; UART IRQ not safely actionable.
- **P5:** PASS as experiment; baseline `performance` restored, no power claim.

The final deployed system is responsive on the physical device, logs no idle
waste, refreshes the panel only for visible time changes, and starts the service
automatically after reboot.

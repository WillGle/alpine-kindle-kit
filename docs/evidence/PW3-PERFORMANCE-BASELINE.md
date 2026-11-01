# PW3 Alpine — Phase 0 Performance Baseline

Date: 2026-10-31 local (device UTC clock: 2026-10-30 22:00–23:00)  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM, 1072 × 1448 E-ink  
OS: Alpine Linux 3.24.3, musl, BusyBox PID 1, kernel `3.0.35-lab126`  
Boot ID: `52b97872-3abb-4ef3-b260-6f7075ab5a67`  
Access: physical USB CDC-ACM console (`0525:a4a7`, `/dev/ttyACM0`)  
Scope: **Phase 0 only. No optimization, remount, restart, service change or
deployment was performed while collecting this baseline.**

## Evidence labels

| Label | Meaning |
|---|---|
| **MEASURED** | Read directly from the physical device with the stated method/sample. |
| **DERIVED** | Arithmetic on MEASURED values. |
| **ESTIMATED** | Inference from source plus measured counters; not A/B tested. |
| **NOT MEASURED** | Could not be measured safely or reliably in this state. |

Raw evidence is in `docs/evidence/raw/PW3-AUDIT-2026-10-31-*.txt`.
The reproducible collector is `tools/maintenance/pw3-audit.py`. It is read-only.

## Measurement method

Two quiet-idle windows were used, with no touch input and no commands during the
window itself:

1. **90 s window** (`sample90`): `/proc/stat`, `/proc/interrupts`, per-process
   `/proc/<pid>/stat` + `status`, `/proc/diskstats`, `/proc/meminfo`,
   `/proc/vmstat`, `/proc/net/dev`, battery sysfs and the GW_UI log counters.
2. **180 s window** (`idle180`): the same counters plus EPDC/render log counts.
   Measured elapsed time was 237.6 s because the on-device log scan itself took
   time; all rates use the measured uptime delta, not the requested 180 s.
3. **EPDC statistics** were extracted from the deployed `/run/gw-system.log`
   (instrumentation already enabled in the running build).

The collector ran at 100 Hz (`CLK_TCK=100`). The console shell and the collector
commands add short-lived CPU, IRQ and process noise at the snapshot boundaries;
the 90 s figure is used for aggregate idle CPU. Per-process figures are from a
separate 62.2 s window.

## 1. Platform and runtime

| Item | Value | Label |
|---|---|---|
| Kernel | `Linux 3.0.35-lab126 #8 PREEMPT Tue Aug 1 12:49:59 UTC 2023 armv7l` | MEASURED |
| Machine | Freescale i.MX 6SoloLite Wario Board | MEASURED (dmesg) |
| RAM | `MemTotal: 514724 kB` | MEASURED |
| Root filesystem | `/dev/loop1` on `mmcblk0p4`, ext3, `rw,noatime,errors=continue,user_xattr,barrier=0,data=writeback` | MEASURED |
| Control partition | `/dev/loop0` on vfat, 3,188,640 kB, 41 % used | MEASURED |
| PID 1 | `/bin/busybox` | MEASURED |
| OpenRC default runlevel | `gw-system` only | MEASURED |
| `gw-system` | `/usr/sbin/gw-system --supervise`, RSS 316 kB | MEASURED |
| `gw-shell` | `/usr/sbin/gw-shell`, RSS 656 kB | MEASURED |
| Tailscale | **not running**; startup failed at boot | MEASURED |
| KOReader | **not running**; static footprint 68.6 MB | MEASURED footprint, runtime NOT MEASURED |
| Swap | none (`/proc/swaps` empty), no swap device | MEASURED |
| zram/zswap | no active swap/zram device; zswap not assumed | MEASURED absence of swap |
| PSI / cpuidle | not available (no `/proc/pressure`, no `cpu0/cpuidle`) | MEASURED absence |
| eBPF | not assumed on 3.0.35; not used | NOT USED |

Mount `barrier=0,data=writeback` is the **existing** state. It is a power-loss
durability risk and is recorded as a safety finding, not changed in Phase 0.

## 2. CPU frequency, idle states and governor

| Setting | Value | Label |
|---|---|---|
| Current governor | `performance` | MEASURED |
| Current frequency | `996000` kHz | MEASURED |
| Available frequencies | `996000 396000` | MEASURED |
| Available governors | `ondemand userspace powersave performance` | MEASURED |
| cpuidle states | none exposed | MEASURED |
| `/sys/power/state` | `standby mem` | MEASURED |
| autosleep | absent | MEASURED |
| suspend enabled/used this boot | no evidence; not tested | NOT MEASURED |

The device is pinned at 996 MHz at idle, with no cpuidle driver. This is a
power opportunity, but no power benefit is claimed until a battery-only
measurement exists.

## 3. Process CPU, memory and context switches

90 s aggregate (MEASURED, n = 1 window, 9,278 CPU jiffies):

| CPU component | Share |
|---|---:|
| user | 2.32 % |
| system | 1.88 % |
| idle | 95.07 % |
| softirq | 0.73 % |
| iowait | 0.00 % |
| irq | 0.00 % |

Per process over 62.2 s (MEASURED):

| Process | PID | CPU | RSS | VmHWM | minflt Δ | majflt Δ | voluntary ctx/s |
|---|---:|---:|---:|---:|---:|---:|---:|
| `gw-system` | 12678 | 0.02 % | 316 kB | 316 kB | 0 | 0 | 2.24 |
| `gw-shell` | 12680 | **1.40 %** | **656 kB** | **656 kB** | 0 | 0 | **127.6** |
| `wpa_supplicant` | 12774 | ≈0 % | 1,612 kB | 1,624 kB | 0 | 0 | 0.10 |
| kernel threads | — | not individually significant | — | — | — | — | — |

Notes:

- `gw-shell` is the only material userspace CPU consumer at idle.
- Its `VmPeak` is 2,724 kB but resident high-water mark is only 656 kB.
- Sum of sampled userspace RSS is ≈ 2.6 MB. The rest of RAM is kernel/cache.
- Context switches: **277.8/s** (MEASURED, 90 s). The 180 s window gives
  **266.8/s**, consistent.
- Load average observed: `0.39 0.37 0.36` (MEASURED, single sample).

## 4. Interrupts and wakeup sources

Per-second rates from the 90 s window (MEASURED; 180 s values in parentheses):

| Source | IRQ/s |
|---|---:|
| **All interrupts** | **306.0** (305.3) |
| `i.MX Timer Tick` | 223.5 (225.8) |
| **`IMX-uart`** | **72.9 (74.0)** |
| `imx-i2c` (IRQ 68) | 7.3 (7.3) |
| `usb_wakeup / fsl-usb2-otg / fsl-usb2-udc` | 1.24 (0.14) |
| `mmc0` | 0.76 (0.19) |
| `fb_dma` | 0.16 (0.12) |
| `max77696-epd` (GPIO 301) | 0.05 (—) |
| `main_ttsp_core` touch | 0 (0) |

Observations:

- 306 interrupts/s and 267–278 context switches/s is high for an idle e-reader.
- The UART contributes ~73–74 IRQ/s continuously with no console traffic
  attributable in dmesg. Root cause is not established in Phase 0.
- No `timer_stats`, no `/sys/kernel/debug/wakeup_sources` and no cpuidle
  residency counters exist on this kernel; true "wakeup" accounting is therefore
  partial. Ctxt+IRQ rates are the usable proxy.

## 5. Memory, page faults and reclaim

90 s snapshot (MEASURED):

| Metric | Value |
|---|---:|
| MemFree | 211,544 kB |
| Buffers | 104,868 kB |
| Cached | 150,064 kB |
| Shmem | 18,208 kB |
| AnonPages | 7,492 kB |
| Slab (SReclaimable + SUnreclaimable) | 13,672 kB |
| Dirty / Writeback | 0 / 0 kB |
| Swap | 0 kB |
| `pgsteal_*` Δ | 0 |
| `pgscan_*` Δ | 0 |
| `allocstall` Δ | 0 |
| `pageoutrun` since boot | 1 |

Between the 90 s snapshots, `Shmem` grew **+956 kB** while the GW_UI log grew
**+978,459 B**. This is effectively a 1:1 relationship: the diagnostic log is
consuming tmpfs RAM as it grows.

The 180 s window confirms it: `Shmem` +2,520 kB, log +2,581,143 B,
`MemFree` −2,480 kB. **No reclaim occurred** during either window.

Global page faults since boot: `pgfault 1,749,753`, `pgmajfault 263` (MEASURED).
None of the three long-running application processes showed page-fault growth
during the sampled window; the only fault growth was in the short-lived
console/audit shell.

Memory pressure is **not** currently a bottleneck. The growing tmpfs log is a
trend that will become one if left unbounded.

## 6. Storage

### 6.1 Capacity

| Metric | Value |
|---|---:|
| Root filesystem size | 253,871 kB (256 MiB image) |
| Used | **210,764 kB (83 %)** |
| Available | **43,107 kB (17 %)** |
| Inodes used | 3,634 / 65,536 (6 %) |
| `/usr` | 101,953 kB |
| `/opt` | 71,692 kB |
| `/opt/koreader` | 68,601 kB |
| `/usr/lib` | 50,061 kB |
| `/usr/lib/python3.14` | 29,488 kB |
| `/usr/sbin` | 42,883 kB |
| `/usr/lib/debug` | 2,886 kB |
| `/var/cache/apk` | 2,791 kB |
| `/opt/koreader/fonts` | 25,076 kB |
| `/opt/koreader/libs` | 17,836 kB |

All values MEASURED with `du -xk` / `df -k`.

### 6.2 Packages

- 68 installed packages; APK database 136,042 B.
- Sum of package `S:` sizes: **42,022,034 B (≈ 40.1 MiB)** (DERIVED).
- Largest packages (MEASURED `S:` field): `tailscale` 13.35 MB, `python3`
  8.05 MB, `python3-pycache-pyc0` 4.50 MB, `gdb` 4.24 MB, `wpa_supplicant`
  1.69 MB, `libcrypto3` 1.55 MB, `musl-dbg` 1.04 MB, `libstdc++` 0.84 MB.
- These `S:` values are package metadata sizes; the uncompressed on-disk image
  is larger (for example `tailscaled` is 36,511,412 B as a file).

### 6.3 Largest files on the root image

| File | Bytes |
|---|---:|
| `/usr/sbin/tailscaled` | 36,511,412 |
| `/.ash_history` | **20,158,538** |
| `/opt/koreader/fonts/noto/NotoSansCJKsc-Regular.otf` | 16,412,332 |
| `/usr/bin/gdb` | 6,928,280 |
| `/usr/lib/libpython3.14.so.1.0` | 3,716,904 |
| `/opt/koreader/libs/libwrap-mupdf.so` | 3,224,500 |
| `/usr/lib/debug/lib/ld-musl-armhf.so.1.debug` | 2,938,956 |
| `/var/cache/apk/APKINDEX.*.tar.gz` | 2,315,953 |
| `/opt/koreader/fonts/freefont/FreeSerif.ttf` | 1,862,212 |
| `/opt/koreader/fonts/noto/NotoSans-Regular.ttf` | 348,408 |
| `/opt/koreader/fonts/noto/NotoSerif-Regular.ttf` | 395,772 |

`/.ash_history` is **20.2 MB on the writable ext3 root**, written by the
interactive USB console shell, with 31,117 recorded lines. It is not rotated.
This is both a storage and a flash-write finding. Do not delete it during Phase 0;
it contains administrative history and must be handled deliberately.

### 6.4 Locale and fonts

- No `/usr/share/i18n`, `/usr/lib/locale` or `/usr/share/locale` data was found
  (probe returned empty). Locale data is not a storage consumer here.
- Vietnamese-capable font families present: Noto Sans/Serif (regular, bold,
  italic), FreeSerif/FreeSans and URW fonts. Glyph coverage for Vietnamese was
  **not** verified in Phase 0.
- A 16.4 MB Noto CJK font is the largest font asset. It must not be removed
  without checking whether CJK documents are required.

### 6.5 Flash writes and temporary activity

| Item | Idle rate | Label |
|---|---:|---|
| `mmcblk0p4` write sectors | +128 sectors / 92.1 s = **712 B/s** | MEASURED |
| Same, 180 s window | +88 sectors / 237.6 s = **190 B/s** | MEASURED |
| Equivalent daily range (idle only) | ≈ 17–61 MB/day | DERIVED |
| Write completions | 11 / 92 s, write ticks 0 | MEASURED |
| GW_UI log | **10.6–11.4 kB/s**, tmpfs (not flash) | MEASURED |
| GW_UI log size at ~47 min uptime | **28,438,702 B** | MEASURED |
| `/run` tmpfs usage | 17–28 MB and rising | MEASURED |
| `/tmp` | 8 kB | MEASURED |

The root log is on tmpfs, so it does not directly wear flash, but it consumes RAM
and will fill `/run` in roughly **6 hours** at the observed rate
(≈ 10.6 kB/s against ≈ 230 MB free at the time of measurement).
`dirty_writeback_centisecs=500` (5 s) explains periodic journal activity; ext3
journaling is enabled.

### 6.6 Boot-critical and separate storage

The 256 MiB Alpine image is not the book/data volume. The control vfat
partition is 3.1 GB and 41 % used; books/documents are outside the system image.
No partition layout or boot-critical partition was changed.

## 7. E-ink rendering and the observed idle refresh pattern

### 7.1 Framebuffer and mode

| Item | Value | Label |
|---|---|---|
| Framebuffer | `/dev/fb0`, virtual 1088 × 6144, 8 bpp | MEASURED |
| Panel | 1072 × 1448 | MEASURED |
| Stride | 1088 | MEASURED |
| Deployed build flags | `EPDC_accepted=0`, `touch_accepted=0`, `GW_UI_EPDC_SPIKE=1`, `GW_UI_TOUCH_SPIKE=1`, `GW_UI_LIFECYCLE_SPIKE=1`, `GW_UI_PERF=1`, fast mode off | MEASURED (log header) |

### 7.2 Render-loop and update rates

| Metric | Value | Label |
|---|---:|---|
| Render cycles | 123.7–124.7/s (≈ every 8 ms) | MEASURED |
| Render handler time | p50 **11 µs**, p95 15 µs, p99 29 µs, max 5,234 µs (n=51,934) | MEASURED |
| EPDC submits since boot | 198 at extraction time | MEASURED |
| EPDC completes | 194 | MEASURED |
| Flush-copy calls | 593 | MEASURED |
| Flush calls per submit (whole boot at extraction) | 2.99 (3.51 before idle updates diluted the initial paint) | DERIVED |
| Flush calls per submit while idle | **1.0** | MEASURED |
| **Idle EPDC update rate** | **12 updates / 237.6 s = 3.03/min (one every ≈ 20 s)** | MEASURED |
| Idle status IPC | 8 / 237.6 s (one every 29.7 s) | MEASURED |
| Idle input events | 0 | MEASURED |

### 7.3 Waveform and dirty regions

At the waveform extraction snapshot (160 submit lines; MEASURED):

| Waveform | Count | Meaning in this build |
|---|---:|---|
| waveform 8, mode 0 | 151 | REAGL partial update |
| waveform 2, mode 1 | 9 | GC16 full update |

Dirty-region areas over 161 submits: **min 3,186 px, p50 5,238 px,
p95 626,940 px, max 1,552,256 px** (full screen = 1072 × 1448). The last ten
idle updates are all waveform 8 with areas 4,428–5,346 px (≈ 0.3 % of screen).

First paint used 46 flush bands of 32 rows (34,304 px each); per-band copy was
≈ 840–920 µs with occasional 10.9 ms outliers, i.e. ≈ 39–42 ms of framebuffer
copy before the first EPDC submit. This is consistent with the earlier Stage 0
observation.

### 7.4 Latency-shaped measurements

| Measurement | Value | Label |
|---|---:|---|
| `MXCFB_SEND_UPDATE` ioctl | p50 **1,563 µs**, p95 1,882 µs, max 11,782 µs (n=198) | MEASURED |
| Flush copy | p50 **730 µs**, p95 855 µs, p99 10,891 µs (n=593) | MEASURED |
| Update-complete wait | p50 12 µs, p90 644,393 µs, p95 647,354 µs, max 652,885 µs (n=194) | MEASURED |
| Full GC16 update completion | ≈ 650 ms | MEASURED (from wait distribution) |
| Physical touch-to-visible change | — | **NOT MEASURED** |

The 650 ms tail is the panel/kernel full-refresh completion, not necessarily the
first visible pixel change. A slow-motion camera test is still required before
calling it visible latency.

### 7.5 Measured idle-refresh cause [ESTIMATED from counters + source]

During 237.6 s of no input there were exactly 12 panel submits, 8 `status ipc`
calls and 4 wall-clock minute changes. The source calls
`lv_label_set_text(g_wifi_label, …)` on every status update without a
change check, and the clock changes once per minute. The counts match 12.
This is a strong hypothesis for the one-update-per-≈20 s pattern, but it has not
been A/B tested. No source change was made in Phase 0.

## 8. Logging and diagnostic overhead

The deployed `gw-shell` has `GW_UI_PERF=1` and writes one line per render cycle
to stderr, which is redirected to `/run/gw-system.log` by the service.

| Metric | Value | Label |
|---|---:|---|
| Log growth, 90 s | **10,625 B/s**, 123.7 lines/s | MEASURED |
| Log growth, 180 s | **10,863 B/s** | MEASURED |
| Log size at 2,799 s uptime | 28,438,702 B | MEASURED |
| Projected `/run` fill time | ≈ 6 h | DERIVED |
| Dominant line | `GW_UI: render handler_us=… delay=8 dirty=0 …` | MEASURED |
| Estimated share of `gw-shell` CPU from this loop | not isolated; bundle is 1.40 % total | ESTIMATED |

The log is extremely valuable for diagnosis, but as deployed it is unbounded and
always-on. This is the single clearest measured overhead in the baseline.

## 9. Wi-Fi and Tailscale

| Item | Value | Label |
|---|---|---|
| `wpa_supplicant` RSS | 1,612 kB | MEASURED |
| `wpa_supplicant` CPU | ≈ 0 % | MEASURED |
| `wpa_supplicant` context switches | 0.10/s | MEASURED |
| `wlan0` traffic during 90 s | **0 bytes, 0 packets** | MEASURED |
| `wlan0` totals since boot | 487,065 RX bytes, 1,555 packets, 886 RX drops | MEASURED |
| Service-reported network | `net=disconnected`, `wifi-state=FAILED` | MEASURED |
| `tailscaled` | **not running** | MEASURED |
| Tailscale boot failure | `crypto/rand: open /dev/urandom: no such file or directory` | MEASURED |
| Tailscale binary | 36,511,412 B; package 13.35 MB | MEASURED |

Boot-order correlation (MEASURED log + source): `/etc/gleworks-early.sh` launches
`tailscaled` before OpenRC starts `gw-system`; `gleworks-hardware-prepare.sh`,
called from the `gw-system` OpenRC `start_pre`, is what creates `/dev/urandom`.
`tailscaled` therefore panics on a fresh boot and is never retried. The node is
presently offline despite the USB console being healthy. Tailscale runtime RSS
is **NOT MEASURED** because it is not running.

## 10. Boot, suspend and resume

| Item | Value | Label |
|---|---|---|
| Kernel to `g_serial` ready | 12.589 s | MEASURED (dmesg) |
| USB CDC high-speed config | 13.036 s | MEASURED (dmesg) |
| Touch driver probe complete | 20.5 s | MEASURED (dmesg) |
| `gw-system` process start time | **1,368.21 s after boot** | MEASURED (`/proc/<pid>/stat`) |
| `gw-shell` process start time | **1,368.21 s after boot** | MEASURED (`/proc/<pid>/stat`) |
| `wpa_supplicant` start time | 1,506.29 s after boot | MEASURED |
| Boot-to-UI latency | — | **NOT MEASURED / not reproducible** |
| Suspend/resume this boot | — | **NOT TESTED** |

The running `gw-system`/`gw-shell` instances did not start during early boot;
their `starttime` is 1,368 s. This is consistent with the known OpenRC
auto-start problem. A boot-to-Home timing cannot be reported from this boot, and
the current deployment cannot be assumed to bring up the UI automatically after
a reset. This is a reliability finding, not a performance result.

Suspend states `standby` and `mem` exist and `wakeup_count` is `0`, but
auto-suspend is absent and no suspend was attempted in this baseline. Previous
project evidence contains an RTC-bounded suspend probe, but it was not re-run on
this boot and is not claimed here.

## 11. Frontlight, battery and power

| Item | Value | Label |
|---|---|---|
| Frontlight brightness | 573 / 4095 = **14.0 %** | MEASURED |
| Battery capacity | 99 % | MEASURED |
| Battery voltage | 4.200–4.205 V | MEASURED |
| Charge counter | 1,465 (unchanged over window) | MEASURED |
| `current_now` | **0** (sensor reports zero) | MEASURED |
| Charger `online` | **0** | MEASURED |
| Battery `status` | `Full` | MEASURED |
| Battery-only idle current / drain | — | **NOT MEASURED** |

The device was on USB for console access and the battery was full. The fuel-gauge
current reading was zero, so no idle power or battery-life conclusion is valid
from this session. A charger-disconnected, remote-controlled (Tailscale/SSH or
on-device logger) test is required.

## 12. OpenRC and service state

| Runlevel | Contents |
|---|---|
| sysinit | empty |
| boot | empty |
| default | `gw-system` only |
| nonetwork | empty |
| shutdown | empty |

MEASURED with `rc-status -a` and `ls /etc/runlevels/*`.

`/etc/inittab` entries:

```
::sysinit:/bin/busybox sh /etc/gleworks-early.sh
ttyGS0::respawn:/bin/busybox sh /etc/gleworks-console.sh
::restart:/run/gleworks-shutdown
::once:/bin/busybox sh /etc/gleworks-openrc.sh
```

`gw-system` depends on `after localmount`, `before gw-shell` (a service that does
not exist) and `use logger` (no logger running). Logs go directly to
`/run/gw-system.log`; there is no rotation.

## 13. What is *not* a bottleneck at idle

These were checked and should not receive speculative optimization before the
real bottlenecks are addressed:

- **Memory pressure/reclaim:** none. `pgsteal=0`, `allocstall=0`, ~200 MB free,
  no swap.
- **`gw-system` CPU:** 0.02 %, 2.2 voluntary wakeups/s.
- **`wpa_supplicant`:** ≈ 0 % CPU and zero network traffic while disconnected.
- **LVGL render handler:** 11 µs median; the handler itself is cheap.
- **EPDC submit syscall:** 1.56 ms median; full-refresh wait is a panel/hardware
  property, not a software busy loop.
- **Idle flash writes:** low (0.19–0.71 kB/s), though nonzero due to journaling
  and shell history.
- **Locale data:** absent; not a storage consumer.

## 14. Preliminary bottleneck register (no fixes applied)

Ranked by the strength of the measured evidence, with likely benefit and
regression risk. This is a Phase 0 hand-off list, not a set of approved changes.

| # | Finding (MEASURED unless marked) | Evidence | Likely benefit | Effort | Risk | Status |
|---|---|---|---|---|---|---|
| 1 | Always-on per-render `dprintf` to `/run/gw-system.log` | 10.6–11.4 kB/s; 28.4 MB at 47 min; ~6 h to fill `/run`; 1.40 % `gw-shell` CPU | RAM, CPU, wakeups, reliability | Low | Low (keep behind opt-in diagnostic flag) | **Not changed** |
| 2 | Idle E-ink updates every ≈20 s from status/clock label invalidation | 12 updates / 237.6 s; 8 status IPC + 4 minute changes; idle areas 4.4–5.3 k px | Panel refreshes, power, ghosting | Low | Medium (UI correctness) | **Not changed**; cause ESTIMATED |
| 3 | LVGL 8 ms timer at idle (≈124.7 render cycles/s) | render cycles/s; `LV_INDEV_DEF_READ_PERIOD=8`; event-driven touch poll already exists | CPU, wakeups | Medium | Medium (input latency) | **Not changed** |
| 4 | CPU pinned at 996 MHz (`performance`), no cpuidle | sysfs; 306 IRQ/s, 267–278 ctxt/s | Idle power (unquantified) | Low–Medium | Medium | **Not changed**; power NOT MEASURED |
| 5 | Root image 83 % full; 20.2 MB `.ash_history`; 36.5 MB `tailscaled`; debug/pycache packages | `df`, `du`, file list | Free space, fewer writes | Medium | Medium (must preserve reader/fonts/recovery) | **Not changed** |
| 6 | `gw-system` 1 s `poll()` timeout | source `poll(..., 1000)`; 2.2 wakeups/s | Small CPU/wakeup reduction | Low | Low–Medium | **Not changed** |
| 7 | Tailscale fails on fresh boot (`/dev/urandom` ordering) | panic log; early-init vs `gw-system` `start_pre` order | Reliability; avoids repeated failed startup | Low | Low | **Not changed** |
| 8 | ext3 mounted `barrier=0,data=writeback` | `/proc/mounts` | None (safety issue) | Low | High if changed blindly | **Recorded only** |
| 9 | UART IRQ ≈73–74/s unexplained | `/proc/interrupts` | Power/wakeups if removable | Unknown | Unknown | **Investigate** |

## 15. Data quality and limitations

- All measurements are from the physical PW3; no host-only benchmark is used as
  a performance claim.
- No device state was intentionally modified. Two background effects are
  unavoidable while using the USB console: the interactive shell appends its own
  commands to `/.ash_history`, and the already-running `gw-shell` continues
  writing `/run/gw-system.log`. Both are measured, not caused by the audit tool's
  logic.
- The deployed `GW_UI_PERF` instrumentation is part of the measured baseline.
  Removing or gating it is an optimization to be evaluated, not assumed.
- The USB console shell and the audit commands add process/CPU/IRQ noise at the
  snapshot boundaries. Short samples are biased; the 90 s window is the clean
  aggregate reference.
- No power meter, no battery-only interval and a zero `current_now` reading mean
  power/battery claims are not possible yet.
- No PSI, cpuidle residency, timer statistics or wakeup-source list on this
  kernel; ctxt/IRQ rates are the proxy.
- KOReader was not running and was not launched during the read-only baseline;
  its runtime RSS and repeated-navigation leak behaviour are NOT MEASURED.
- Physical touch-to-visible latency, ghosting, suspend/resume and boot-to-UI are
  NOT MEASURED in this phase.
- The service start time anomaly (1,368 s) is recorded, but its exact cause was
  not proven. Do not report a boot-time improvement until automatic startup is
  reproduced and measured.

## 16. Reproduction

Read-only audit with a 90 s quiet window:

```sh
python3 tools/maintenance/pw3-audit.py \
  --sample-seconds 90 \
  --json docs/evidence/raw/PW3-AUDIT-$(date +%F).json \
  --raw  docs/evidence/raw/PW3-AUDIT-$(date +%F).txt
```

Add `--full` to include the slow `du -xk /` and largest-file inventory. The tool
requires the physical USB console and does not write to the device.

EPDC log analysis on the device:

```sh
python3 tools/maintenance/parse-eink-perf.py /run/gw-system.log --json
```

## 17. Phase 0 conclusion

The system is **not** memory- or reclaim-limited at idle, and Wi-Fi is not
consuming measurable CPU while disconnected. The measured idle cost is dominated
by the UI render loop and its always-on diagnostic logging:

- `gw-shell`: 1.40 % CPU, 127.6 voluntary wakeups/s, 656 kB RSS.
- 123.7–124.7 render cycles/s, 3.03 panel updates/min while idle.
- 10.6–11.4 kB/s of tmpfs log growth with no cap.
- 306 IRQ/s and 267–278 context switches/s overall.
- CPU pinned at 996 MHz with no cpuidle.

No optimization has been applied and no success is claimed. The next phase
should target the measured items in Section 14 with small, independently
reversible patches, starting with the unbounded logging and the idle panel
invalidation path, and must be validated on the physical device.

# PW3 Alpine — Phase 2 Active Workload Performance

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
OS: Alpine 3.24.3, musl, kernel `3.0.35-lab126`  
Final boot ID: `f4c54f56-4721-427f-8e2d-48996a78aae1`  
Access: physical USB CDC-ACM console (`0525:a4a7`)  
Scope: read-only workload measurement. No reader/UI code was changed in this
phase.

**Status: PARTIAL.** The document-open, page-turn and repeated-cycle baselines
are measured. The library-index update and a statistically complete managed-UI
launch trace were not obtained. The dominant measured cost is KOReader CRE
rendering of the large EPUB, not storage I/O.

## Evidence labels

| Label | Meaning |
|---|---|
| **MEASURED** | Read from the physical device with the stated method. |
| **DERIVED** | Arithmetic on measured values. |
| **ESTIMATED** | Inference from source plus measured counters. |
| **NOT TESTED** | No valid device measurement in this phase. |

Raw evidence is in `docs/evidence/raw/PW3-PHASE2-P3-*.txt`. The device-side
launcher is `tools/maintenance/pw3-reader-bench.sh`.

## 1. Workloads

The device library contained one 2,272-byte install-check EPUB. For a meaningful
baseline, three public/test documents were copied to `/mnt/us/documents` over
the USB console and hashed before use:

| Workload | File | Size | SHA-256 |
|---|---|---:|---|
| Small EPUB | `P2-Small-Guide.epub` | 54,435 B | `f5de9e4bf0b273e0bdae49bf6471b0e8dd855784376fe9a5c977fd3278bbbc81` |
| Large EPUB | `P2-Moby-Dick.epub` | 814,023 B | `1bc88e79ef7cf9d5325abe0071e6302265b708df56d3af07db124c81e84197f6` |
| Complex PDF | `P2-Complex.pdf` | 1,021,710 B | `dfac230dc104b2e519b505cb016b080d4f95d0b748cfb992d0831cee21d95271` |

The small and large EPUBs were measured with the same reader runtime and library
path. The PDF was selected as a real multi-page text/graphics PDF rather than a
generated blank page.

## 2. Cold boot to Home

Measured on the final clean boot:

| Marker | Time after boot | Method |
|---|---:|---|
| `tailscaled` start | **10 s** | `/proc/<pid>/stat` field 22 |
| `wpa_supplicant` start | **13 s** | same |
| `gw-system` start | **16 s** | same |
| `gw-shell` start | **16 s** | same |
| first `gw-shell.diag` PING reply | **23.1 s** | `/proc/uptime` at first successful reply |
| Wi-Fi associated + IPv4 | **≤ 23 s** | `wpa_state=COMPLETED`, `192.168.1.200` |
| DHCP client resident | **≤ 23 s** | `udhcpc` PID 964 |

This is consistent with the Phase 1 result and does not regress it. The 23.1 s
figure includes host console reconnection time after boot; `gw-shell` itself
starts at 16 s.

## 3. Document open and first render

Method: stop `gw-system`, launch the reader directly with
`pw3-reader-bench.sh`, and time from process start to KOReader's
`Restoring user input handling` marker. This is the point at which the reader
has completed document load, layout and first render and has re-enabled input.

| Workload | Launch→ready | CRE load | CRE render | RSS | Peak RSS | Minor faults | Major faults |
|---|---:|---:|---:|---:|---:|---:|---:|
| Small EPUB | **7.01 s** | 0.169 s | 0.476 s | 33,860 kB | 52,136 kB | 14,756 | 0 |
| Large EPUB | **18.17 s** | 1.279 s | **11.841 s** | 51,888 kB | 70,176 kB | 20,039 | 0 |
| Complex PDF | **6.17 s** | n/a | n/a | 30,732 kB | 33,292 kB | 11,307 | 0 |

The CRE load/render values come from a `-d` repeat: 7.61 s, 20.02 s and 6.67 s
to ready respectively. Debug logging adds overhead; the table uses the
non-debug ready times and the debug internal timings.

**Root cause of the large-EPUB cost (MEASURED).** `loading took` is 1.28 s, but
`rendering took` is 11.84 s. Storage is therefore not the bottleneck: the reader
reads the 814 KB file quickly, then spends almost twelve seconds in CRE page
layout/rendering before the first page becomes interactive. The same 11.84 s
appears on a second open, so this is not a one-time flash-cache effect. The
reader logs `CRE: failed reading index from cache file` on each open; no
`.sdr` sidecar/cache appeared for this document under this launch method.

No safe small source change was made. Optimising CRE layout would be a KOReader
change, not an OS configuration change, and was not attempted without an
upstream-supported cache/configuration mechanism and regression testing.

## 4. 100-page navigation

KOReader's built-in AutoTurn plugin was enabled temporarily at 0.5 s per page.
The original setting was backed up and restored after the run.

| Metric | Result |
|---|---:|
| Navigation window | 60.05 s |
| Page advances | ~100 |
| EPDC GPIO updates | 105 |
| `fb_dma` IRQs | 314 |
| Reader CPU | **11.02 %** |
| Reader RSS | 52,368 kB |
| Reader peak RSS | 70,376 kB |
| Minor faults | 25,117 |
| Major faults | 2 |

**DERIVED:** approximately 1.75 EPDC GPIO updates per second and about 11 % of
one CPU core for 0.5 s page turns. The panel refresh pattern is consistent with
one visible update per page plus occasional multi-update waveforms.

## 5. Repeated launch/exit memory

Five consecutive direct opens of the small EPUB, waiting for the ready marker,
then terminating and relaunching:

| Cycle | Ready | RSS | Peak RSS | Minor faults | Major faults |
|---:|---:|---:|---:|---:|---:|
| 1 | 7.20 s | 34,148 kB | 52,432 kB | 14,837 | 1 |
| 2 | 7.19 s | 33,860 kB | 52,140 kB | 14,864 | 0 |
| 3 | 7.22 s | 34,100 kB | 52,380 kB | 14,746 | 0 |
| 4 | 7.14 s | 33,884 kB | 52,164 kB | 14,731 | 0 |
| 5 | 7.21 s | 33,916 kB | 52,196 kB | 14,856 | 0 |

RSS range: **33,860–34,148 kB**, a 288 kB spread with no upward trend. No
repeated-cycle memory growth was observed in five cycles.

## 6. Home versus KOReader

| Path | Measured behaviour |
|---|---|
| Home idle | Phase 1 regression sample: `gw-shell` 0.10 %, 64.9 ctxt/s, 0 B/s log growth, 1 EPDC/min |
| Home touch | Phase 1 physical test: 1.32–1.42 ms input-to-render, 1.85 ms input-to-submit |
| KOReader small book | 7.0 s cold open, 0 % measured steady CPU after ready |
| KOReader large book | 18.2 s cold open; 11.8 s of that is CRE first render |
| KOReader page turn | 11.0 % CPU at 0.5 s/page, 105 panel updates in 60 s |

Home is CPU-idle and immediately responsive. KOReader's cost is concentrated in
document layout/render and page painting, not in Home, storage, or idle polling.

## 7. What was not measured

- **Library index update** — NOT TESTED. No controlled index rebuild command was
  available without changing reader settings or library state.
- **Managed launch ready marker** — PARTIAL. A UI tap launched KOReader and the
  process reached the reader library, but the log did not expose an unambiguous
  "library interactive" marker. Startup totals above come from the direct
  runtime path, which uses the same binary/libraries.
- **I/O latency** — NOT MEASURED. `/proc/<pid>/io` was not exposed by this 3.0.35
  kernel for the reader process.
- **P50/P95/P99 interaction latency** — NOT TESTED for reader page turns. Only
  aggregate 0.5 s AutoTurn timing was measured. Home touch P50/P95 remains the
  Phase 1 physical dataset, not a statistically complete distribution.
- **Large library** — NOT TESTED. The device had four small documents.

## 8. Changes, deployment and rollback

- No reader, KOReader, UI or EPDC source change was made in Phase 2.
- No new package or dependency was added.
- The three temporary benchmark documents were removed from
  `/mnt/us/documents` after measurement, leaving the original library intact.
  They can be re-copied from the host sources recorded by hash above.
- Reader settings (`autoturn_*`) were restored from the pre-test backup.
- Rollback for the OS components is the Phase 2 P1 rollback in
  `PW3-PHASE2-RELIABILITY.md`; this workload did not alter them.

## 9. Ranked bottlenecks

1. **CRE first-render of the large EPUB — 11.84 s.** Dominates cold open.
2. **KOReader process startup — ~5.9 s before the document marker.** Small and
   large books share this cost; a small book is ready at 7.0 s.
3. **Page-turn CPU — 11 % at 0.5 s/page.** Expected for continuous rendering;
   not representative of normal human page cadence.
4. **Large-book memory — 51.9 MB RSS / 70.2 MB peak.** Fits the 512 MB device
   with ample headroom; no swap is needed.
5. **Storage — not limiting.** The 814 KB EPUB loads in 1.28 s; the PDF and
   small EPUB are faster still.

## 10. Reproduction

```sh
# Upload the launcher and documents, then from the device:
python3 tools/maintenance/pw3-audit.py --sample-seconds 60   # host-side idle audit

# On the device, with gw-system stopped:
/tmp/pw3-reader-bench.sh -d /mnt/us/documents/P2-Moby-Dick.epub

# For page turns, temporarily set:
#   autoturn_enabled=true
#   autoturn_timeout_seconds=0.5
# in /opt/koreader/settings.reader.lua, then measure EPDC IRQs and CPU for 60 s.
```

Raw files:

- `PW3-PHASE2-P3-inventory.txt`
- `PW3-PHASE2-P3-document-open.txt`
- `PW3-PHASE2-P3-debug-open.txt`
- `PW3-PHASE2-P3-pageturns.txt`
- `PW3-PHASE2-P3-repeat-memory.txt`
- `PW3-PHASE2-P3-managed-attempt.txt`

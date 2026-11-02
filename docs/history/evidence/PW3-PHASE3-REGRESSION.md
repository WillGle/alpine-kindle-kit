---
status: historical
date: 2026-11-02
superseded_by: ../../status/CURRENT.md
---

# PW3 Alpine — Phase 3 Regression and Rollback

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
Final boot ID: `a3028309-3468-4340-b81f-6b749caae64f`  
Access: physical USB CDC-ACM console (`0525:a4a7`)

**Status: PASS for the deployed system; no production reader/OS patch was made
in Phase 3.** The only reader change was a temporary test patch, removed after
the cache trials. The OS binaries are unchanged from the verified Phase 2 state.

## 1. What changed in Phase 3

| Item | Change | State |
|---|---|---|
| CRE cache investigation harness | temporary `/opt/koreader/patches/2-bench-exit.lua` | removed |
| Reader launcher | `tools/maintenance/pw3-reader-bench.sh` | repository only |
| Battery logger | `tools/maintenance/pw3-battery-logger.sh` | repository only |
| ext3 copy hardening | `tools/maintenance/pw3-ext3-harden-copy.sh` | repository only |
| Production `gw-shell` | none | unchanged |
| Production `gw-system` | none | unchanged |
| Production Wi-Fi script | none | unchanged |
| Filesystem mount/options | none | unchanged |
| Suspend | none | not tested |

The cache and benchmark files were removed from the device after measurement.
The original library contains only `PW3-Install-Check.epub`.

## 2. Cold versus warm reader baseline

| Measurement | Cold | Warm |
|---|---:|---:|
| Launch to first readable page | **18.05–22.26 s** | **5.41–6.43 s** |
| CRE loading | 1.278–1.280 s | 0.202–0.236 s |
| CRE rendering | 11.820–12.546 s | **0.063–0.076 s** |
| Cache after normal close | 829,440 B | unchanged |
| RSS | 51.9 MB (Phase 2) | **28.4 MB** |
| Peak RSS | 70.2 MB (Phase 2) | **28.4 MB** |

The warm path is **~12.8 s faster** than the 18.2 s cold path, a **66–70 %**
reduction. The cache is valid and persistent. The earlier repeated 18 s result
was caused by forced kills that left a zero-byte cache; it was not a CREngine
cache-lookup bug. Full details are in `PW3-PHASE3-CRE-CACHE.md`.

Cache files are removed at the end of Phase 3, so the **next** opening of a new
book will be cold. A normal close will create its cache and subsequent openings
will be warm. No cache-size increase was made.

## 3. Preserved Phase 1/2 behaviour

Final 120 s idle sample, after all measurements and cleanup:

| Metric | Result |
|---|---:|
| `gw-shell` CPU | **0.100 %** |
| Context switches | 80.0 /s |
| Interrupts | 124.0 /s |
| Log growth | **0 bytes** |
| EPDC GPIO updates | 2 in 120 s (**1/min**) |
| Root filesystem | 179,049 KiB / **71 %** |
| `/run` tmpfs | small, bounded |
| Wi-Fi | `wpa_state=COMPLETED`, `192.168.1.200` |
| DHCP | resident `udhcpc` PID 972 |
| Tailscale | daemon PID 715, authenticated peer ping 39 ms |
| `wifi-state` | **CONNECTED** |
| Guardian | `state=00000000` |
| USB console | working throughout |
| Home framebuffer | verified; clock, calendar, notes, shortcuts correct |

The context-switch count is 80/s versus 64.9/s in the Phase 2 final sample. The
`gw-shell` CPU share and EPDC rate are unchanged; the difference is small and
consistent with normal Wi-Fi/Tailscale activity. It is not an idle regression.

## 4. Event-driven touch and E-ink behaviour

- `gw-shell` is byte-identical to the verified Phase 2 binary:
  `6cde57047385b5cec4fb2da06d7156b41a489df258778819805d1f446f861c38`.
- The input thread, evdev poll path, LVGL timer period and EPDC partial-refresh
  code were not changed in Phase 3.
- The Phase 1 physical result remains the reference: **1.32–1.42 ms**
  input-to-render and **1.85 ms** input-to-submit.
- A new physical latency sample was not taken in Phase 3 because the production
  binary does not expose per-touch timing and no diagnostic build was deployed.
  This is recorded as **not re-measured**, not as a new PASS sample.
- One visible clock update per minute is preserved: 2 EPDC GPIO updates in 120 s.

## 5. Network and lifecycle regression

| Check | Result |
|---|---|
| Boot to `gw-system`/`gw-shell` | 16 s |
| Wi-Fi association | by 23 s |
| DHCP + DNS | PASS |
| Tailscale startup | one daemon at 10 s |
| Tailscale authenticated ping | PASS, 39 ms |
| Reader cold open | PASS |
| Reader warm open | PASS |
| Reader normal close / cache write | PASS |
| Reader forced kill with valid cache | PASS, cache preserved |
| Reader forced kill during first cache write | recovers after one normal close |
| Vietnamese glyph rendering | PASS, all diacritics visible |
| Guardian | PASS, state 0 |
| USB console | PASS |
| Home after cleanup | PASS |

## 6. Logs and storage

- `/run/gw-system.log` final size: **4,118 B**.
- Log growth during the 120 s idle window: **0 bytes**.
- Root filesystem: **71 %** used.
- CRE cache: empty after cleanup; it is regenerated on normal reader close.
- No cache size increase; default 64 MB cap remains. At ~0.8 MB per large book,
  a full 64 MB cache would consume most of the remaining root headroom; this is
  a policy risk, not a current failure.

## 7. Deployed artifact hashes

| Artifact | SHA-256 |
|---|---|
| `/usr/sbin/gw-shell` | `6cde57047385b5cec4fb2da06d7156b41a489df258778819805d1f446f861c38` |
| `/usr/sbin/gw-system` | `3ee82bb96728917369e7a3a7b84155c4f7bb04ae2427e84a21a1ce24928c57be` |
| `/etc/gleworks-wifi.sh` | `2a70dca301afd920f6977900ebe2b656fd447d6ecbe43c238c15f6ef81d6fe9c` |
| `/etc/gleworks-early.sh` | `85f15ac82fe4d42f65ca59cb23f93c3397f35d1a829868888612854c2182b4a1` |
| `/etc/gleworks-openrc.sh` | `c86827517374cacf04a4d7fa403a500553f2c65d80f854e8e2a1e30fd758f5d7` |

These are the Phase 2 verified values; Phase 3 did not replace them.

## 8. Rollback

Reader test state:

```sh
rm -f /opt/koreader/patches/2-bench-exit.lua
rmdir /opt/koreader/patches 2>/dev/null || true
```

(already done)

OS rollback if a later change is ever deployed:

```sh
# from rescue / validated recovery path only
cp -a /usr/sbin/gw-system.before-phase2-p1 /usr/sbin/gw-system
cp -a /etc/gleworks-wifi.sh.before-phase2-p1 /etc/gleworks-wifi.sh
# or replace system.ext3 with the recorded clean backup
```

Filesystem changes remain **undeployed**. The offline migration/rollback plan
is in `PW3-PHASE3-DURABILITY.md`.

## 9. Remaining limitations

1. No production filesystem change; ext3 remains writeback/barriers-off.
2. No real suspend/resume test.
3. No battery-only energy measurement.
4. No statistically complete physical touch sample in Phase 3; Phase 1 result is
   retained by binary identity.
5. The CRE cache can fill root space if many large books are opened and capped
   at the default 64 MB. No cap change was deployed.
6. A forced exit during the **first** cache build causes one cold reopen; the
   next normal close repairs it. This is acceptable but should be documented for
   recovery.

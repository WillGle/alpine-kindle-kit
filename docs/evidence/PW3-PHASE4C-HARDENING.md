# PW3 Phase 4C — Controlled Filesystem Hardening Acceptance

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
Kernel: `3.0.35-lab126`  
Deployment authorized explicitly by the owner in this session.  
Final boot ID: `062d91e3-7f5a-4fd9-83b0-44bb8e2dfd34`

**Overall result: PASS** — the copy-only hardened candidate was deployed,
booted, rebooted, functionally exercised, and left in production.

This is **not** complete power-loss protection. `barrier=0` remains, the FAT
backing container is unjournaled, and end-to-end flush ordering through
loop → FAT → eMMC is still unverified.

## 1. Scope and authorization

- Candidate: `673fa31ffaba546be2a04f8adba1885f71ea5ad57e6cb6986ebdc4c3d2277e5b`
- Change: `journal_data_ordered` + `errors=remount-ro`
- `barrier=0` retained
- p1, bootstrap, kernel, bootloader, Guardian, partition layout, and waveform
  data unchanged
- Deployment path: existing USB rescue + `kindle-linux.py deploy-root`
- Rollback image: `system-pre-hardening-20261031.ext3`

## 2. Gate A — Preflight: PASS

| Check | Result |
|---|---|
| Phase 4A/4B reports reviewed | PASS |
| Current production boot state | PASS — Alpine `820f14c4…`, Guardian 0 |
| Current root mount options | PASS — `data=writeback`, `errors=continue` |
| Fresh clean rescue snapshot | PASS — `9c935312c44da51e3d5effbdc2bb2f2360faa818814fb64272995c06ae36d5cf` |
| Snapshot `e2fsck` | PASS — clean |
| Candidate hash | PASS — `673fa31f…` |
| Candidate `e2fsck` | PASS — clean |
| Candidate vs current file content | PASS — only Tailscale cache/logs differed |
| Config preservation | PASS — all four config hashes matched preflight |
| Document preservation | PASS — EPUB and sidecar copied and hashed |
| FAT consistency before deployment | PASS — `(true,)` |
| p1 hash | PASS — `e50079c1…` unchanged |
| Known-good rollback image | PASS — fresh snapshot clean; Phase 4B physical restore/rollback already proven |
| Rescue access | PASS — p4 exported as `0525:a4a5` bcd0200 |
| Rollback tooling | PASS — `deploy-root`, token read-back, safe eject available |

Current-vs-candidate comparison:

- The candidate is a superblock-only copy of the earlier clean root
  `fa59de77…`.
- Fresh preflight root `9c9353…` differed from the candidate only in Tailscale
  `netmap-cache` files and `tailscaled.log1/2.txt`.
- All application/config/reader files compared identical, including
  `gleworks-early.sh`, `gw-shell`, `gw-system`, KOReader `native.lua`, and all
  four `/var/lib/gleworks` configs.
- No user book, sidecar, credential, or reader setting would be lost by the
  candidate; Tailscale reconnects and rebuilds its cache.

## 3. Gate B — Deployment: PASS

The existing protected workflow was used:

```text
rescue
  -> disable boot.enabled
  -> p4 exported independently
  -> FAT check (true,)
  -> deploy-root candidate 673fa31f…
  -> partial file copied, fsynced, hash-verified
  -> current system.ext3 renamed system-pre-hardening-20261031.ext3
  -> candidate renamed system.ext3
  -> boot.enabled written and read back
  -> safe eject
```

| Item | Value |
|---|---|
| Deployed candidate | `673fa31ffaba546be2a04f8adba1885f71ea5ad57e6cb6986ebdc4c3d2277e5b` |
| Pre-deployment active image | `34147cd303b9c3410c2526f885d0217439d88f630d38ef446c82c90d4b09e339` |
| Rollback backup | `system-pre-hardening-20261031.ext3`, same `34147c…` |
| Read-back | PASS |
| Safe eject | PASS |
| FAT consistency before deploy | `(true,)` |

No p1, kernel, bootloader, Guardian, partition, or waveform write occurred.

## 4. Gate C — First boot: PASS, with one mount-display caveat

First boot:

| Check | Result |
|---|---|
| Bootstrap selected the candidate | PASS — deployed hash verified, new boot |
| Boot ID | `6ecd3492-b5b2-4b59-80ab-f98bcfa3ff77` |
| Alpine reached Home | PASS |
| USB administration | PASS |
| `/proc/mounts` `data=ordered` | PASS — `rw,noatime,user_xattr,barrier=0,data=ordered` |
| `/proc/mounts` `errors=remount-ro` | **PARTIAL (display)** — not printed; effective superblock value is remount-ro |
| Effective `errors` behavior | PASS — deployed image superblock: `Errors behavior: Remount read-only` |
| Unexpected ext3 errors | PASS — none |
| Root writable | PASS — `touch /run/...` succeeded |
| Guardian | PASS — `state=00000000` |
| Kernel log | `EXT3-fs (loop1): mounted filesystem with ordered data mode` |

### 4.1 Why `/proc/mounts` omits `errors=remount-ro`

The running mount line is:

```text
/dev/loop1 / ext3 rw,noatime,user_xattr,barrier=0,data=ordered 0 0
```

`errors=remount-ro` is absent because ext3’s `ext3_show_options()` only prints
`errors=remount-ro` when the superblock default is currently `panic` or
`continue`. When the superblock itself already says `remount-ro`, the option is
the default and is suppressed from `/proc/mounts`.

The deployed image was copied back through rescue and checked on the host:

```text
Default mount options: journal_data_ordered user_xattr acl
Errors behavior:       Remount read-only
Filesystem state:      clean
```

Therefore the effective superblock setting is `errors=remount-ro`. This is
**not** treated as a mount-option failure. The runtime transition to read-only
after a real ext3 error was **not** fault-injected, because that would require
deliberately corrupting or injecting an error into production storage.

## 5. Gate D — Functional regression

| Test | Result | Evidence |
|---|---|---|
| Home rendering | **PASS** | final framebuffer shows clean Home |
| Touch input | **PASS** | instrumented tap closed Wi-Fi panel; Settings/Home navigation worked |
| Calendar | **PASS** | Saturday, 31 October shown correctly |
| Notes | **PASS** | “No notes yet”; no unexpected note data |
| Time | **PASS** | `Sat Oct 31 03:54 UTC 2026`, RTC epoch consistent |
| Timezone / format | **PASS** | `UTC`, 24-hour |
| Frontlight | **PASS** | 14 → 12 → 14; `light.conf` hash restored |
| Wi-Fi | **PASS** | connected to `Olak Coffee`, WPA2, IP `192.168.100.56` |
| DHCP / route / DNS | **PASS** | default via `192.168.100.1`; DNS resolves; ping 1.1.1.1 succeeds |
| Tailscale | **PASS** | `alpine-reader` at `100.100.178.75`; netcheck UDP/IPv4/DERP pass |
| KOReader launch | **PASS** | disposable book opened |
| KOReader normal exit | **PASS** | `P4C_NORMAL_CLOSE`, exit code 0, writer/UI shutdown clean |
| Sidecar write | **PASS** | `metadata.epub.lua` created, 2,701 B |
| Reading data preservation | **PASS** | original EPUB and sidecar hashes unchanged |
| Reading-progress page turn | **PARTIAL** | sidecar persistence proven; no page-turn position change was made |
| Clean reboot | **PASS** | `6ecd3492…` → `499bfdf1…` |
| USB rescue after deployment | **PASS** | p4 exported; rollback image verified; resumed |
| Idle CPU | **PASS** | 99.58 % idle; `gw-shell` 0.100 % |
| Idle logging | **PASS** | 0 bytes growth over 120 s |
| EPDC/PXP idle behavior | **PASS** | 2 PXP interrupts / 120 s = 1/min; no errors |
| Frontlight/EPDC visual state | **PASS** | Home and Wi-Fi panel rendered correctly |

### 5.1 Wi-Fi note

The preflight saved profile was SSID `Will` (`wifi.conf` hash
`9ddab56b…`). That AP was not visible in the scan. During the acceptance
window the device connected to the available `Olak Coffee` profile and the
current `wifi.conf` became `b3e3c186…`; `wifi-enabled.conf` is now `enabled=1`
(hash `16f4affa…`). The original preflight config is preserved as
`gw-config-preflight.tar` (`39378e89…`). This is a user-visible config change,
not a lost credential or a filesystem failure.

### 5.2 Reader test detail

A disposable copy of the existing EPUB was created as
`PW3-Acceptance-Test.epub`. KOReader opened it, logged:

```text
loading took 0.071 seconds
rendering took 0.125 seconds
Restoring user input handling
P4C_NORMAL_CLOSE
DocSettings: Writing to /mnt/us/documents/PW3-Acceptance-Test.sdr/metadata.epub.lua
```

The test book hash stayed `57e74396…`; the test sidecar was written; the test
book, sidecar, and temporary exit patch were then removed. The original
`PW3-Install-Check` book and sidecar remain unchanged.

## 6. Performance comparison

Measured after clean reboot, with Wi-Fi/Tailscale connected and no user input:

| Metric | Phase 3 baseline | Phase 4C hardened | Result |
|---|---:|---:|---|
| Idle CPU | ~95–99.6 % depending on window | **99.58 % idle** | PASS |
| `gw-shell` CPU | 0.100 % | **0.100 %** | PASS |
| `gw-system` CPU | ~0.02 % | **0.017 %** | PASS |
| EPDC/PXP idle updates | 1/min | **1/min** | PASS |
| `gw-system.log` growth | 0 bytes / 120 s | **0 bytes / 120 s** | PASS |
| Load average | low | **0.02 / 0.04 / 0.02** | PASS |
| MemFree | — | 319.8 MB | PASS |
| Ext3/I/O errors | none expected | **none observed** | PASS |

The 60-second window measured 6,004 total jiffies, of which 5,979 were idle
(99.58 %). `gw-shell` used 6 jiffies (0.100 %). The 120-second window measured
2 PXP interrupts (1/min), 12,491 context switches, and zero log growth.

No performance regression attributable to `data=ordered` was observed at idle.
No write-throughput claim is made.

## 7. Gate E — Rollback readiness

| Item | Result |
|---|---|
| Verified rollback image | `system-pre-hardening-20261031.ext3` |
| SHA-256 | `34147cd303b9c3410c2526f885d0217439d88f630d38ef446c82c90d4b09e339` |
| Size | 268,435,456 B |
| `e2fsck -fn` | clean |
| Superblock | `user_xattr acl`, errors continue, writeback |
| On-device copy | present on p4 |
| Host copy | `system-rollback-pre-hardening-20261031.ext3` |
| FAT consistency | `(true,)` at final rescue |
| Rescue access | PASS |
| Actual rollback executed | **NOT TESTED** — not performed because acceptance passed; production was not deliberately corrupted |

### 7.1 Exact rollback procedure

```text
1. From Alpine: kindle-linux.py rescue
2. Confirm p4 identity: 0525:a4a5 bcd0200
3. Unmount p4; run UDisks2 Filesystem.Check; expect (true,)
4. Verify rollback image:
     sha256sum system-pre-hardening-20261031.ext3
     e2fsck -fn system-pre-hardening-20261031.ext3
   Expected: 34147cd303b9c3410c2526f885d0217439d88f630d38ef446c82c90d4b09e339
5. Preserve the active hardened image under a new name:
     mv system.ext3 system-hardened-<date>.ext3
6. Restore:
     mv system-pre-hardening-20261031.ext3 system.ext3
7. Verify hash and e2fsck again
8. Remove stale boot.pending / boot.repairing / boot.once only after inspection
9. Write exactly "gleworks-alpine-v1\n" to boot.enabled
10. fsync file and directory; read token back
11. Unmount p4 and SCSI eject
12. Verify new boot ID, PID1 /bin/busybox, Guardian 0, Home, Wi-Fi, Tailscale
```

### 7.2 Boot-failure behavior

- Missing/invalid/dirty `system.ext3` → bootstrap selects USB rescue.
- `boot.repairing` present at next boot → rescue.
- Guardian deadline 90 s; early startup failure can reboot and leave
  `boot.repairing`.
- After early startup, Guardian is cancelled and `errors=remount-ro` does not
  automatically reboot or force rescue. Manual rescue entry remains:
  USB console `RESCUE` or power-cycle then rescue.
- p1 failure remains outside p4 USB recovery.

### 7.3 User-data restoration

- `/mnt/us/documents` is on p4/FAT and is not part of `system.ext3`; rolling back
  the root does not remove books or sidecars.
- Preflight and post-acceptance config tars are on the host:
  `gw-config-preflight.tar` and `gw-config-after.tar`.
- Original preflight config: `39378e891d6dab2964519eec0e456fb3bd318e2ebeb4cac0f6125a9f5ad2a324`.
- Post-acceptance config: `bae0bd615245508aafc381c1f5a49d16fb28ca1761faa417d344e81bba11c6bb`.
- If a rebuilt image is ever used, restore `/var/lib/gleworks` explicitly.

## 8. Final state

| Check | Result |
|---|---|
| Boot ID | `062d91e3-7f5a-4fd9-83b0-44bb8e2dfd34` |
| PID1 | `/bin/busybox` |
| Root | `/dev/loop1` ext3, `rw,noatime,user_xattr,barrier=0,data=ordered` |
| Effective errors | `Remount read-only` (superblock; hidden in `/proc/mounts`) |
| Root used | 179,123 KiB / 253,871 KiB / 71 % |
| `boot.enabled` | `gleworks-alpine-v1` |
| `boot.pending` | present (normal runtime marker) |
| `boot.once` / `boot.repairing` | absent |
| Guardian | `state=00000000` |
| Wi-Fi | connected, `192.168.100.56` |
| Tailscale | connected, `100.100.178.75` |
| Home | rendered |
| p4 FAT | 2,296,320 KiB used / 892,320 KiB free / 72 % |
| p1 image | 471,859,200 B, `e50079c1…`, unchanged |
| Hardened active image | candidate `673fa31f…`, post-boot snapshot `75ef18c2…` |
| Rollback image | `34147c…`, clean |

No p1, bootloader, kernel, Guardian, partition, or waveform partition was
modified. The hardened image is deployed and active.

## 9. Outstanding risks

| Risk | Status / limitation |
|---|---|
| `barrier=1` / power-loss ordering | **NOT PROVEN.** `barrier=0` retained; loop→FAT→eMMC flush ordering unverified |
| FAT container | Unjournaled. A prior USB-reset inconsistency was repaired; this remains a real failure mode |
| `errors=remount-ro` runtime transition | **NOT TESTED** by fault injection; option is effective in the superblock |
| Post-early automatic rescue | No watchdog; `errors=remount-ro` does not reboot or force rescue after Guardian is cancelled |
| p1 failure | No p4-only recovery path; raw p1 image and host copy exist, but p1 write/restore is not demonstrated |
| Reading-progress page turn | Sidecar write verified; page-turn position persistence not exercised on the disposable copy |
| Wi-Fi configuration | Changed from saved `Will` to `Olak Coffee` during acceptance; original tar retained |
| Historical `system-before-rca` | Clean `e2fsck` fails; never use as rollback |
| A/B boot | Not available; single `system.ext3` slot plus named backups |

## 10. Evidence index

Public:

- `docs/evidence/PW3-PHASE4C-HARDENING.md`
- `docs/evidence/PW3-PHASE4C-HARDENING.json`
- `docs/evidence/raw/PW3-PHASE4C-*`

Key raw files:

- `PW3-PHASE4C-state-before.txt`
- `PW3-PHASE4C-rescue-preflight.txt`
- `PW3-PHASE4C-preflight-sha.txt`
- `PW3-PHASE4C-tree-diff.txt`
- `PW3-PHASE4C-fat-check-preflight.txt`
- `PW3-PHASE4C-deploy.json`
- `PW3-PHASE4C-first-boot.txt`
- `PW3-PHASE4C-mount-effective.txt`
- `PW3-PHASE4C-wifi-retest.txt`
- `PW3-PHASE4C-tailscale.txt`
- `PW3-PHASE4C-reader-test.txt`
- `PW3-PHASE4C-clean-reboot.txt`
- `PW3-PHASE4C-post-reboot.txt`
- `PW3-PHASE4C-idle-60.txt`
- `PW3-PHASE4C-idle-120.txt`
- `PW3-PHASE4C-perf-summary.txt`
- `PW3-PHASE4C-rescue-final.txt`
- `PW3-PHASE4C-fat-check-final.txt`
- `PW3-PHASE4C-final-state.txt`
- `PW3-PHASE4C-final-ready.txt`

Private host copies (archived 2026-11-01 as
`work/private/evidence/archive-20261101/phase4c-20261031.tar.zst`):

```text
work/private/evidence/phase4c-20261031/
  images/system-preflight-20261031.ext3
  images/system-hardened-deployed-20261031.ext3
  images/system-rollback-pre-hardening-20261031.ext3
  config/gw-config-preflight.tar
  config/gw-config-after.tar
  meta/...
  screens/final-home.png
```

# PW3 Alpine — Phase 2 Reliability, Storage Integrity and Power

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
OS: Alpine 3.24.3, musl, kernel `3.0.35-lab126`  
Final boot ID: `f4c54f56-4721-427f-8e2d-48996a78aae1`  
Access: physical USB CDC-ACM console (`0525:a4a7`)  
Scope: P0 filesystem integrity, P1 network reliability, P2 suspend design, P4
storage, P5 power.

**Overall: P1 and P4 pass on hardware. P0 is analysis plus an offline-validated
build change only; it was not deployed and no power-loss test was run. P2 is
design-only. P5 is NOT TESTED because no battery-only measurement was possible
with the device on USB.**

## 1. Status summary

| Priority | Status | Deployed? | Basis |
|---|---|---|---|
| P0 filesystem/data integrity | **PARTIAL** | No | topology/config/source analysis; disposable-image validation; rollback plan |
| P1 network reliability | **PASS** | Yes | scan, auth, DHCP, DNS, reconnect, reboot, wrong credentials, persistent config, Tailscale reconnect |
| P2 suspend/resume | **NOT TESTED (design only)** | No | audit + gated test plan; no suspend transition executed |
| P3 active workloads | **PARTIAL** | n/a | see `PW3-PHASE2-ACTIVE-PERF.md` |
| P4 storage | **PASS** | Yes | 14.8 MB of regenerable cache removed; fonts/firmware/credentials/data preserved |
| P5 power | **NOT TESTED** | No | battery full, `current_now=0`, USB attached; no energy measurement |

No bootloader, kernel, Guardian, partition layout, recovery partition, USB
recovery path, Vietnamese font, credential, book or reader data was modified.

## 2. P0 — filesystem and data integrity

### 2.1 Measured topology (read-only)

| Layer | Observation |
|---|---|
| eMMC | `mmcblk0`, 3.64 GiB Samsung `4FPD3R` |
| Partitions | `p1` 460,800 kB, `p2` 65,536 kB, `p3` 65,536 kB, `p4` 3,192,832 kB |
| Control/FAT | `/dev/loop0` vfat, 3.19 GB, backing `/dev/mmcblk0p4` at offset 8,192; mounted `/boot/gleworks-control` |
| Alpine root | `/dev/loop1` ext3, 262,144 × 1 KiB blocks, backing `/boot/gleworks-control/gleworks-alpine/system.ext3` |
| Root mount | `rw,noatime,errors=continue,user_xattr,barrier=0,data=writeback` |
| Kernel command line | `... root=/dev/mmcblk0p1 ro ... console=ttymxc0,115200 ...` |
| Superblock | `has_journal filetype sparse_super large_file`; 4 MiB internal journal; `errors=continue`; default `user_xattr acl`; 1 KiB blocks; 128-byte inodes |
| Boot sequence | bootstrap maps p4 → vfat loop0, opens `system.ext3`, maps ext3 loop1, mounts it **read-only**, pivots, then `gleworks-early.sh` remounts `/` rw after Guardian cancel |
| Shutdown | `/gleworks/shutdown` syncs, unmounts Alpine root, writes `shutdown.log`, clears `boot.pending`, unmounts FAT |
| Guardian | `boot.pending` is retained across normal guarded boots and cleared only after an orderly shutdown; dirty image triggers offline `e2fsck -p` |

`/proc/filesystems` and the kernel config explain the observed options:

```
work/build/pw3-kernel-headers/.config:
# CONFIG_EXT3_DEFAULTS_TO_ORDERED is not set
```

Kernel 3.0.35 therefore defaults ext3 to **writeback**. The bootstrap mount does
not request `data=ordered` or `barrier=1`. The ext3 journal code only enables
barriers when the `BARRIER` mount option is set; dmesg confirms:

```
EXT3-fs: barriers not enabled
EXT3-fs (loop1): mounted filesystem with writeback data mode
```

The loop driver can forward `REQ_FLUSH` to `vfs_fsync()` of the backing file when
the backing filesystem supplies `.fsync` (FAT does), but this is **not** proof
that the eMMC write cache is flushed end-to-end. The backing FAT itself is
unjournaled. Barrier effectiveness through loop→FAT→eMMC remains **NOT
VERIFIED** and is not claimed.

### 2.2 Power-loss exposure

1. Ext3 metadata is journaled, but file data is not ordered before the journal
   commit (`data=writeback`). After power loss, files can contain stale or
   uninitialised blocks even when the filesystem is otherwise consistent.
2. `errors=continue` allows writes to continue after a detected filesystem
   error. That is a corruption-amplification risk for a root filesystem.
3. The ext3 image is a regular file inside unjournaled FAT. A FAT/power-loss
   event can damage the image container itself, independent of the ext3 journal.
4. Guardian reduces damage but is not a substitute for data ordering: a dirty
   image leads to offline `e2fsck`, and an unavailable/insufficient battery or a
   failed repair forces USB rescue.
5. The existing repair log contains multiple `recovering journal` and one
   `Clearing orphaned inode` line. Those prove the recovery path has been used;
   they are not a controlled power-loss experiment.

No power interruption was performed on the production PW3. The current root was
never remounted with different options.

### 2.3 Offline validation on disposable images

A copy of `work/images/releases/pw3/alpine-reader-v4.ext3` was created under
`/tmp/p2-fs` and modified with host e2fsprogs 1.47.4:

```sh
cp --reflink=auto alpine-reader-v4.ext3 system-ordered.ext3
tune2fs -o journal_data_ordered system-ordered.ext3
e2fsck -fn system-ordered.ext3
tune2fs -e remount-ro system-safe.ext3
e2fsck -fn system-safe.ext3
```

| Test image | Default mount options | Errors behavior | e2fsck |
|---|---|---|---|
| Original | `user_xattr acl` | Continue | clean |
| `system-ordered.ext3` | `journal_data_ordered user_xattr acl` | Continue | clean |
| `system-safe.ext3` | `journal_data_ordered user_xattr acl` | Remount read-only | clean |

A fresh 16 MiB ext3 image built with the same `mkfs.ext3` feature set and then
`tune2fs -o journal_data_ordered -e remount-ro` also passed `e2fsck -fn`.

**Source change (not deployed).** Both image builders now apply those defaults
to every newly built image:

- `tools/build/pw3-build.py`
- `tools/build/pw3-os-trial.py`

No running filesystem or image was altered by that change. The build still uses
1 KiB blocks and 128-byte inodes, matching the existing device.

### 2.4 Migration and rollback plan (before any deployment)

1. Boot the device to rescue/safe mode, verify USB identity and nonce, and copy
   the current `system.ext3` to a new named backup on FAT. Do not overwrite an
   existing backup.
2. On the host, copy that backup again; run `e2fsck -fn` and record its SHA-256.
3. Produce a new image offline with ordered data and `errors=remount-ro`; run
   `e2fsck -fn`; verify the manifest and image hash; boot it read-only first.
4. Deploy only through `tools/maintenance/kindle-linux.py deploy-root` with a new
   `--backup-name` and evidence file. Keep the pre-change image indefinitely.
5. Acceptance: clean boot, Guardian cancel, auto-start, Home render, Wi-Fi,
   Tailscale, KOReader open/close, clean reboot, and a controlled offline
   `e2fsck` of a copy.
6. Rollback: boot rescue, replace `system.ext3` with the recorded backup, verify
   the hash and `e2fsck`, clear pending only after the replacement is proven
   clean, then reboot.
7. Do **not** deploy `barrier=1` until a disposable-image power-cut matrix proves
   flush ordering through loop→FAT→eMMC. No such test is claimed here.

## 3. P1 — network reliability

### 3.1 Baseline defects

| Finding | Evidence |
|---|---|
| DHCP client exited immediately after the first lease (`udhcpc -b -q`) | no `udhcpc` process after boot; `/etc/gleworks-wifi.sh` line 26 |
| Lease renewal was therefore impossible | `udhcpc` absent, no lease file |
| Status reported `wifi-state=OFF` while Wi-Fi was connected | boot-time Wi-Fi is started outside `gw-system`; service state initialized OFF |
| Reconnect required an explicit `wpa_cli reconnect` after `disconnect` | expected for an explicit disconnect, but DHCP had no resident client |
| Tailscale was authenticated, not logged out | `tailscale debug prefs` → `"LoggedOut": false` |

### 3.2 Changes

1. `os/system/gleworks-wifi.sh`:
   - removed `-q` from `udhcpc`;
   - keeps `udhcpc` resident so it renews leases and reacquires an address;
   - added `stop_dhcp()` with PID and command-line validation;
   - `start` stops any previous DHCP client before starting a new one;
   - `stop` reuses the same safe stop path.
2. `os/system/gw-system.c`:
   - at startup, if the link is already connected, record
     `GW_WIFI_STATE_CONNECTED` and do not start a duplicate supplicant;
   - on `STATUS`, reconcile `wifi-state` with the actual link so a boot-time
     connection is not reported as OFF.

Nothing in the changes delays or blocks the Home UI: the Wi-Fi child still runs
as a separate process and the IPC handler returns immediately.

### 3.3 Hardware tests

| Test | Result | Evidence |
|---|---|---|
| Scan | **PASS**, `rc=0`, 4+ APs returned | `PW3-PHASE2-P1-audit.txt` |
| Authentication | **PASS**, `wpa_state=COMPLETED` | same |
| DHCP | **PASS**, `192.168.1.200/24`, resident `udhcpc` PID 964 after reboot | final reboot |
| DNS | **PASS**, `example.com` returned A records | final reboot |
| TCP reachability | **PASS**, 2/2 ICMP to 1.1.1.1 | reconnect tests |
| Explicit disconnect | **PASS**, state DISCONNECTED, ping fails; stale IP retained | `P1-reconnect-before.txt` |
| Reconnect | **PASS**, state COMPLETED, IP restored, 0 % loss | `P1-reconnect-after.txt` |
| Lease renewal | **PASS**, SIGUSR1 renewed without losing PID/IP; PID file updated | `P1-dhcp-pid.txt` |
| Reboot persistence | **PASS**, boot `f4c54f56…`: Wi-Fi at 13 s, IP and DNS by 23 s | `P1-final-reboot.txt` |
| Wrong credentials | **PASS**, temporary non-existent SSID stayed SCANNING, no IP, network unreachable; real profile restored | `P1-bad-credentials.txt` |
| Persistent config | **PASS**, `/var/lib/gleworks/wifi.conf` mode 0600, one network, SSID redacted | audit |
| Status consistency | **PASS**, `wifi-state=CONNECTED` after final boot | final reboot |
| Tailscale startup | **PASS**, one daemon at 10 s, no entropy panic | final reboot |
| Tailscale authenticated | **PASS**, `LoggedOut=false`, peers listed | `P1-tailscale.txt` |
| Tailscale reconnect | **PASS**, after Wi-Fi drop it fell back to relay; after reconnect direct 5 ms pong | `P1-tailscale-reconnect.txt` |

The Tailscale outage test deliberately used `wpa_cli disconnect`, i.e. a full
link-down event at the station. During the outage `tailscale ping` timed out;
after Wi-Fi recovered, the same daemon was still running and a peer ping
succeeded over a direct path. This is a real reconnect result, not a logged-out
claim.

`udhcpc` PID changed from 547 to 825 after a renewal signal, and the PID file
was rewritten to 825; the stop path then terminated 825 cleanly. The final boot
uses PID 964.

### 3.4 Deployed artifacts and rollback

| Artifact | SHA-256 |
|---|---|
| `/usr/sbin/gw-system` | `3ee82bb96728917369e7a3a7b84155c4f7bb04ae2427e84a21a1ce24928c57be` |
| `/etc/gleworks-wifi.sh` | `2a70dca301afd920f6977900ebe2b656fd447d6ecbe43c238c15f6ef81d6fe9c` |
| `/usr/sbin/gw-shell` (unchanged Phase 1) | `6cde57047385b5cec4fb2da06d7156b41a489df258778819805d1f446f861c38` |
| `/etc/gleworks-early.sh` (unchanged Phase 1) | `85f15ac82fe4d42f65ca59cb23f93c3397f35d1a829868888612854c2182b4a1` |
| `/etc/gleworks-openrc.sh` (unchanged Phase 1) | `c86827517374cacf04a4d7fa403a500553f2c65d80f854e8e2a1e30fd758f5d7` |

Rollback:

```sh
cp -a /usr/sbin/gw-system.before-phase2-p1 /usr/sbin/gw-system
cp -a /etc/gleworks-wifi.sh.before-phase2-p1 /etc/gleworks-wifi.sh
kill -TERM $(pidof gw-system)
GW_SYSTEM_LOG=/run/gw-system.log /usr/sbin/gw-system --supervise &
```

Backup hashes:

- `/usr/sbin/gw-system.before-phase2-p1`:
  `54103f9bb6d260c57af20cc98d879af1d63fcebb56bae8634837923a21c54ae3`
- `/etc/gleworks-wifi.sh.before-phase2-p1`:
  `4de4af6906062766da7bea25813759c121354dbc2a1c48791de0d87d0b74fee2`

## 4. P2 — suspend/resume audit and test plan

### 4.1 Audit findings (read-only)

| Item | Finding |
|---|---|
| Supported states | `/sys/power/state` = `standby mem` |
| Autosleep | `0` (disabled) |
| `pm_test` | available: `[none] core processors platform devices freezer` |
| Wakeup sources | `snvs_rtc.0`, `wario_hall`, `fsl-usb2-otg`, `fsl-usb2-udc` |
| Wake IRQs | 51 `snvs_rtc`; 74/75 `usb_wakeup`; GPIO wake bank |
| RTC | `max77696-rtc.0` at `/sys/class/rtc/rtc0`; wakealarm node present |
| Power button | `/dev/input/event0`, `max77696-onkey` |
| Touch | `/dev/input/event1`, `cyttsp4_mt` |
| Frontlight | `max77696-bl`, current 573/4095 (14 %) |
| EPDC/framebuffer | `fb0` power control `auto`, runtime status `unsupported` |
| Wi-Fi PM | `auto`; supplicant/DHCP/Tailscale are userspace processes |
| USB | CDC-ACM gadget; suspend will remove the administration console |
| Prior probes | only `pm_test` / devices-suspend, not a validated real memory sleep |

The prior `pm-mem-probe.log` enters and leaves suspend in ~0.4 s with an RTC alarm
set 20 s later. The devices probe explicitly logs `suspend debug: Waiting for 5
seconds`. These are `pm_test` runs; they do not prove that real `mem` suspend
resumes correctly, preserves the panel/frontlight, or keeps USB recovery
available.

### 4.2 Gated test plan (not executed)

Preconditions before any transition:

1. Clean `e2fsck`, current `system.ext3` backed up twice off-device and once on
   FAT under a new name; rollback hash recorded.
2. Battery ≥ 80 %, charger attached during the first experiments, Guardian
   `state=00000000`, `boot.pending` understood.
3. RTC wake alarm proven with a **non-suspend** read/write test.
4. Power-button wake proven using a screen-off/on cycle without suspend.
5. USB console recovery path written down and tested by a clean reboot.
6. `sync` immediately before suspend; no document open and no writes in flight.

Sequence, stopping at the first failure:

| Stage | Action | Pass condition |
|---|---|---|
| 0 | `pm_test=freezer`, `echo mem > /sys/power/state` | immediate resume, shell/console alive |
| 1 | `pm_test=devices`, RTC +60 s | resumes at alarm, no lost devices, panel intact |
| 2 | `pm_test=platform` | resumes, no kernel fault, USB console returns |
| 3 | `standby` (shallow) with RTC +60 s | resumes at alarm; frontlight/panel state checked |
| 4 | `mem` with RTC +60 s and power button held ready | resumes; console returns; Wi-Fi/Tailscale recover |
| 5 | repeat `mem` 5×, then a 10-min sleep | no leak, no lost wake source, no filesystem repair needed |

For every stage record boot ID, uptime before/after, RTC alarm, wake IRQ, panel
image, frontlight sysfs, Wi-Fi state, Tailscale state, USB enumeration, and
`e2fsck` of a copied image after a clean shutdown.

**Do not execute any stage while the only recovery path is the USB console that
suspend itself may remove.** No such transition was executed in this phase.

## 5. P4 — storage optimization

### 5.1 Audit

Before cleanup:

```
/dev/loop1  253,871 KiB total  193,684 KiB used  60,187 KiB free  76 %
```

Largest consumers:

| Path | Size |
|---|---:|
| `/usr/sbin/tailscaled` | 35,797 kB |
| `/usr/lib/python3.14` | 29,488 kB |
| `/opt/koreader` | 68,616 kB |
| `gdb` | 6,794 kB |
| `/usr/lib/debug` (musl debug symbols) | 2,886 kB |
| `/var/cache/apk` | 2,791 kB |
| `/opt/koreader/fonts/noto/NotoSansCJKsc-Regular.otf` | 16,092 kB |
| `/opt/koreader/fonts` total | 25,076 kB |

Preserved deliberately: all fonts (including Vietnamese-capable fonts), firmware
modules, USB/Guardian recovery assets, `/var/lib/gleworks` credentials and
configuration, KOReader data/settings/statistics, books and sidecars.

### 5.2 Change and result

Only regenerable caches were removed:

- `/var/cache/apk/*` (package index cache)
- `__pycache__` / `*.pyc` under `/usr/lib/python3.14`

| Metric | Before | After | Recovered |
|---|---:|---:|---:|
| Root used | 193,684 KiB | 178,883 KiB | **14,801 KiB** |
| Root free | 60,187 KiB | 74,988 KiB | **14,801 KiB** |
| Root use | 76 % | 70 % | **−6 percentage points** |

Recovered space: **15,156,224 bytes**, **5.83 %** of the 253,871 KiB filesystem,
or **7.64 %** of the pre-cleanup used space. After subsequent diagnostic Python
use, two small cache directories were regenerated and the final root is
179,053 KiB / 71 %; the recovery remains >14.6 MB.

Not removed (documented candidates, not proven unused):

- `gdb` + `musl-dbg` (~9.7 MB): useful crash diagnostics, not required at runtime.
- Noto CJK font (16.1 MB): deliberately preserved; removing it could break CJK
  book rendering and was not requested.
- Python itself: on-device maintenance tooling; not removed.
- Tailscale: required network service; not removed.

No package database entry, firmware, font, credential or reader data was
deleted. Rollback is cache regeneration (`apk update`, normal Python import);
there is no user data to restore.

## 6. P5 — power benchmark

### 6.1 Instrumentation result

With USB attached and the battery full:

```
max77696-battery:
  status=Full  capacity=99  voltage_now=4201  current_now=0
  charge_now=1464  temp=93
max77696-charger: online=0
max77696-uic: present=1
```

`current_now` reports **0** and the device is on USB power. A meaningful
battery-only comparison (Home idle, KOReader idle, page turns, Wi-Fi,
Tailscale, frontlight, suspend) cannot be made in this state. The existing
KOReader battery statistics are stale and contain no discharge interval.

**P5 status: NOT TESTED.** No battery-life claim is made. CPU frequency and idle
percentages are not energy measurements.

### 6.2 Required battery-only procedure

1. Record `charge_now`, `voltage_now`, `capacity`, uptime and CPU counters.
2. Unplug USB; run an on-device logger in `/tmp` (no console available).
3. Hold each state for 10 minutes: Home idle at light 14; Home idle at light 0;
   KOReader reading idle; repeated AutoTurn page turns; Wi-Fi on/off; Tailscale
   on/off. Do not change more than one variable per interval.
4. Reconnect USB, read the logger, compute mAh/minute and mWh/minute from
   `charge_now`/`voltage_now`, and discard intervals where the charger/USB
   state changed.
5. Keep `performance` governor unless a repeatable touch-latency test shows no
   regression from an alternative.

No suspend interval can be included until P2 passes.

## 7. Regression and final state

Final clean boot `f4c54f56-4721-427f-8e2d-48996a78aae1`:

| Check | Result |
|---|---|
| `gw-system` / `gw-shell` auto-start | PASS, both at 16 s |
| Wi-Fi + IPv4 + DHCP + DNS | PASS, by 23 s |
| `wifi-state` | PASS, CONNECTED |
| Tailscale daemon | PASS, one PID 715 at 10 s |
| Tailscale authenticated ping | PASS, 38 ms direct |
| Home diag socket | PASS, `PING` → `OK` |
| Idle 120 s: `gw-shell` CPU | **0.100 %** |
| Idle 120 s: context switches | **64.9 /s** |
| Idle 120 s: IRQs | 117.3 /s |
| Idle 120 s: log growth | **0 B/s** |
| Idle 120 s: EPDC GPIO updates | 2 in 120 s (1/min clock) |
| Root filesystem | 179,053 KiB used / 71 % |
| `/run` tmpfs | 216 KiB used / 0 % |

Phase 1 gains are preserved. The final log is bounded and currently 655 B after
the clean boot.

## 8. Remaining risks

1. **Ext3 writeback + no verified barriers remains deployed.** This is the
   highest data-integrity risk; the safer build is validated offline but not
   deployed.
2. **FAT backing image.** A power-loss event can corrupt the container before
   ext3 recovery runs.
3. **`errors=continue` remains deployed.**
4. **P2 is unproven.** No suspend transition or wake path has been accepted.
5. **P5 is unmeasured.** Battery drain and suspend energy are unknown.
6. **P3 large-EPUB first render is application-bound.** OS tuning will not fix
   an 11.8 s CRE render.

## 9. Reproduction

- Filesystem: `tune2fs -o journal_data_ordered -e remount-ro <image copy>` then
  `e2fsck -fn`; never on the mounted production root.
- Network: `tools/maintenance/pw3-network-check.sh` on the device, with
  `--reconnect` and `--bad-credentials` as explicit destructive tests.
- Reader workload: `tools/maintenance/pw3-reader-bench.sh -d <document>`.
- Raw logs: `docs/evidence/raw/PW3-PHASE2-*.txt`.

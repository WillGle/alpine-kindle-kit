# PW3 Alpine — Phase 3 Filesystem Durability, Suspend Readiness and Battery Logging

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
Kernel: `3.0.35-lab126`  
Final boot ID: `a3028309-3468-4340-b81f-6b749caae64f`  
Scope: P2 filesystem durability, P3 suspend readiness, P4 battery logger.

**Status: P2 PARTIAL — offline validation and architecture only, production
unchanged. P3 NOT TESTED — read-only audit complete, no suspend executed. P4
PARTIAL — logger implemented and tested on USB power; battery-only run not
executed.**

No partition layout, bootloader, kernel, Guardian, recovery partition or
production root mount was changed.

## 1. P2 — filesystem durability

### 1.1 Deployed topology

| Layer | Observation |
|---|---|
| eMMC | Samsung `4FPD3R`, 3.64 GiB, `mmcblk0` |
| Control/FAT | `loop0` vfat over `mmcblk0p4` at offset 8192 |
| Alpine root | `loop1` ext3 over `system.ext3` inside that FAT |
| ext3 mount | `rw,noatime,errors=continue,user_xattr,barrier=0,data=writeback` |
| Superblock | 1 KiB blocks, 4 MiB internal journal, `errors=continue`, default `user_xattr acl` |
| Kernel defaults | `CONFIG_EXT3_DEFAULTS_TO_ORDERED` is **not set** |
| Barrier state | dmesg: `EXT3-fs: barriers not enabled` |
| FAT mount | no `flush` option in `/proc/mounts` |
| eMMC queue | no `write_cache`, no `fua` sysfs attributes |
| Loop queue | no `write_cache` / `fua` sysfs attributes |

### 1.2 What the stack can and cannot guarantee

Kernel 3.0.35 loop code advertises `REQ_FLUSH` when the backing file supports
`.fsync`:

```c
if (!(lo_flags & LO_FLAGS_READ_ONLY) && file->f_op->fsync)
    blk_queue_flush(lo->lo_queue, REQ_FLUSH);
```

FAT supplies `.fsync`, but `fat_file_fsync()` only calls
`generic_file_fsync()` and `sync_mapping_buffers()`; it does **not** issue a
block-device cache flush. The FAT mount also lacks the `flush` option. Therefore:

- an ext3 `barrier=1` request may reach loop and call `vfs_fsync()` on the FAT
  file, but there is no demonstrated `FLUSH` to the eMMC cache;
- barrier success cannot be claimed for the real loop→FAT→eMMC path;
- `barrier=0` is the honest current state.

| Layer | Consistency mechanism | Power-loss limitation |
|---|---|---|
| ext3 internal | journal, writeback ordering | metadata may recover; file data can be stale/uninitialised without ordered mode |
| FAT container | none (no journal) | directory/FAT/image file can be damaged independently |
| Application data | some atomic temp+fsync+rename paths | not every writer (for example reader sidecars) is proven atomic |
| Physical flash | eMMC cache | no end-to-end flush proven; ordering may be lost |

### 1.3 Safer configuration evaluated

A copy-only hardening tool was added:

```sh
tools/maintenance/pw3-ext3-harden-copy.sh SOURCE.ext3 OUTPUT.ext3
```

It refuses device nodes, mounted sources and in-place output. It then runs:

```sh
cp --reflink=auto SOURCE OUTPUT
tune2fs -o journal_data_ordered -e remount-ro OUTPUT
e2fsck -fn OUTPUT
```

Offline result on `alpine-reader-v4.ext3`:

| Field | Original | Hardened copy |
|---|---|---|
| SHA-256 | `927e9ab6…d3eb` | `357e1d18…bfd8e` |
| Default mount options | `user_xattr acl` | `journal_data_ordered user_xattr acl` |
| Errors behavior | Continue | **Remount read-only** |
| e2fsck | clean | **clean** |
| Journal | 4 MiB internal | 4 MiB internal |

`journal_data_ordered` is supported by this ext3 kernel; it does not require
ext4. `errors=remount-ro` is a supported ext3 error mode. Both changes are
superblock defaults and can be built into future images without remounting the
running root.

`barrier=1` is **not** included in the validated proposal because the backing
stack cannot prove a hardware flush. Full `data=journal` was rejected as a
default: it would increase write amplification on a nearly full, slow eMMC and
still does not protect the FAT container.

### 1.4 Production durability architecture

The correct split is:

1. **Replaceable OS** — `system.ext3`, `/usr`, `/opt`, kernel modules, caches.
   Rebuildable from source/manifest; never the only copy of user state.
2. **Reading data** — books, sidecars, reading positions, annotations under
   `/mnt/us/documents` (FAT). Back up independently of the OS image.
3. **Critical configuration** — `/var/lib/gleworks/light.conf`, `time.conf`,
   `wifi.conf`, `wifi-enabled.conf`. These are tiny but currently only on the
   root image. They should have an application-level copy outside the root
   image and a host backup.
4. **Recovery assets** — Guardian, USB rescue, `system.ext3` backups, boot
   markers. Never store these only in the replaceable root.
5. **Caches** — CRE cache, Python bytecode, APK cache. Never back these up;
   deleting them is safe.

Recommended non-partition design:

- Keep the current FAT control partition; do not repartition.
- Write authoritative config atomically (temp file → `fsync` → `rename` →
  directory `fsync`). `gw_config_save_int/string` already do this.
- Mirror the four `/var/lib/gleworks` files to a small versioned directory on
  FAT after changes, with a SHA-256 manifest; restore on boot if root is
  replaced. This is a proposal, not deployed.
- Back up `documents` sidecars before OS replacement; KOReader metadata is
  application-level data, not OS state.
- Keep at least two named `system.ext3` backups on FAT and one on the host.
- Treat FAT corruption as a real failure mode: recovery must be able to use the
  host backup, not only the on-device copy.

### 1.5 Migration and rollback

Do not deploy until the whole recovery path is exercised:

1. Boot rescue, verify USB identity and nonce, copy current `system.ext3` to a
   new named FAT backup; copy that backup to the host and record SHA-256.
2. Build a new image offline with ordered data + remount-ro; run
   `e2fsck -fn`; boot it read-only first.
3. Deploy through `kindle-linux.py deploy-root` with a new backup name and
   evidence file.
4. Acceptance: clean boot, Guardian cancel, auto-start, Home, Wi-Fi, Tailscale,
   KOReader cold/warm open, clean shutdown, offline `e2fsck` of a copy.
5. Rollback: rescue → replace `system.ext3` with the recorded backup → verify
   hash and `e2fsck` → clear pending only after the replacement is proven clean
   → reboot.
6. Never test power loss on the production PW3. Use disposable images only.

**P2 status: PARTIAL / NOT DEPLOYED.** The safer superblock configuration is
validated offline; the recovery path was not re-exercised end-to-end in this
phase.

## 2. P3 — suspend readiness

### 2.1 Exact interfaces

| Item | Value |
|---|---|
| `/sys/power/state` | `standby mem` |
| `/sys/power/autosleep` | absent |
| `/sys/power/wakeup_count` | `0` |
| `/sys/power/pm_test` | `[none] core processors platform devices freezer` |
| RTC | `max77696-rtc.0`; `/sys/class/rtc/rtc0` |
| `snvs_rtc.0/power/wakeup` | **enabled** |
| `max77696-rtc.0/power/wakeup` | **enabled** |
| PMIC parent `0-003c/power/wakeup` | **enabled** |
| `wario_hall/power/wakeup` | disabled |
| `fsl-usb2-otg/power/wakeup` | disabled |
| `fsl-usb2-udc/power/wakeup` | disabled |
| `max77696-onkey.0/power/wakeup` | **attribute absent** |
| Charger wakeup | disabled |
| Power button IRQs | `max77696-onkey_press/release`, currently 0 |
| Touch IRQ | `main_ttsp_core` GPIO |
| USB console | CDC-ACM gadget; suspend will remove host access |

**Wake conclusion:** the RTC is the only wake source with an explicit enabled
sysfs wakeup. The PMIC parent is enabled, so the power button may be able to
wake through the PMIC, but the child onkey device exposes no `power/wakeup`
attribute and no power-button wake has been demonstrated. A power-button wake is
therefore **NOT VERIFIED**. USB and hall wake are disabled.

This is enough to forbid an actual `mem` trial. A deep suspend with only the RTC
would be recoverable only if the RTC alarm succeeds; if it does not, the USB
console is gone.

### 2.2 Test matrix (not executed)

Preconditions: clean `e2fsck`, two off-device backups, battery ≥80 %, Guardian
`state=00000000`, `sync` before each stage, no document open, RTC alarm proven
without suspend, USB recovery path documented.

| Stage | Test | Pass condition |
|---|---|---|
| 0 | `pm_test=freezer`; `mem` | immediate resume; console alive |
| 1 | `pm_test=devices`; RTC +60 s | resumes at alarm; no device loss |
| 2 | `pm_test=platform`; RTC +60 s | resumes; panel/frontlight intact |
| 3 | Shallow `standby`; RTC +60 s | resumes at alarm; USB console returns |
| 4 | `mem`; RTC +90 s, power button ready | resumes; console returns; Wi-Fi/Tailscale recover |
| 5 | `mem` 5×, then 10 min | no leak, no lost wake source, no offline repair |

Every stage records boot ID, uptime, RTC alarm, wake IRQ, panel image, frontlight,
Wi-Fi state, Tailscale state, USB enumeration and a copied-image `e2fsck` after
clean shutdown.

**P3 status: NOT TESTED.** No suspend transition was executed.

## 3. P4 — battery measurement infrastructure

### 3.1 Logger

`tools/maintenance/pw3-battery-logger.sh` writes a TSV to
`/tmp/pw3-battery-log.tsv` only. It never writes flash during the measurement.

Default: 60 s interval, 24 h maximum, 1,440 rows, single-instance PID lock.

Columns: epoch, uptime, battery status/capacity/voltage/current/charge, CPU
jiffies, context switches, interrupts, load, Wi-Fi/proc PIDs, frontlight,
governor, KOReader RSS/HWM.

Two-sample test on USB power:

```
status=Full capacity=99 voltage_now=4201 current_now=0 charge_now=1463
wlan=up wpa=791 udhcpc=972 tailscale=715
frontlight=573 governor=performance
```

`udhcpc` is identified through `/run/gw-udhcpc.pid` because BusyBox exposes its
process name as `busybox`, not `udhcpc`.

### 3.2 Limitations

- `current_now` is **0** while USB is attached and the battery is full. It is
  not evidence of zero consumption.
- `charge_now` has 1 mAh resolution; a 10-minute interval is required to see a
  useful delta.
- No calibrated power meter, no fuel-gauge energy counter, and no reliable
  `power_now` are available.
- Therefore no mW or battery-life claim is made.

### 3.3 Controlled battery-only procedure

1. Start the logger over USB with `interval=60`, `duration=3600` (or longer).
2. Record frontlight level, governor, Wi-Fi/Tailscale state, book state.
3. Unplug USB; do not touch the device during the interval.
4. After the interval, reconnect USB and copy `/tmp/pw3-battery-log.tsv` to the
   host before any reboot.
5. Repeat one variable at a time: Home light 14, Home light 0, KOReader idle,
   page turns, Wi-Fi off/on, Tailscale off/on.
6. Reject samples where charger/USB state changed; compute mAh/min from
   `charge_now` and voltage, not CPU frequency alone.
7. Do not include suspend until P3 passes.

**P4 status: PARTIAL.** Infrastructure exists and was tested on USB power; no
battery-only run was performed.

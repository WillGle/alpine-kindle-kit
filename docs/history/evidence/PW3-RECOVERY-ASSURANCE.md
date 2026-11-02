---
status: historical
date: 2026-11-02
superseded_by: ../../status/CURRENT.md
---

# PW3 Recovery Assurance — Phase 4A

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
OS: Alpine 3.24.3, musl, kernel `3.0.35-lab126`  
Final boot ID: `d5af787a-e8fd-4f81-bc9a-87f386818c28`  
Access: physical USB CDC-ACM console, then independent USB mass-storage rescue

**Decision: NO-GO for deploying the ordered/remount-ro filesystem image.**
**Phase 4B supersedes the deployment decision after a physical restoration and
rollback drill; see `PW3-PHASE4B-RECOVERY.md`.**

The independent rescue path was exercised twice and the current p1, root, config,
reading data and waveform assets are backed up and verified. However, automatic
recovery from a failed Alpine boot was not physically tested, the current p1
bootstrap has no automatic stock fallback, and a full production restore/boot
drill was not performed. Those are hard gates for filesystem deployment.

## 1. Boot chain

### 1.1 Exact chain

```text
stock bootloader
  -> stock 3.0.35-lab126 kernel + initramfs (unchanged)
  -> kernel mounts /dev/mmcblk0p1 readonly as /
  -> p1 /sbin/init
       current: static ARM bootstrap, 98,092 bytes,
       SHA-256 9db871b9f8cdfcfe4d632766bb85eee8e44c00d61ca873165a335f3b6f6ca555
  -> bootstrap mounts p4 raw at +8192 as loop0 vfat /control
  -> reads boot.once / boot.enabled / boot.pending / boot.repairing
  -> validates /control/gleworks-alpine/system.ext3
       size, regular file, ext3 magic, revision, no needs-recovery bit
  -> if needed: e2fsck -p (battery >= 50%) and boot.repairing marker
  -> loads /modules/gleworks_pid1_reset.ko and arms a 90 s deadline
  -> writes boot.pending
  -> maps system.ext3 as loop1, mounts ext3 readonly
  -> moves /control into the new root, pivot_root, detaches p1
  -> exec Alpine /sbin/init = BusyBox
  -> /etc/gleworks-early.sh: proc/sys/dev/run/tmp, USB serial, Guardian cancel,
     remount root rw, retain boot.pending, start Tailscale
  -> /etc/gleworks-openrc.sh: Wi-Fi, OpenRC sysinit/boot/default
  -> gw-system -> gw-shell -> Home
```

Normal shutdown runs `/gleworks/shutdown` as PID1 from RAM: sync, unmount the
Alpine ext3, journal the clean unmount, clear `boot.pending`, unmount FAT, reboot.

### 1.2 Failure path before Alpine

```text
missing / invalid / dirty image, bad token, missing boot.enabled,
boot.repairing present, or mount/pivot failure
  -> panel status "Alpine USB rescue - connect USB to recover"
  -> close root/loop/FAT
  -> load USB controller + g_file_storage on /dev/mmcblk0p4
  -> enumerate as 0525:a4a5 bcd0200 (independent USB mass storage)
  -> host repair/replace system.ext3, sync, unmount, SCSI eject
  -> bootstrap reboots; boot.enabled selects Alpine
```

The rescue path exports **p4 only**, not p1. There is no serial console in rescue;
the host sees a USB disk.

### 1.3 Components that run before `system.ext3`

| Component | Location | Needed for |
|---|---|---|
| Bootloader | protected boot area | kernel load |
| Kernel + initramfs | protected partitions | mount p1, drivers, loop, USB gadget |
| p1 `/sbin/init` | p1 | bootstrap and rescue |
| `/modules/*.ko` | p1 | USB controller, storage gadget, Guardian |
| `/sbin/init.gleworks-stock` | p1 | manual stock entrypoint only; not auto-selected |
| p4 FAT | `mmcblk0p4` | boot tokens, root image, rescue export |
| `boot.enabled` | p4 | persistent Alpine selection |
| `boot.once` | p4 | one-shot trial selection (absent now) |
| `boot.pending` | p4 | unclean-startup marker |
| `boot.repairing` | p4 | interrupted offline repair marker |
| `system.ext3` | p4 | Alpine root |

## 2. Recovery independence

| Scenario | Independent of gw-system/gw-shell/KOReader? | Independent of current `system.ext3`? |
|---|---|---|
| Guardian deadline | **Yes** — kernel module in p1 | **Yes** |
| Bootstrap image validation/e2fsck | **Yes** — p1 static binary | **Yes** (reads image, does not execute it) |
| Rescue USB export | **Yes** — p1 modules + p4 raw | **Yes** |
| Stock fallback | File exists, but **not automatic** | Yes |
| Normal USB serial console | No — created by Alpine early script | No |

The normal USB console (`0525:a4a7`) **cannot be assumed** when Alpine fails.
Recovery access is the independent mass-storage device `0525:a4a5` bcd0200.

### 2.1 Timeout and retry behavior

- Guardian deadline: **90 seconds**, armed immediately before Alpine exec.
- Guardian state is RAM-only; reset clears it.
- `boot.pending` survives resets and is removed only after clean unmount.
- If a boot fails before Alpine early setup cancels Guardian, the next bootstrap
  sees `boot.pending`, creates `boot.repairing`, retries once.
- If `boot.repairing` is present at the next boot, bootstrap goes to rescue.
- So there is **one retry**, then rescue. There is no numeric retry counter.
- `boot.pending` alone does not force rescue if the image is clean; it marks the
  boot as needing supervision. A second failure or dirty image forces rescue.

### 2.2 Single points of failure

1. **p1 `/sbin/init`** — if corrupt or unbootable, no recovery code runs. The
   preserved `/sbin/init.gleworks-stock` is not selected automatically.
2. **p1 / kernel / bootloader** — p4 rescue cannot repair these. This remains
   outside the demonstrated USB recovery path.
3. **Guardian module failure** — `arm_trial_guard()` halts/reboots. Because p1
   `/sbin/init` is the bootstrap itself, a persistently unloadable module can
   cause a boot loop rather than stock fallback.
4. **p4 FAT corruption** — tokens/image cannot be read; rescue may not enumerate.
5. **USB gadget/module failure** — no host access.
6. **Host rescue tooling** — wrong `sdX`, missing `udisksctl`/`gdbus`, or host
   filesystem errors can prevent safe recovery.
7. **No verified automatic p1 fallback** — current p1 has no dispatcher that
   falls back to stock after bootstrap failure.
8. **No physical test of corrupt-image/reset fallback** — only token-selected
   rescue was exercised on hardware.
9. **Historical image `system-before-rca`** — `e2fsck -fn` reports free-block and
   free-inode count errors; it is not a trusted recovery image.

## 3. Backup verification

### 3.1 Physical rescue export

`kindle-linux.py rescue` was used twice:

1. Remove `boot.enabled`, orderly shutdown, reboot.
2. p4 enumerated as independent USB `0525:a4a5` bcd0200 and mounted at
   `/run/media/will/Kindle`.
3. Images and metadata were copied to the host.
4. `resume` recreated `boot.enabled`, verified `system.ext3` clean, SCSI-ejected,
   and Alpine booted with a new boot ID.

| Cycle | Alpine boot before | Alpine boot after |
|---|---|---|
| 1 | `a3028309-3468-4340-b81f-6b749caae64f` | `da99cc2e-523d-4b05-8244-e35c985cdc63` |
| 2 | `da99cc2e-523d-4b05-8244-e35c985cdc63` | `d5af787a-e8fd-4f81-bc9a-87f386818c28` |

No root image was replaced. No bootloader/kernel/partition was changed.

### 3.2 Host copies

Stored under (archived 2026-11-01 as
`work/private/evidence/archive-20261101/phase4a-rescue-20261031.tar.zst`):

```text
work/private/evidence/phase4a-rescue-20261031/
  images/
  documents/
  config/gw-config.tar
  meta/
```

| Artifact | Bytes | SHA-256 | Integrity |
|---|---:|---|---|
| `p1-current-20261031.ext3` | 471,859,200 | `e50079c101692bcb1f48b441d8c87f56350edf2a5cabe37f4e5d54defbab0bd9` | `e2fsck -fn` clean |
| `system-current-after-resume.ext3` | 268,435,456 | `1a780d423206d03cbfe6c612be7f681ffaf61bfce241e6b6093c1f4414b9049c` | `e2fsck -fn` clean |
| `system-before-os-trial-20261030.ext3` | 268,435,456 | `0678b99279922a548cc19d09baa9f080dc9c5e077a7caa23d82b20c4ba030a9e` | clean |
| `system-after-hotplug-20261030.ext3` | 268,435,456 | `ad86fca45f9fff56e338d41eaf1da7d5a0e55e112f7f36ac56027a64c6e663ae` | clean |
| `system-before-candidate-…ext3` | 268,435,456 | `f66914c948f1d13f626f7513883288184b96971528a437c223c8df65767f3f0e` | clean |
| `system-before-rca-…ext3` | 268,435,456 | `fa11773957ce483b9d7bf70c91cfe34bfd42c040504cdd055b5e7c9189c728db` | **free counts wrong** |
| `gw-config.tar` | 5,632 | `65e5ae9314494ab97d36f2734adf6c0bfe5fe6444a2a4cc1037c1157bfc7af76` | readable tar |
| `documents/` (book + sidecar) | 20K | — | copied |

`p1-current-20261031.ext3` was also written to p4 for an on-device copy. It is
the raw p1 image; p1 was not mounted and was not modified.

### 3.3 p1 component hashes

| Path in current p1 | SHA-256 |
|---|---|
| `/sbin/init` | `9db871b9f8cdfcfe4d632766bb85eee8e44c00d61ca873165a335f3b6f6ca555` |
| `/sbin/init.gleworks-stock` | `ad624cbad3090bf15e65c8420a4e451ee10fd067a40b1bfe662965e82d932d0a` |
| `/sbin/init.gleworks-dispatch` | `11750399e01fcc499a74bd6357651ec5916e431358ea134f06df292fb1d7cee5` |
| `/gleworks-native-trial/bootstrap` | `b4aa162ffc2972a59a3c649d15b600c90e495e5025f6c29bada7798acb0b7175` |
| `/modules/fsl_otg_arc.ko` | `c7ee9e42f8c61249bce13591c910ed05c53eca27fec4a655851d3bbd4e8da788` |
| `/modules/arcotg_udc.ko` | `63a5333d9717a37f58b153382c5c7bbfb7eaf78d8ce415a9b8c81dee0ceb4edc` |
| `/modules/g_file_storage.ko` | `95c68e28659a0c43f0c62273cca5a9b817f9570f4a74dac5a0effbfdc95d23e4` |
| `/modules/gleworks_pid1_reset.ko` | `1d26699185511e5cf9d523d147ab2105fab4f14b7e3816ddf6818262cba7d03d` |

The pre-trial host p1 backup (`c33cdeb6…`) contains original stock init
`ad624cba…`; it **does not** contain the current bootstrap/modules. It is useful
for provenance, not as a current p1 restore source.

### 3.4 Waveform and device data

Host waveform capture:
`work/private/evidence/captures/gleworks-waveform-capture-20261029-190452-16018/`

| Asset | Bytes | SHA-256 |
|---|---:|---|
| `active-waveform.bin` | 6,613,472 | `c6ab1a5fdd32f9f465fb8be03efea2382f50e08d0db896d629ac44fa6f8019b7` |
| `panel-waveform.bin` | 456,570 | `f937329cbcc271d0e6b48c015377a0744680a949b8c3f117e861fcd91dbd9c33` |
| `SHA256SUMS` | 1.5K | `e95f206088a99bcbf520721fe513df9788fbb8b8602d65faef72717b0d7bf4a9` |

Both the original and second capture copies match. This is a host-side capture
and structural review, not a panel-flash restore mechanism.

## 4. Offline fault simulation

Tool: `tools/maintenance/pw3-recovery-sim.py`  
Source: the verified current root snapshot.

| Case | Bootstrap decision | Result |
|---|---|---|
| Clean image | **boot Alpine** | `e2fsck -fn` clean, rc 0 |
| INCOMPAT_RECOVER set | **rescue** | `needs-journal-recovery` |
| Bad magic/revision | **rescue** | `bad-magic-or-revision`; e2fsck errors |
| Truncated update (16 MiB) | **rescue** | `bad-size` |
| Missing image | **rescue** | `missing-or-not-regular` |
| Restore copy | **boot Alpine** | hash identical, `e2fsck` clean |
| Read-only root | code inspection | early startup failure can leave `boot.repairing` and reach rescue; a later `errors=remount-ro` event does **not** reboot or force rescue |

The read-only-root case was not run on hardware. The bootstrap source shows that
the early script's EXIT trap sends `kill -QUIT 1` on failure; if the failure is
before early startup completes, the next boot can see `boot.repairing` and reach
rescue. Phase 4B corrected the stronger claim: after early startup cancels the
Guardian and removes `boot.repairing`, an `errors=remount-ro` event only marks the
mounted filesystem read-only. It does not reboot the device or automatically
select rescue.

Supporting emulator-only evidence exists for Guardian deadline firing,
corrupt/missing image fallback and stock return, but it is not treated as physical
proof for the current binaries.

## 5. Recovery gates

### G1 — Backup integrity: **PASS**

- Current p1: raw image, `e2fsck` clean, components hashed, host+p4 copies.
- Current root: clean snapshot, `e2fsck` clean, key files present.
- Config/credentials: tar copied; not printed.
- Reading data/sidecar: copied.
- Waveform/calibration capture: hashes match.
- Caveat: historical `system-before-rca` is not clean and must not be used as a
  recovery image.
- Caveat: the FAT container itself was mounted/read/written successfully during
  both rescue cycles, but a separate FAT `fsck` was not run (it was mounted).
  FAT structural integrity is therefore **not independently audited**.

### G2 — Independent boot fallback: **PARTIAL**

- **Physical:** rescue selection by removing `boot.enabled` worked; p4 exported,
  host mounted it, Alpine resumed twice.
- **Not physical:** automatic fallback after a genuinely failed Alpine boot,
  corrupt image, dirty journal or Guardian timeout.
- **Not available:** automatic fallback when p1 `/sbin/init`/kernel/bootloader is
  broken. Current p1 bootstrap has no stock fallback path.
- The Guardian/pending/repairing retry logic is code-verified and emulator-tested,
  not hardware-fault-tested.

### G3 — Recovery access without a working Alpine root: **PASS**

- Verified physically: rescue enumerated independently while Alpine was not
  running, p4 was mounted and read by the host, and the host safely ejected.
- The normal serial console was not used for rescue; recovery is USB mass storage
  `0525:a4a5` bcd0200.

### G4 — Verified restoration and rollback procedure: **PARTIAL / BLOCKED**

- **Offline PASS:** a backup image was copied to a disposable file, hash matched,
  `e2fsck -fn` was clean, and key files were readable with `debugfs`.
- **Not performed:** replacing production `system.ext3` with a backup and booting
  it. This is intentionally not done without explicit approval and a second
  recovery mechanism.
- **Not performed:** restoring p1. The current p1 image exists, but no p1 write
  path has been validated on this device.

## 6. Filesystem deployment decision

### **NO-GO**

The ordered/remount-ro image is kernel-compatible and passes offline `e2fsck`,
but the complete recovery gates are not met:

| Gate | Status |
|---|---|
| G1 backup integrity | PASS |
| G2 independent fallback | PARTIAL |
| G3 rescue access | PASS |
| G4 restore/rollback | PARTIAL/BLOCKED |

Additional deployment blockers:

1. Automatic fallback from a failed Alpine boot was not physically tested.
2. Current p1 bootstrap has no automatic stock fallback; p1 failure is a single
   point of failure.
3. Production restore of a backup was not boot-tested.
4. `barrier=1` remains unproven through loop→FAT→eMMC; only `data=ordered` +
   `errors=remount-ro` are in the offline proposal.
5. Existing historical image `system-before-rca` is not a clean fallback.

### GO conditions

Before deploying the hardened image:

1. Verify the automatic failure path on hardware using a disposable/sacrificial
   root image, with the independent rescue path already proven.
2. Add or validate a p1-level stock fallback so a bootstrap/Guardian failure
   cannot boot-loop.
3. Perform a full backup→production restore→boot→rollback drill, including
   p1 recovery, with host backups retained.
4. Re-run `e2fsck -fn` and hash verification on the exact candidate image.
5. Keep the current clean root snapshot and current p1 image off-device.

## 7. Tested rollback procedure (partial)

What is tested:

1. Remove `boot.enabled`; orderly reboot.
2. Confirm `0525:a4a5` bcd0200, mount p4, copy images.
3. Run `e2fsck -fn` on copies.
4. Recreate `boot.enabled`, verify current image clean, safe-eject, boot.
5. Verify new boot ID, Guardian state 0, Home, Wi-Fi, Tailscale and logs.

What is **not** tested:

1. Writing a backup over production `system.ext3` and booting it.
2. Restoring p1 or recovering from a corrupted p1.
3. Recovering after a physical power loss during image replacement.

The intended production restore path is the existing `deploy-root` flow:
new backup name, `.partial` file, fsync, hash readback, rename, `boot.enabled`,
safe eject. It must not be used for rollback acceptance until the untested steps
above are exercised.

## 8. Final device state

| Check | Result |
|---|---|
| Boot ID | `d5af787a-e8fd-4f81-bc9a-87f386818c28` |
| PID1 | `/bin/busybox` |
| `boot.enabled` | `gleworks-alpine-v1` |
| `boot.pending` | present (normal runtime marker) |
| `boot.once` / `boot.repairing` | absent |
| Guardian | `state=00000000` |
| Root | `/dev/loop1` ext3, 179,049 KiB / 71 % |
| FAT control | 1,772,032 KiB / 56 %, 1,416,608 KiB free |
| `p1-current` on p4 | 471,859,200 B, hash `e50079c1…` |
| `gw-system` / `gw-shell` | running |
| Wi-Fi / Tailscale | connected / connected |
| Console | `wifi-state=CONNECTED` |
| USB | host-class |
| Log | 655 B |

No root filesystem or protected partition was modified during rescue testing.

## 9. Deliverables

- This report: `docs/evidence/PW3-RECOVERY-ASSURANCE.md`
- Machine-readable: `docs/evidence/PW3-RECOVERY-ASSURANCE.json`
- Boot-chain/dependency map: section 1–2
- Offline fault results: `docs/evidence/raw/PW3-PHASE4A-offline-faults.{txt,json}`
- Backup inventory: `docs/evidence/raw/PW3-PHASE4A-p1-inventory.txt`
- Physical rescue/resume evidence: `docs/evidence/raw/PW3-PHASE4A-final-state.txt`
- Reproducible tools:
  - `tools/maintenance/pw3-recovery-sim.py`
  - `tools/maintenance/pw3-ext3-harden-copy.sh`
  - existing `tools/maintenance/kindle-linux.py rescue|resume`

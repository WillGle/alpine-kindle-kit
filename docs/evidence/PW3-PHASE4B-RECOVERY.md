# PW3 Phase 4B — Recovery Deployment Readiness

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
OS: Alpine 3.24.3, musl, kernel `3.0.35-lab126`  
Final boot ID: `820f14c4-01df-422d-a3d9-2918065288c2`  
Access: physical USB CDC-ACM console, then independent USB mass-storage rescue

**Decision: GO — conditional — for the next authorized deployment of the
copy-only hardened candidate.**  
**Phase 4C deployed this candidate and passed physical acceptance; see
`PW3-PHASE4C-HARDENING.md`.**

This is **not** a claim of full recovery assurance, and the hardened image was
**not** deployed in this task. The GO applies only to the incremental,
superblock-only change: `journal_data_ordered` plus `errors=remount-ro`, built
as a byte-for-byte copy of the current clean root. It does not authorize a
rebuilt image, `barrier=1`, partition changes, or any p1 change.

The basis is that the candidate only reduces existing writeback/continue
exposure, the USB rescue path is physically proven, and a production
replacement plus rollback has now been boot-tested on hardware. The remaining
automatic-fallback gap (G2) is pre-existing, is not made worse by this
candidate, and is accepted explicitly in the residual-risk register. Deployment
must stop and roll back if any acceptance check fails.

## 1. What changed in Phase 4B

| Item | Result |
|---|---|
| Fresh clean production snapshot | `fa59de77…`, `e2fsck` clean |
| Copy-only hardened candidate | `673fa31f…`, `e2fsck` clean, undeployed |
| Physical replacement drill | PASS — restored copy booted |
| Physical rollback drill | PASS — pre-drill image restored and booted |
| Independent USB rescue | PASS — four rescue exports used |
| Config/document preservation | PASS — hashes matched after both boots |
| FAT integrity | UDisks2 found an inconsistency after the USB reset; repair succeeded, follow-up check is `(true,)`, and all file hashes match |
| p1/bootloader/kernel/partition changes | None |
| Hardened image deployment | **Not performed** |

## 2. Failure-domain matrix

`USB-only recovery` means recovery from the host using the independent
`0525:a4a5` bcd0200 mass-storage export of p4, without Alpine userspace.

| # | Failure domain | Failure examples | Detection / behavior | USB-only recovery | Introduced or worsened by candidate? | Residual risk |
|---|---|---|---|---|---|---|
| 1 | p1 / bootstrap | p1 corruption, missing `/sbin/init`, unpatchable module, Guardian module load failure | Kernel mounts p1; if bootstrap cannot run, no p4 rescue code executes | **No demonstrated path.** p4 is not exported when p1 init is dead. Current p1 has no automatic stock fallback | **No.** Candidate does not touch p1 | **High, pre-existing.** A p1-level failure can require host-side p1 repair or bootloader access not proven here |
| 2 | Alpine root image | Missing, truncated, bad magic, dirty journal, mount failure, low battery during repair | Bootstrap validates size/type/magic/revision/needs-recovery; rescue on failure. Dirty image: `e2fsck -p` if battery ≥50 %, else rescue | **Yes.** p4 exported with tokens/image; host can replace/repair | **No for pre-mount failures.** `errors=remount-ro` changes post-mount error behavior from continue to read-only | Automatic fallback after `boot.repairing` is code/offline-tested, not physically fault-injected |
| 3 | p4 / FAT container | FAT corruption, token missing/invalid, image directory damage, USB gadget failure | Missing/invalid token → rescue. Bootstrap maps p4 raw; rescue exports raw p4, so a FAT-level repair is possible if the block device is healthy | **Yes, if p4 block device and USB gadget still work.** A physically failed p4 is not recoverable this way | **No.** Candidate is one more file in the same FAT | **Medium.** FAT is unjournaled; host-side USB reset was observed and recovered, but a bad p4 is a hard limit |
| 4 | Application / service | `gw-system`, `gw-shell`, KOReader, Wi-Fi/Tailscale failure after early startup | Guardian is already cancelled; device may stay up degraded. No automatic reboot. Normal USB console may remain usable | **Yes, if console or reboot is available.** Manual `RESCUE` removes `boot.enabled`; otherwise power-cycle then rescue | **No.** Candidate does not change service supervision | **Medium.** No post-early watchdog. Repeated runtime failure does not necessarily escalate to rescue |
| 5 | Interrupted replacement | Power loss or USB reset during copy/rename/token write | Deploy order leaves a preserved backup and removes `boot.enabled` until the candidate is fully installed. Interruption before the token → rescue | **Yes.** Backup and/or partial remain; token absent forces rescue | **No.** Same order used for the drill | **Low–medium.** USB reset during token write occurred; token readback and fail-closed behavior now required |

### 2.1 Pre-existing versus candidate-introduced risk

The candidate changes only ext3 superblock defaults. It does not alter:

- p1, bootloader, kernel, partition table, Guardian, or USB gadget code;
- the fixed `system.ext3` boot path;
- the FAT container or loop-device flush behavior;
- user documents on FAT.

It does change:

- file data ordering from writeback to ordered;
- detected-filesystem-error behavior from continue to remount read-only.

Both changes reduce corruption exposure. They do not create an A/B boot path and
do not make p1 recoverable.

## 3. Image selection design

### 3.1 Existing bootstrap is single-slot

The current p1 bootstrap defines:

```c
#define REQUEST   CONTROL "/gleworks-alpine/boot.once"
#define ENABLED   CONTROL "/gleworks-alpine/boot.enabled"
#define PENDING   CONTROL "/gleworks-alpine/boot.pending"
#define REPAIRING CONTROL "/gleworks-alpine/boot.repairing"
#define IMAGE     CONTROL "/gleworks-alpine/system.ext3"
```

Both `boot.once` and `boot.enabled` select the **same fixed** `system.ext3`.
`boot.once` is one-shot; `boot.enabled` is persistent. Neither names an
alternate image. The bootstrap opens the image with `O_NOFOLLOW`, so a symlink
cannot be used to stage an A/B target. There is no hash manifest or generation
counter in the bootstrap.

`image_clean()` only checks:

- regular file, 64 MiB–1 GiB;
- ext3 magic and revision;
- `INCOMPAT_RECOVER` clear.

It does not run `e2fsck` on every clean-looking image and does not verify a
SHA-256 manifest.

**Conclusion:** true A/B selection is not available without a p1 change. Do not
implement A/B booting on this bootstrap.

### 3.2 Safe staging without p1 changes

Use one active slot plus named, never-overwritten backups:

```text
p4:/gleworks-alpine/
  system.ext3                                  active single slot
  system-pre-hardening-YYYYMMDD.ext3           preserved production image
  system-restoration-candidate-20261031.ext3   post-restore snapshot
  ...other named historical images...
```

A candidate may be copied to p4 under an alternate name, but it is **inert**
until renamed to `system.ext3`. The next authorized deployment must use the
existing `deploy-root` order:

1. enter rescue and remove `boot.enabled`;
2. copy candidate to `system.native-kit.partial`, fsync, read back and hash;
3. rename current `system.ext3` to a new `system-*.ext3` backup;
4. rename verified partial to `system.ext3`;
5. clear stale pending only through the tool’s explicit path;
6. write `boot.enabled` last, fsync, and read it back;
7. safe-eject.

Any interruption before the final token leaves no normal boot selection, so the
next boot goes to USB rescue with at least one complete image preserved. The
replacement order was modeled at every crash point:

- `docs/evidence/raw/PW3-PHASE4B-restore-order.json`
- `docs/evidence/raw/PW3-PHASE4B-restore-order.txt`

| Crash point | Active slot | Preserved image | Next boot |
|---|---|---|---|
| During candidate partial | old root | old root still active | rescue (token removed) |
| After old renamed to backup | empty | old backup + partial | rescue |
| After candidate installed, before token | candidate | old backup | rescue |
| After token written | candidate | old backup | candidate |
| Rollback image restored, before token | old root | candidate | rescue |
| After rollback token | old root | candidate | old root |

The model asserts that every state retains a complete image and that states
without a valid token require rescue.

## 4. Physical restoration and rollback

### 4.1 Images and hashes

| Artifact | Bytes | SHA-256 | State |
|---|---:|---|---|
| Clean production snapshot, pre-drill | 268,435,456 | `fa59de77a1e3a2afefc48ede979783a861ce72d036aa31fa0204f280257fb520` | `e2fsck` clean |
| Copy-only hardened candidate | 268,435,456 | `673fa31ffaba546be2a04f8adba1885f71ea5ad57e6cb6986ebdc4c3d2277e5b` | `e2fsck` clean, **not deployed** |
| Post-restore snapshot | 268,435,456 | `40ff83f3e9b46a7755d34d53d2dbea1c21c337bd59958bf58fdbb6366aba03ef` | clean |
| Current p1 image | 471,859,200 | `e50079c101692bcb1f48b441d8c87f56350edf2a5cabe37f4e5d54defbab0bd9` | clean |
| Config tar | 5,632 | `65e5ae9314494ab97d36f2734adf6c0bfe5fe6444a2a4cc1037c1157bfc7af76` | readable |

The hardened candidate differs from the production snapshot in **108 bytes
across eight 1 KiB blocks**, all superblock/backup-superblock locations. File
count, free blocks, free inodes, journal inode, and inode count are unchanged.
Key file hashes before and after hardening match. This is the expected
copy-only change, not a rebuilt image.

### 4.2 Replacement drill

Procedure actually executed:

1. `kindle-linux.py rescue`; p4 exported as `0525:a4a5` bcd0200.
2. Copied clean `system.ext3` to the host and verified `e2fsck`.
3. `deploy-root` copied that snapshot to a partial, verified its hash, renamed
   the running production image to
   `system-restoration-drill-20261031.ext3`, installed the verified copy as
   `system.ext3`, wrote the token, and safe-ejected.
4. Alpine booted with a new boot ID:
   `c43fce66-b0e8-4c55-9237-21aa4431e2ab`.
5. Verified PID1 `/bin/busybox`, Guardian `state=00000000`, Home, Wi-Fi,
   Tailscale, `system.ext3` clean, and all four config hashes unchanged.
6. Captured the Home framebuffer.

The restored copy used identical bytes to the production image, so this proves
the replacement/boot mechanism, not the hardened superblock flags. The hardened
candidate itself was intentionally not booted in this phase.

### 4.3 Rollback drill

Procedure actually executed:

1. Entered rescue again.
2. Preserved the post-restore active image as
   `system-restoration-candidate-20261031.ext3` (`40ff83f3…`).
3. Renamed the pre-drill backup back to `system.ext3`; verified its hash was
   exactly `fa59de77…` and `e2fsck -fn` was clean.
4. Removed stale pending/repairing/once markers.
5. Wrote `boot.enabled`, fsynced, read it back, and safe-ejected.
6. Alpine booted with a new boot ID:
   `b9562d8c-f895-415a-b727-18b735f6ee75`.
7. Verified Guardian `state=00000000`, Home, Wi-Fi/Tailscale, documents, and
   config hashes.

Rollback is therefore tested end to end at the image and boot-token level. It
does not depend on `gw-system` or `gw-shell`; it ran entirely from the host and
the p1 rescue path.

### 4.4 USB/FAT event during rollback

While writing `boot.enabled` during the first rollback attempt, the host hit:

```text
sd 0:0:0:0: [sda] Sense Key : Not Ready
Add. Sense: Medium not present
I/O error, dev sda, sector 4509212
...
usb 1-1: USB disconnect, device number 30
usb 1-1: new high-speed USB device number 31
```

The device did **not** boot Alpine. It re-enumerated as rescue `0525:a4a5`
bcd0200. Re-mounting p4 showed:

- all prior system images and the p1 image intact;
- `system.ext3` still exactly the expected rollback image
  `fa59de77…`;
- `system-restoration-candidate-20261031.ext3` exactly `40ff83f3…`;
- `boot.enabled` absent, so no unverified boot was possible.

The token was rewritten, fsynced, read back as exactly
`gleworks-alpine-v1\n`, and only then ejected.

The USB reset left the FAT container inconsistent. UDisks2
`Filesystem.Check` returns “whether the filesystem is undamaged”; it returned
`(false,)`, which is a **damage finding**, not a clean result. The host kernel
had also warned that the volume was not properly unmounted. The entire
`/documents` and `/gleworks-alpine` trees were copied to the host first.
UDisks2 `Filesystem.Repair` then returned `(true,)`, and a follow-up
`Filesystem.Check` returned `(true,)`.

Post-repair SHA-256 comparison against the pre-repair backup was **identical**
for every image and document, including:

- p1 image `e50079c1…`;
- active `system.ext3` `d7e8f097…`;
- post-restore candidate `40ff83f3…`;
- the older named system images;
- the EPUB and sidecar.

The FAT inconsistency is therefore repaired and the file data is preserved.
The FAT container is still unjournaled and this remains a real failure mode.

**Tool change:** `kindle-linux.py` now reads `boot.enabled` back after fsync in
both `deploy-root` and `resume` and refuses to eject on a mismatch.

### 4.5 Data preservation

After both the restored boot and the rollback boot:

| Config file | SHA-256 |
|---|---|
| `/var/lib/gleworks/wifi.conf` | `9ddab56b5e9756cd3ff45a9e3809755d70885e1e9ae31ebabdc9bb1627ce4a9f` |
| `/var/lib/gleworks/light.conf` | `d05cbee0f4902e39fcfb5454d31cad9487397b7e18e3d1c810780b06b349e948` |
| `/var/lib/gleworks/time.conf` | `074ec55872b7834d2880a08cff0097985e4641f48c9c1d2be376791b3de8c1e3` |
| `/var/lib/gleworks/wifi-enabled.conf` | `825f9535b105f0903b59617eff997f11f9b8682a0af039c0badc4b5685c55c5f` |

`/mnt/us/documents/PW3-Install-Check.epub` and its sidecar remained present
and unchanged. The config tar matches the live root configs.

## 5. Filesystem migration assessment

### 5.1 Candidate comparison

| Field | Current production | Hardened candidate |
|---|---|---|
| SHA-256 | `fa59de77…` | `673fa31f…` |
| Volume | `AlpinePW3` | `AlpinePW3` |
| Features | `has_journal filetype sparse_super large_file` | same |
| Block size | 1 KiB | 1 KiB |
| Inode count | 65,536 | 65,536 |
| Free blocks | 74,820 | 74,820 |
| Journal inode | 8 | 8 |
| Default mount options | `user_xattr acl` | `journal_data_ordered user_xattr acl` |
| Errors behavior | Continue | Remount read-only |
| `e2fsck -fn` | clean | clean |

Only superblock/backup-superblock bytes differ. No file content changed.

### 5.2 Effective mount behavior

The bootstrap mounts the root with:

```c
mount(root_device, ROOT, "ext3", MS_RDONLY|MS_NOATIME, NULL)
```

It does not pass `data=` or `barrier=1`. The early script later runs:

```sh
mount -o remount,rw /
```

On this kernel, ext3 reads `s_default_mount_opts` and `s_errors` during mount
(`fs/ext3/super.c`), sets `ORDERED_DATA` and `ERRORS_RO` from the candidate
superblock, and refuses to change data mode on remount. The remount without a
`data=` argument preserves the existing mode (`fs/ext3/super.c`, `Opt_data_*`).
Therefore the source-derived expectation for the candidate is:

```text
rw,noatime,errors=remount-ro,user_xattr,barrier=0,data=ordered
```

This was **not observed on hardware**, because the hardened candidate was not
deployed. It must be verified from `/proc/mounts` on the first hardened boot.

`acl` in the superblock default is inert because this kernel was built without
`CONFIG_EXT3_FS_POSIX_ACL`; the current mount already omits it.

`barrier=0` remains the honest state:

- the bootstrap does not request barriers;
- loop can forward `REQ_FLUSH` to `vfs_fsync()` of the FAT backing file;
- `fat_file_fsync()` writes data and mapping buffers but does not issue a
  block-device cache flush;
- the bootstrap FAT mount lacks the `flush` option;
- eMMC/loop queue sysfs exposes no usable `write_cache`/`fua` evidence.

Do not add `barrier=1` until a disposable-image power-cut matrix proves
end-to-end flush ordering. Even then, the FAT container remains unjournaled.

### 5.3 Journal replay and repair

- Candidate is clean and has no pending recovery.
- A candidate with `INCOMPAT_RECOVER` set is rejected by the bootstrap and sent
  to rescue.
- A persistent image that is dirty can trigger `e2fsck -p` when battery ≥50 %,
  with `boot.repairing` written before repair. A failed repair goes to rescue.
- `errors=remount-ro` does not change journal replay; it changes what happens
  after a detected runtime filesystem error.

### 5.4 Read-only fallback

`ext3_handle_error()` sets `MS_RDONLY` for `ERRORS_RO`, logs the error, and
commits the error state. It does **not** reboot, signal userspace, or invoke the
USB rescue path.

After early startup, the Guardian has been cancelled and `boot.repairing` has
been removed. A later `errors=remount-ro` event can therefore leave the device
running with a read-only root and no automatic escalation. This is a
pre-existing supervision gap; the candidate does not introduce it, but
`errors=remount-ro` makes it more likely that the kernel stops writes instead of
continuing. Manual recovery remains: use the USB console if alive, or reboot and
remove `boot.enabled` through the normal rescue command.

### 5.5 User-data persistence

- Books, sidecars, and reading positions live under `/mnt/us/documents`, which
  is the FAT control partition, not `system.ext3`. Replacing the root does not
  touch them.
- `/var/lib/gleworks` is inside the root. A copy-only hardening preserves it
  exactly. A rebuilt image requires explicit config migration; do not assume it
  is present.
- The current production snapshot `fa59de77…` contains config hashes matching
  the host tar. The hardened candidate was derived from it without file changes.
- Keep one host copy and at least one on-device named backup before any
  deployment.

### 5.6 Guardian behavior

- Guardian is loaded and armed before the root is mounted.
- It is cancelled by `gleworks-early.sh` only after USB serial setup and before
  the root is remounted read-write.
- If boot fails before early completion, the 90 s deadline or the early EXIT
  trap can reboot; pending/repairing can then lead to rescue.
- After cancellation, Guardian provides no protection for service or runtime
  filesystem failures.
- The candidate does not change Guardian behavior.

### 5.7 FAT and loop constraints

- `system.ext3` is a regular file inside unjournaled FAT on p4.
- Root I/O traverses ext3 → loop → FAT file → p4 → eMMC.
- ext3 `data=ordered` orders file data against metadata inside that stack; it
  does not add a journal to FAT or a verified cache flush to eMMC.
- A FAT/eMMC failure can damage the image independently of ext3.
- No discard is advertised by the loop queues; the eMMC reports large discard
  capability, but this design does not issue discard on the root image.
- There is no A/B copy and no checksum manifest in the bootstrap. A damaged
  active image is detected only by superblock checks, `e2fsck`, mount failure,
  or a failed boot.

## 6. Gates and decision

### 6.1 Gate status

| Gate | Status | Basis |
|---|---|---|
| G1 — Backup integrity | **PASS** | Clean production snapshot, hardened candidate, post-restore snapshot, p1 image, config tar, documents, waveform assets; hashes and `e2fsck` verified. FAT was found inconsistent after the USB reset, repaired, re-checked, and all file hashes matched |
| G2 — Independent boot fallback | **PARTIAL** | Manual USB rescue physically verified four times. Automatic fallback for a genuinely failed Alpine boot / Guardian timeout is code- and offline-verified, not physically fault-injected. Post-early runtime failure has no automatic escalation |
| G3 — Recovery access without Alpine root | **PASS** | p4 exported independently at `0525:a4a5` bcd0200; host mounted, read, wrote, checked, and safe-ejected; normal serial console not required |
| G4 — Verified restoration and rollback | **PASS for the mechanism** | Production image replaced, booted, rolled back, and booted again; configs and documents verified. Hardened candidate itself not booted by design |

### 6.2 Required evidence

| Required evidence | Present |
|---|---|
| Known-good backup | Yes: host `fa59de77…`, p4 `system.ext3`, plus older images |
| Independent USB rescue | Yes: four exports, including one after a host USB reset and one for FAT repair |
| Verified candidate image | Yes: `673fa31f…`, copy-only, `e2fsck` clean |
| Tested restoration mechanism | Yes: physical replacement and rollback boot |
| Clear rollback sequence | Yes: section 7 |
| Explicit residual-risk register | Yes: section 6.4 |

### 6.3 Decision

**GO — conditional — for the next authorized deployment.**

Why GO despite G2 PARTIAL:

1. The candidate is a copy-only superblock change, not a new boot chain. It
   lowers writeback/continue exposure and does not touch p1, kernel, bootloader,
   Guardian, partitions, or documents.
2. G1, G3, and G4 now have physical evidence. G4 was the main Phase 4A blocker.
3. G2's remaining gap is pre-existing and unchanged by this candidate. The
   candidate does not make p1 recoverable, and it does not remove the
   post-early supervision gap; it also does not add either failure mode.
4. The task explicitly excludes requiring an impossible p1 recovery mechanism
   before approving lower-risk work. The p1 residual is therefore documented,
   not used as a blanket blocker.

This GO is **not** a statement that automatic recovery is complete. It is a
statement that the incremental filesystem change is acceptable with the
controls below.

### 6.4 Residual-risk register

| Risk | Severity | Pre-existing? | Mitigation / acceptance |
|---|---|---|---|
| p1 `/sbin/init` or module failure has no p4 rescue | High | Yes | Do not modify p1; retain p1 raw image and host copy; accept that p1 failure is outside the demonstrated USB path |
| No automatic escalation after post-early runtime/service failure | Medium | Yes | USB console if alive; power-cycle then rescue; future watchdog/persistent failure counter would close this |
| Automatic Guardian-timeout fallback not physically fault-injected | Medium | Yes | Code/offline verification only; manual rescue proven; accept until a safe disposable-image test is designed |
| FAT container is unjournaled; one inconsistency was observed and repaired | Medium | Yes | Keep host image copies; safe-eject; avoid writing while rescue is active except controlled image/token operations; re-run UDisks2 Check after any USB reset |
| Host USB reset during rescue token write | Medium | Yes (observed) | Token readback after fsync; fail closed; remount and retry; never eject on unverified token |
| `barrier=1` not proven through loop→FAT→eMMC | Medium | Yes | Keep `barrier=0`; do not add it without power-cut evidence |
| `data=ordered` does not protect FAT/eMMC | Medium | n/a | Treat ext3 consistency and container integrity separately; keep off-device backups |
| `errors=remount-ro` can leave a degraded read-only system | Low–medium | New behavior | Stops writes instead of continuing; manual rescue; first-boot acceptance checks |
| Historical `system-before-rca` image is unclean | Low | Yes | Never use it as a recovery image; keep it only as historical evidence |
| Candidate boot not yet observed | Low | n/a | First hardened boot is part of deployment acceptance; rollback immediately on failure |

### 6.5 Deployment conditions

The next authorized deployment must satisfy all of these:

1. **Use the copy-only candidate** `673fa31f…`, derived from the current clean
   production snapshot `fa59de77…`. Do not substitute a rebuilt image without a
   new config/data migration review.
2. Take a fresh clean rescue snapshot immediately before deployment and keep a
   host copy. Do not overwrite any existing backup name.
3. Deploy only through `deploy-root`, with a new `system-pre-hardening-*.ext3`
   backup name and evidence file. Never rename over the only known-good image.
4. Do not add `barrier=1`; do not change partitions, p1, bootloader, kernel, or
   Guardian.
5. Verify the candidate hash and `e2fsck` on the host immediately before the
   write, and verify the p4 readback hash from `deploy-root`.
6. After the first hardened boot, verify:
   - `/proc/mounts` shows `data=ordered` and `errors=remount-ro`;
   - PID1 is `/bin/busybox`, Guardian `state=00000000`;
   - Home renders, Wi-Fi and Tailscale connect;
   - config hashes and document/sidecar are unchanged;
   - no `boot.repairing`; `boot.pending` is normal runtime state.
7. If any acceptance check fails, enter rescue and roll back to the preserved
   `system-pre-hardening-*.ext3` image, then verify hash, `e2fsck`, token
   readback, and a clean boot.
8. Retain the current host copy and p4 images until at least one clean shutdown,
   one clean reboot, and an offline `e2fsck` of a post-deployment snapshot pass.
9. If any rescue session experiences a USB reset, unmount p4, run the UDisks2
   consistency check, repair only after a full control-tree backup, and verify
   all image/document hashes before restoring the boot token.

## 7. Tested rollback sequence

This sequence was executed successfully in Phase 4B.

```text
1. From Alpine: kindle-linux.py rescue
   - removes boot.enabled
   - orderly reboot
   - p4 exports as 0525:a4a5 bcd0200

2. On host: verify identity and current files
   - lsusb idVendor=0525 idProduct=a4a5 bcdDevice=0200
   - confirm mount is vfat and source is the PW3 partition
   - record boot.enabled/pending/once/repairing

3. Verify the rollback image before touching the active slot
   - sha256sum system-pre-hardening-*.ext3
   - e2fsck -fn system-pre-hardening-*.ext3

4. Preserve the active candidate under a new name
   - mv system.ext3 system-candidate-<date>.ext3
   - do not delete the candidate

5. Restore the known-good image
   - mv system-pre-hardening-*.ext3 system.ext3
   - sha256sum system.ext3 and compare with the recorded hash
   - e2fsck -fn system.ext3

6. Clear only stale runtime markers
   - rm -f boot.pending boot.repairing boot.once
   - do not remove a marker whose meaning is unknown; inspect first

7. Restore the boot token last
   - write exactly gleworks-alpine-v1\n to boot.enabled
   - fsync file and directory
   - read the token back and compare exactly

8. Safe eject
   - unmount p4
   - SCSI eject
   - confirm the device reboots

9. Verify recovery
   - new boot ID
   - PID1 /bin/busybox
   - Guardian state=00000000
   - Home, Wi-Fi, Tailscale
   - config hashes and documents
```

The same sequence must be used for a hardened-image rollback. The only
difference is the backup filename and the expected candidate hash.

## 8. Final device state

| Check | Result |
|---|---|
| Boot ID | `820f14c4-01df-422d-a3d9-2918065288c2` |
| PID1 | `/bin/busybox` |
| Root | `/dev/loop1` ext3, `rw,noatime,errors=continue,user_xattr,barrier=0,data=writeback` |
| Root used | 179,052 KiB / 71 % |
| `boot.enabled` | `gleworks-alpine-v1` |
| `boot.pending` | present (normal runtime marker) |
| `boot.once` / `boot.repairing` | absent |
| Guardian | `state=00000000` |
| Wi-Fi / Tailscale | connected / connected |
| Home | rendered |
| p4 FAT | 2,034,176 KiB used / 1,154,464 KiB free / 64 % |
| p1 image | 471,859,200 B, `e50079c1…`, `e2fsck` clean |
| Hardened candidate | host only, `673fa31f…`, not deployed |

The final active root is the restored production lineage. The p4
`system-restoration-candidate-20261031.ext3` image is the post-restore
snapshot; the host retains the exact pre-drill production snapshot
`fa59de77…`.

No p1, bootloader, kernel, partition layout, Guardian, or waveform partition was
modified. No hardened image was deployed.

## 9. Evidence index

Public evidence:

- `docs/evidence/PW3-PHASE4B-RECOVERY.md` — this report
- `docs/evidence/PW3-PHASE4B-RECOVERY.json` — machine-readable summary
- `docs/evidence/raw/PW3-PHASE4B-state-before.txt`
- `docs/evidence/raw/PW3-PHASE4B-restore-boot.txt`
- `docs/evidence/raw/PW3-PHASE4B-rollback-boot.txt`
- `docs/evidence/raw/PW3-PHASE4B-final-state.txt`
- `docs/evidence/raw/PW3-PHASE4B-hardened-faults.{txt,json}`
- `docs/evidence/raw/PW3-PHASE4B-restore-order.{txt,json}`
- `docs/evidence/raw/PW3-PHASE4B-fat-audit.txt`
- `docs/evidence/raw/PW3-PHASE4B-fat-check-before.txt`
- `docs/evidence/raw/PW3-PHASE4B-fat-repair.txt`
- `docs/evidence/raw/PW3-PHASE4B-fat-check-after.txt`
- `docs/evidence/raw/PW3-PHASE4B-usb-reset-journal.txt`
- `docs/evidence/raw/PW3-PHASE4B-remount-after-usb-reset.txt`

Private host copies (archived 2026-11-01 as
`work/private/evidence/archive-20261101/phase4b-20261031.tar.zst`):

```text
work/private/evidence/phase4b-20261031/
  images/system-production-20261031.ext3       fa59de77…
  images/system-hardened-20261031.ext3         673fa31f…
  images/system-post-restore-20261031.ext3     40ff83f3…
  meta/                                         logs, hashes, FAT audit
  screens/                                      restored, rollback and final Home
  p4-before-fat-repair/                         full p4 control-tree backup
  SHA256SUMS
```

Reproducible tools:

- `tools/maintenance/kindle-linux.py rescue|resume|deploy-root`
- `tools/maintenance/pw3-ext3-harden-copy.sh`
- `tools/maintenance/pw3-recovery-sim.py`
- `tools/maintenance/pw3-restore-order-sim.py`

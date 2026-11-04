---
diataxis: how-to
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# Safe reinstall and fallback on PW3

**Current device: stop at host preparation.** The reference PW3 enters USB
rescue before Alpine init. That rescue exposes p4 only. This kit cannot repair
its p1; boot must first be restored through a separately verified recovery
procedure. Do not run another root-image deployment to guess around that fault.
No device operation was performed while preparing this kit.

## What went wrong and what the checks mean

The retained private opt-a restore script replaced `/sbin/init` and copied
modules, but created only `/modules`, omitting `/control` and `/newroot`.
`bootstrap.c` mounts the p4 FAT at `/control`, mounts Alpine at `/newroot`, checks
its layout, moves the control mount, then calls `pivot_root` before executing
Alpine init. Missing p1 directories can therefore leave USB rescue functioning
while Alpine never logs. This is a concrete defect in that script, and a strong
cause hypothesis for the present failure; the actual p1 still needs inspection.
A failed logging-init probe alone does not identify the exact failing syscall.

Do not infer root-image failure from the rescue USB ID. Do not infer p1
compatibility from a booted rescue gadget, `bootstrap --check`, or an image that
booted with a different p1. `--check` prints a build/version string; it does not
exercise mounts, modules or pivot. A clean filesystem is necessary, but cannot
prove executables, kernel ABI, boot scripts or physical recovery.

## Choose the correct entry path

| Observed state | Allowed path | Required fallback |
| --- | --- | --- |
| Rooted Amazon, exact supported profile | Existing `pw3-install.py` state machine; first-install hardware qualification still pending | Fresh default-stock boot, consumed readonly trial and physical USB repair/stock-return proof before promotion |
| Stable existing native Alpine with paired guardian/shutdown | Automatic p4-only kit described below | Host copy of old root, matching clean fallback image, session-bound independent rescue |
| Rescue before Alpine, p1 state unknown (current device) | Host preparation and diagnosis only | Recover/inspect p1 separately; p4 rescue cannot restore it |
| Native boot failed during this kit's install | One automatic fallback attempt if session-bound rescue appears | Preserve failed root and markers; restore fallback on p4, then verify a fresh boot |
| Kernel, bootloader, storage or USB rescue broken | Stop USB automation | Separate hardware recovery; do not guess flash commands |

The supported profile is `G090KB`, kernel `3.0.35-lab126`, p1 start 65536,
p1 size 921600 sectors and p4 size 6385664 sectors. Do not generalize this to
other Kindle models or firmware variants.

## Prepare all files on the host

Use Linux with Python 3, `e2fsck`, `debugfs`, UDisks2 (`udisksctl`), `findmnt`,
`gdbus`, permission to open the physical USB console and sufficient disk space.
Keep a working USB cable and at least 50% device battery. Never run two console,
maintenance or installation tools concurrently. No Amazon account is required
for native operation.

Choose a complete **PW3 native** candidate and a known working fallback from
the same paired bootstrap/guardian generation. An arbitrary Alpine tarball,
QEMU core image, different release's guardian or downloaded Amazon update is
not a substitute. Keep the existing p1 backup even if its bootability is unknown.

```sh
python3 tools/install/pw3-safe-reinstall.py prepare \
  --image /absolute/candidate/system.ext3 \
  --fallback /absolute/known-working/system.ext3 \
  --p1-backup /absolute/backup/p1.ext3 \
  --output /absolute/new-private-kit
```

The preparer does not contact USB. It runs readonly filesystem checks, requires
the native early-boot/console/device scripts and mount directories, checks paired
guardian/module/shutdown hashes, and copies a portable tool subset and guide.
It refuses an existing output directory or mismatched fallback. A prepared kit
contains:

| File | Purpose |
| --- | --- |
| `system.ext3` | Candidate native userspace image |
| `fallback.ext3` | Complete clean matching fallback; never modified in place |
| `p1-backup.ext3` | Private 450 MiB archive; bootability not certified, no automatic flash command |
| `kit.json`, `SHA256SUMS` | Exact payload hashes and image metadata; local integrity records, not a vendor signature |
| `tools/install/pw3-safe-reinstall.py` | Offline verify/status, automatic p4 install, explicit bound rollback |
| `tools/maintenance/kindle-linux.py`, `tools/lib/` | Existing USB identity/nonce and rescue/eject handling |
| `os/*/*.json`, `AGENTS.md`, this guide, `README.txt` | Device/input metadata and operating rules |

The initial private kit uses the saved `1937813d…` image for both candidate and
fallback. This makes it a baseline reinstall/restore kit, **not a new minimal-core
release**. That image ran before the p1 reinstall; it has not booted successfully
with the current broken p1. Historical KOReader/Tailscale contents stay in that
archive; the product direction keeps both as optional apps. New development
images need a separately prepared kit and matching fallback.

Keep the kit and session on a reliable host disk, with a second copy elsewhere.
Do not publish them: images contain private settings and possibly credentials.
SHA256 checks detect accidental changes; an administrator editing both code
and manifests can bypass them. Rules and local state are not tamper-proof.

From the kit directory (the script also supports an absolute `--kit` path):

```sh
sha256sum -c SHA256SUMS
python3 tools/install/pw3-safe-reinstall.py --kit . verify
```

`verify` is offline. It does not establish that either image is hardware-qualified.
An existing kit is immutable; build a new one instead of editing a manifest to
accept a changed file.

## Automatic native reinstall — after boot recovery

This command writes p4 and reboots. It is **not applicable to the current
rescue-only device**. First restore stable native operation; keep the physical
USB cable attached and close other serial/maintenance programs.

```sh
python3 tools/install/pw3-safe-reinstall.py --kit . \
  --session /absolute/new-private-session install
```

The runner performs this sequence without asking between steps:

1. Verify the complete kit again. Require an existing stable native boot,
   at least 100 seconds uptime, a cancelled guardian, persistent writable root,
   exact profile/layout, at least 50% battery and live hashes matching the
   candidate/fallback guardian, module and shutdown helper.
2. Record device identity hash, boot ID, kit digest and a fresh nonce in a new
   private session. Write the identity/nonce marker on p4, remove the normal
   boot request and request a clean PID1 shutdown. No p1 changes are made.
3. Detect only independent USB rescue `0525:a4a5`, bcdDevice `0200`, exact disk
   capacity and the same session marker. Recheck the USB instance during I/O.
   Never select a disk by a guessed host drive name.
4. Require clean install markers; stream the selected old image to the host,
   fsync it and compare its hash with the device. Save marker contents and a
   readonly fsck report before any replacement. Retain the host backup.
5. Require space for the complete new image plus 64 MiB headroom. Write a fresh
   exclusive staging file, fsync and compare hashes, atomically replace
   `system.ext3` on the same filesystem, then sync the directory. Write/read
   back `boot.enabled` **last**. Safely unmount/SCSI-eject before reboot.
6. Wait up to 180 seconds for a fresh native boot stable beyond the guardian
   deadline, with the same identity and paired runtime. Record host acceptance.
   Physically check display, touch, navigation, reading (if installed), USB
   reconnect, suspend and battery separately.
7. If the new native boot is not verified, wait up to 45 seconds for bound
   independent rescue and attempt the fallback **once**. Preserve the failed
   root, markers and readonly fsck report on the host first. Verify another
   fresh stable native boot after fallback. No infinite reboot/repair loop.

The session contains `state.json`, the captured native proof, before/failure
images and fsck logs. Do not delete it to make a failed install look new.
`installed-host-verified` is not panel/touch or whole-OS acceptance.

## Interrupted install or explicit rollback

Inspect offline, keeping all files:

```sh
python3 tools/install/pw3-safe-reinstall.py \
  --session /absolute/same-private-session status
```

If native still runs, use the maintained clean rescue command first:

```sh
python3 tools/maintenance/kindle-linux.py rescue
```

Once the **same session-bound rescue** is available, request one rollback:

```sh
python3 tools/install/pw3-safe-reinstall.py --kit . \
  --session /absolute/same-private-session rollback
```

Rollback requires the original kit digest and identity/nonce marker. It saves
failure state before acknowledging known `boot.pending`/`boot.repairing` tokens,
then replaces the failed root with the already checked fallback. This is not
permission to delete markers manually. Unknown marker contents, a missing
selected root, unknown session phase or a fallback already attempted stops the
runner. If stopped mid-fallback, inspect the retained state manually; do not
reset its phase or create another session to retry.

| Interruption/failure | Expected action |
| --- | --- |
| Before native preflight or missing native console | Stop, no automatic install; current rescue-only machine stays blocked |
| After entering rescue but before staging | Preserve the session and old root; use bound rollback if its preconditions hold |
| Mid-staging, low space, hash mismatch or USB disconnect | Keep staging/old image/session; no blind retry, no automatic marker clearing |
| After root replacement but before boot token | Stay in rescue on the next boot; fallback remains on host; inspect state and use bound rollback |
| After boot token but before host records eject | Treat boot as unknown; inspect actual USB mode and state before any further write |
| Candidate crashes into bound rescue | One fallback attempt; failed image and markers retained |
| Candidate hangs after cancelling guardian | Rescue might not appear automatically; stop after deadline, inspect/reset safely after host unmount; do not promise automatic rollback |
| p1 mount/pivot failure, damaged USB stack, missing session marker | Stop; p4 automation cannot prove or repair p1 |
| Power failure, FAT corruption or eMMC fault | Atomic rename/fsync cannot guarantee storage survival; preserve evidence and inspect offline |

## First install and stock fallback are separate

Use the [first-install tutorial](../tutorials/02-first-install-community.md)
only from the exact known rooted Amazon state. It has its own backup, bind,
stock-default dispatcher, fresh stock return, supervised readonly native trial,
invalid-image physical USB repair/nonce return and promotion gates. It is not
a general reinstall script. The coordinator's first-install hardware acceptance
remains pending; host checks cannot remove that gate.

The maintained default installer creates real p1 `/control`, `/newroot`,
`/modules` and the trial directory before changing init. Promotion rechecks
those directories, empty/unmounted mount targets and exact module hashes.
A restored stock init wrapper alone does not prove a working Amazon OS; stock
state, libraries and a fresh successful stock boot matter too.

`restore-stock-init.sh` requires the stock OS already running with the known
saved wrapper. It cannot execute inside a p4-only rescue disk. Promotion makes
bootstrap the default init; the retained dispatcher/stock wrapper is a recovery
artifact, **not an automatic Amazon selector from p4**. Removing `boot.enabled`
selects USB rescue, not Amazon. A certified persistent Amazon/A-B fallback is
still future work. Do not label this kit as that feature.

## Mandatory rules for agents and operators

- Read [AGENTS.md](../../AGENTS.md) and current status before touching boot.
- Use maintained entry points; never invoke archived opt-a/private scripts.
- No standalone init replacement, raw disk write, repartition, factory reset,
  IDME/bootmode edit, kernel or bootloader flash to fix a userspace problem.
- Do not mix bootstrap, reset module, guard-control or shutdown across builds.
  A matching filename/version is not matching build identity.
- No QEMU image deployment, evidence reuse from another boot/build, session
  edits, checksum rewrites, `--force`, ignored failures or gate deletion.
- No mounted-root fsck/resize, USB export of an active FAT/root, guessed disk,
  forced reboot during host writes or simultaneous console clients.
- No automatic deletion of backups, settings, books or crash evidence for space.
- On an unknown state: stop writes, report the actual mode/step/hash/error and
  retain evidence. Do not improvise a new installer on the device.

See [USB recovery](recover-usb.md) and [install/update](install-update.md).

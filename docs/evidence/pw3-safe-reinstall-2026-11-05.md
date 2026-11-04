---
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# Safe reinstall preparation and readonly device investigation

Scope: host kit preparation, installer guardrails and read-only investigation
of the connected reference PW3. No root image, boot marker, p1, kernel,
bootloader or IDME was written; no device reboot/eject was requested.

## Host implementation and checks

- Added root agent instructions and the [operating guide](../how-to/safe-reinstall.md).
- Added a portable p4-only kit preparer/validator and automatic native reinstall
  runner, with session/device binding, host backup, hash readback, fsync,
  same-filesystem replacement, boot token last, safe eject and one bounded
  fallback attempt. New runner is not hardware-qualified.
- Promotion now requires real p1 mount directories, empty/unmounted targets,
  paired module readback and no interrupted-repair marker before init changes.
- The default dispatcher installer checks its created directories before init
  replacement. The builder copies the root-layout header its source includes.
- Existing tooling suite: **19 tests passed** inside the documented Nix shell.
  The initial plain-host attempt lacked `cc`; it did not pass until rerun there.
- Existing public-source extraction/roundtrip gate: **passed** with 293 files
  before this evidence document was added; validation concerns source packaging,
  not device installation.
- Python compile/lint and both changed shell syntax checks passed.
- Disposable host filesystem probes passed: kit verification, QEMU image
  rejection, unclean candidate rejection before replacement, staged-hash failure
  preserving the selected root/no boot request, fallback preserving failed root
  and markers before replacement/eject, and repeated-fallback rejection.
  These use a host directory in place of USB and do not prove device timing,
  FAT power-loss behavior or native boot.

The prepared private baseline kit has the retained 176 MiB image
`1937813d3fb09be4772552141cae2898521e08cc6d33ab592f863a49f8f4840c`
as both candidate and fallback. The p1 archive is 450 MiB,
`e50079c101692bcb1f48b441d8c87f56350edf2a5cabe37f4e5d54defbab0bd9`.
The full kit is approximately 802 MiB plus tools/metadata. It is private and
contains historical settings/apps; it is not the new minimal-core release.

## Connected device findings

Initially no Kindle enumerated. After the user changed/reconnected cable/port,
the host observed `0525:a4a5`, bcdDevice `0200`, one mass-storage interface,
with **3269459968 bytes** exported. The inner FAT partition starts at sector 16.
No serial interface/`ttyACM` was exposed. This matches the p4 rescue volume,
not whole eMMC or p1 access.

The desktop had automatically mounted FAT writable; it was unmounted and
remounted read-only through UDisks without SCSI eject. The host kernel reports
that FAT was not properly unmounted. No FAT repair was attempted. Device
file content was only read; diagnostic copies were written to the host.

The currently selected `system.ext3` is **256 MiB**, SHA256
`927e9ab68fa9677e18d35691e226bd9ad519b44e666aa275d9ea6c3d3eb3d3eb`.
A host copy has the same hash and passes `e2fsck -fn`. It has the relative
BusyBox init link, real `/old-root` and `/boot/gleworks-control` directories,
a console node and native early-boot/console inittab entries. `boot.enabled`
contains the expected token; `boot.pending`/`boot.repairing` are absent.
No new `debug-init.log` was found on the exported volume. These facts exclude
obvious current p4 layout/superblock problems; they do not prove p1 mounting,
pivot, ABI compatibility or successful execution of init.

The retained opt-a script/log confirms that it installed bootstrap `9db871…`
and modules but only created `/modules`, omitting `/control` and `/newroot`.
That is a concrete reinstall defect. However the saved **older p1** archive
`e50079…` already contains both mount directories and the same `9db871…`
bootstrap. It is the prior native p1, not proof of the current on-device p1
layout after the intervening official-firmware reinstall. Only live p1 access
can settle what is missing or incompatible now. Do not present the omission
as a confirmed live-device root cause or flash the archive blindly.

## Remaining acceptance

- Restore/inspect current p1 separately; USB rescue cannot write it.
- Qualify the new p4-only runner on a healthy native PW3, including failed
  candidate boot, session-bound fallback and physical panel/input acceptance.
- Qualify first install from the exact rooted Amazon state separately.
- Persistent Amazon/A-B fallback is not implemented. This kit's fallback is a
  matching native userspace image, conditional on functioning p1/rescue.
- Minimal-core QEMU/app work remains tracked separately in B33; its image is
  intentionally rejected by this device installer.

---
status: historical
date: 2026-11-02
superseded_by: ../../../../status/CURRENT.md
---

# Persistent Alpine on PW3

> Current runtime/release status: [native-kit/CURRENT.vi.md](../../../guides/CURRENT.vi.md).
> Stage-specific descriptions and pending gates below may describe earlier trials.

Alpine 3.24.3 userspace boots as global PID1 on the retained stock 3.0.35-lab126
kernel. The bootloader and stored kernels are unchanged. Static bootstrap is
installed as `/sbin/init`; it does not start the preserved Amazon init/services.
Amazon files remain on p1 for rollback. The console-only phase is superseded by the minimal reader in
[the native kit](../../../guides/BUILD-INSTALL.vi.md). KOReader display/touch startup is
now available; Wi-Fi, Tailscale, web upload and power suspend remain pending.

Physical USB serial: `0525:a4a7`, normally `/dev/ttyACM0`, raw mode. Send `STATUS`
for status, `SHELL` for an administrator shell, or `REBOOT` for orderly reboot.
Keep reading serial output while requesting reboot. From inside `SHELL`, use
`/bin/busybox kill -QUIT 1` for orderly reboot; do not use `reboot -f` during
normal operation. The shutdown executable runs as PID1 from RAM, unmounts ext3,
then its backing FAT, and resets only after both steps succeed.

`/gleworks-alpine/boot.enabled` on exported user storage contains exactly
`gleworks-alpine-v1` plus newline. Startup creates `boot.pending` and a 90-second
kernel reset deadline. Root stays readonly until early console setup completes
and the deadline is cancelled. Pending remains during all runtime and is only
removed after root unmounts successfully. An unexpected reset therefore selects
our USB rescue, including when the console itself is broken. An unclean ext3
state or pending journal recovery also selects rescue before mounting the root.

Independent rescue USB is `0525:a4a5`, bcdDevice `0200`; stock USB `1949:0004`
is not rescue evidence. This rescue export has no automatic reset while the
host can write. Never export the FAT partition while Alpine is using its image.
Host unmount and SCSI eject close the backing device and trigger reboot.

For recovery, work only on the exact identified rescue device. Keep a copy of
`system.ext3`, repair it offline with e2fsck or replace it with a verified clean
Alpine image. Confirm the ext3 filesystem is clean. Deliberately remove
`boot.pending` only after repair/inspection. Retain `boot.enabled` to resume
Alpine; remove it to remain in USB rescue. Sync, unmount and eject normally.
Do not mount the ext3 image writable simultaneously with the device's use.

Rollback inputs: exact original `/sbin/init.gleworks-stock`, previous
`/sbin/init.gleworks-dispatch`, bootstrap copies in `/gleworks-native-trial`,
and the private host full-p1 backup at
`ours/docs/evidence/private/pretrial-backup/p1.ext3`. If reverting p1 entrypoint,
boot a working Alpine root through USB first and atomically restore the
preserved entrypoint with p1 subsequently remounted readonly. Recovery from
corrupted p1, bootstrap, kernel or physical eMMC is outside the demonstrated
p4/Alpine USB recovery path; do not erase p1 or change bootloader/kernel.

`review.json` and `physical-*.json` distinguish emulator checks from physical
acceptance. Never infer runtime verification from the presence of an image.

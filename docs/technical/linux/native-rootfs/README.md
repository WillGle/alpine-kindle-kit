# Native Alpine root image — host-only candidate

> Current runtime/release status: [native-kit/CURRENT.vi.md](../../../guides/CURRENT.vi.md).
> Stage-specific descriptions and pending gates below may describe earlier trials.

`build-host.py` builds a separate64MiB ext3 image from the pinned complete
Alpine3.24.3 armv7 minirootfs, preserving the proven earlier kernel/initramfs
artifacts. It performs no device access. Output is private under `.private/`.
This image is not an installer and must not be written to the Kindle yet.

Direct `/sbin/init -> ../bin/busybox` plus a minimal inittab starts Alpine with
no Amazon Upstart/services. The image includes apk3.0.8, published Alpine keys,
exact captured USB modules, raw-USB nonce proof, and the exact guardian/control
pair already exercised on the unchanged stock kernel. Root must stay readonly;
trial state is RAM-backed. Guardian reset90s is armed before USB initialization.
There is no exposed shell and no automatic package/network installation.

Coldstart emulation found two corrections now in the builder: stock kinit
checks init before chroot, so the symlink must be relative; static console/null
nodes must exist before sysinit mounts /dev because devtmpfs is disabled.
The original private image/review hashes describe the earlier image and are
retained unchanged. Corrected copies and serial evidence are under
`../bootstrap/emulator/`. Exact captured kernel + stock bootargs coldstarts
Alpine there; an observed copy additionally proves sysinit exit0, BusyBox PID1,
guardian state2/90s, USB modules loaded and ready marker. Deadline restart
occurred. Emulator driver readiness is not physical USB enumeration/proof.

Host checks: complete filesystem e2fsck passes;530 supplied/root inodes checked
UID/GID0 in the actualimage;8 key files extracted and compared byte-for-byte;
actual init symlink checked; apk runs through host ARM user emulation. A later actualKindle ABI check also ran apk3.0.8-r0 successfully from the
full root in tmpfs while Upstart remainedPID1. These checks do not establish
coldboot, earlyUSB ordering or apk package-install compatibility.
Failures before the guardian is armed are not protected by that90s deadline.

Installation remains NO-GO: no demonstrated ordinary-USB rescue independent of
the rootfs being replaced. p2 currently contains Nh_* data, not a usable rescue
OS. Stock kernel disables both special Android/mfg reboot routes. The compiled
bootloader Fastboot source and actual binary do not boot downloaded RAM data;
bootmode selection persists. Retaining bootloader/kernel is insufficient to
prove recovery when p1 init/filesystem fails.

Next actual hardware acceptance needs a functioning device control session,
a reviewed independent rescue entry/return mechanism and ordinaryUSB host
access. Do not erase Amazon OS or replace p1/init merely to discover recovery.

---
status: historical
date: 2026-11-02
superseded_by: ../../../../status/CURRENT.md
---

# PW3 minimal rescue bootstrap and supervised coldboot trial

> Current runtime/release status: [native-kit/CURRENT.vi.md](../../../guides/CURRENT.vi.md).
> Stage-specific descriptions and pending gates below may describe earlier trials.

`bootstrap.c` is a static ARM PID1 spike, using the exact installed kernel.
It does not run Amazon init/services. `build-host.sh` builds it using the
already-built musl1.2.5 static library and existing ARM toolchain. The builder
does not contact the Kindle. `install-default-trial.sh` is a separate explicit
device-side installer for the stock-default dispatcher; its procedure and
rollback are described in `ours/docs/BACKLOG.md`.

A small readonly p1 contains this init, exact captured USB modules, console/null
nodes and `/proc`, `/sys`, `/dev`, `/control`, `/newroot` directories. It mounts
p4's FAT at offset8192, accepts exactly `gleworks-alpine-v1\n` in
`gleworks-alpine/boot.once`, removes/syncs that request, then mounts
`gleworks-alpine/system.ext3` readonly. The Alpine root must have relative
`/sbin/init`, static console/null, `/old-root` and `/boot/gleworks-control`.
The bootstrap moves the FAT control mount, pivots root, detaches the entire old
mount tree, closes inherited file descriptors and execs Alpine init.

Absent request or missing/corrupt image returns to our USB rescue path: unmount
image and FAT before exposing p4 with g_file_storage. It never exports storage
while that storage contains a mounted Alpine root. After replacing the image
and creating the request over USB, safely unmount it on the host and reset the
Kindle; rescue does not poll or mount host-writable storage while exporting it.
A failed post-pivot exec requests reset with the one-shot already consumed.
The supervised dispatcher invokes `--trial`: bootstrap arms the90s guardian
after initial mounts/devices, before FAT/image/USB work. Alpine adopts that
deadline. Explicit earlier errors request reboot; a stall before arming still
may require a manual power-button reset. The p1 trial request is consumed before
bootstrap, so the next normal boot selects the retained stock wrapper. This
does not protect against a broken p1/bootstrap or kernel, or abrupt power loss.

Emulator evidence in `host-review.json` / `rescue-emulator/`: no request, missing
image and corrupt image bind USBstorage to p4; valid image starts AlpinePID1,
sysinit exits0, guardian arms, USBserial driver becomes ready, reset follows,
and a separate next coldstart selects USB rescue. Kernel is the actual captured
3.0.35-lab126 with PW3/Muscat model; synthetic identities/disks only. A model
charger/USBhost warning appears after gadget configuration. Driver binding is
not proof of physical USB enumeration or an external host file transfer.

Emulator results alone do not establish physical recovery. The static binary
and USB serial transitions have now been checked on hardware in RAM, and a
small stock-default dispatcher has passed physical coldboot with authenticated
Upstart return: `default-coldboot-proof.json`. The original rootfs and exact init
wrapper remain available. Independent bootstrap rescue and Alpine coldboot
acceptance are separate gates. Do not execute
the full bootstrap through a live PID1 replacement while stock FAT mounts or
automounters remain: it assumes a fresh boot and would otherwise map the same
physical FAT through another loop device. The RAM --check and separate USB
driver probe do not execute that path. Production needs repeated coldboot and persistent
Alpine operation; the90s-reset trial is not delivery. Amazon OS remains on the
physical Kindle. The independent stock signed-recovery experiment is recorded
separately in `emulator/signed-recovery-review.json`.

Physical native coldboot is now proven separately in
`../native-rootfs-v4-diagnostic/physical-coldboot-proof.json`: fresh native boot
ID, Alpine BusyBox global PID1, own readonly root, no observed Amazon userspace,
sysinit0 and authenticated stock return. Wired serial still does not enumerate
from that coldboot. This milestone does not authorize erasing the recovery OS
or claim persistent Alpine installation; default remains stock until USB rescue
and production boot are accepted.

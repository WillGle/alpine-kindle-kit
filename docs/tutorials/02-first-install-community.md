---
diataxis: tutorial
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: user
---

# Native Alpine installer for Kindle Paperwhite 3

This Linux-host toolkit replaces the Amazon userspace with native Alpine and
optionally KOReader while retaining the installed kernel and bootloader. It does not
require an Amazon account during Alpine operation. It is not a jailbreak,
an Android installer, a mainline-kernel port, or an installer for every Kindle.

**Status: development candidate.** The earlier native system has physical
evidence on one PW3. This new first-install coordinator still needs its own
stock-to-native hardware qualification. Do not read passing host checks as a
claim that this release has been installed on a second device.

Supported entry profile is deliberately narrow: PW3 white Wi-Fi, serial model
prefix G090KB (device code 0KB), stock firmware 5.16.2.1.1, kernel
3.0.35-lab126, the qualified partition layout and known stock init wrapper.
Other PW3 variants are not silently accepted; see [contributing](../reference/contributing.md).

Requirements:

- A rooted/jailbroken stock PW3 of the exact profile, with at least 50% battery,
  600 MiB free user storage, working physical USB and saved Wi-Fi credentials.
- A Linux computer, ordinary USB cable, at least 5 GiB free for backups plus
  build space, and the tools listed in [BUILD.md](../how-to/build-private-candidate.md).
- Your own stock module capture and p1 backup, a clean pinned Alpine archive
  and KOReader bundle. Captured vendor libraries/modules are not redistributed.
- A complete host backup of books and the private install session. Existing
  bootstrap installations must use maintenance, not repeat first installation.

Wi-Fi/Tailscale/web upload and complete suspend are **not implemented in the
community first-install image**. On the author's reference PW3, Wi-Fi
scan/connect and the Tailscale userspace daemon are working (see
[CURRENT.vi.md](../status/CURRENT.md)), but that build has not been
re-verified as a public first-install. Stock Wi-Fi is used only for the
temporary root SSH setup. The native administrator console and recovery use
physical USB.

## Prepare a private control session

Run from the extracted `kindle-native` directory. All example `/path/...`
arguments must point to your inputs, not files from the author's machine.
Build/acquisition instructions are in [BUILD.md](../how-to/build-private-candidate.md).

```sh
python3 tools/install/pw3-control.py \
  --host-address 192.168.1.10 --device-address 192.168.1.20 \
  --dropbear /path/to/clean-koreader/dropbear \
  --libz /path/to/clean-koreader/libs/libz.so.1 \
  --rootfs /path/to/alpine-minirootfs-3.24.3-armv7.tar.gz \
  --output /path/to/private-control
```

Copy `gleworks-alpine-session.sh` and the `gleworks-alpine-session/` directory
from that output into the Kindle's `documents/`, using normal USB storage.
Safely eject, reconnect as needed, then launch the script as root through
your existing jailbreak tooling. The fallback launcher assumes the existing
KMC `su` path; other jailbreak tooling must launch it explicitly as root.
No root exploit is supplied. Do not register an Amazon account for this tool.

The session allows only the named host over Wi-Fi, uses public-key auth,
disables forwarding/passwords, and expires after one hour. Its stock return
launcher is hash-bound to the candidate and starts the same temporary session
after supervised reboots. Keep the addresses stable (for example DHCP leases).
If the session expires, relaunch the **same** private launcher; do not generate
new keys midway through installation. Logs for this launcher remain in tmpfs.

```sh
python3 tools/install/pw3-install.py --session /path/to/private-session init \
  --host 192.168.1.20 --port 2222 \
  --key /path/to/private-control/private/client-key \
  --known-hosts /path/to/private-control/private/known_hosts
python3 tools/install/pw3-install.py --session /path/to/private-session backup
```

The default `--work-dir` is repository `work/`; pass an external work directory
before the subcommand if desired. Explicit `--session` keeps each device/attempt
separate. See [layout](../reference/repository-layout.md).

The initial probe is read-only on the Kindle. Backup streams p1, modules,
userstore files and live Amazon user state to the host. It does not export a
mounted root as USB storage. The Amazon state archive is live, not a certified
consistent full-system restore. Firmware/bootloader and hardware waveform
data are retained on the Kindle; a p1 backup alone cannot recover all damage.

## Build and bind your candidate (base or reader)

The default build is base Alpine. Add `--variant reader --reader /path/to/clean-koreader`
for a reader image. Follow [BUILD.md](../how-to/build-private-candidate.md) using `backup/p1.ext3` and
`backup/stock-modules.tar` from this session. The build creates a new paired
guardian identity and a size-computed image (128 MiB minimum, 25% headroom; currently about 176 MiB); the directory contains private material
and must not be published. Use the launcher from the same private control set.

```sh
python3 tools/install/pw3-install.py --session /path/to/private-session bind /path/to/private-candidate
```

## Supervised first installation

Run one step at a time and read its result. Do not run another USB console,
maintenance tool or installer in parallel. Commands labelled WRITE/REBOOT in
`--help` change the device. They do not delete your books or erase Amazon state.

```sh
python3 tools/install/pw3-install.py --session /path/to/private-session install-dispatcher
python3 tools/install/pw3-install.py --session /path/to/private-session stock-reboot
# Wait for stock boot and temporary SSH to return.
python3 tools/install/pw3-install.py --session /path/to/private-session verify-stock
python3 tools/install/pw3-install.py --session /path/to/private-session trial
# Keep the cable attached. Observe promptly during the 90-second readonly trial.
python3 tools/install/pw3-install.py --session /path/to/private-session observe-trial
# Wait for the guardian to return to stock and SSH to restart.
python3 tools/install/pw3-install.py --session /path/to/private-session verify-stock
python3 tools/install/pw3-install.py --session /path/to/private-session rescue-trial
# The valid image is preserved; only a separate invalid trial image is selected.
python3 tools/install/pw3-install.py --session /path/to/private-session repair
# Wait for stock/SSH to return after safe USB eject.
python3 tools/install/pw3-install.py --session /path/to/private-session verify-rescue-return
python3 tools/install/pw3-install.py --session /path/to/private-session promote
python3 tools/install/pw3-install.py --session /path/to/private-session activate
# Allow at least 100 seconds uptime, then verify (reader check only for reader variant).
python3 tools/install/pw3-install.py --session /path/to/private-session verify-native
python3 tools/install/pw3-install.py --session /path/to/private-session native-rescue
python3 tools/install/pw3-install.py --session /path/to/private-session native-resume
# Allow at least 100 seconds uptime again.
python3 tools/install/pw3-install.py --session /path/to/private-session verify-native
```

Installation evidence binds the device identity hash, private session,
candidate hashes, fresh boots and USB challenges. The CLI cannot attest to
the physical display: for the reader variant separately confirm opening a book,
touch, page turns and repeated full refresh on the actual panel. Framebuffer PNGs are not photos.

## Interrupted operations and recovery

```sh
python3 tools/install/pw3-install.py --session /path/to/private-session status
python3 tools/install/pw3-install.py --session /path/to/private-session reconcile
```

`status` is offline. A pending write blocks normal steps. `reconcile` only
recognizes fully completed dispatcher install or promotion using read-only
postconditions; it does not erase partial state or force a retry. Other pending
states require inspection. Preserve session/backup files and logs and report
the exact step, phase and error with identifying data redacted. Never edit
`session.json` to mark a recovery gate passed. It is local operator state,
not a signature or protection against an administrator intentionally editing it.

Before promotion, absence of the one-shot request boots the retained stock
init. A supervised failed trial should return to stock on reset; the guardian
does not protect bootloader/p1 corruption or all power-loss cases. Do not run
the stock-default installer again on an already-native Kindle.

For an installed system, USB `0525:a4a7` is Alpine serial. Independent rescue
is `0525:a4a5`, bcdDevice `0200`; an empty `a4a6` handshake or stock
`1949:0004` is not writable rescue. Never mount/export FAT while Alpine is
using its root image. Persistent startup attempts offline journal repair once,
with battery at least 50%, before arming the startup reset deadline. Failed or
interrupted repair selects USB rescue. An unexpected `boot.pending` or
`boot.repairing` in rescue requires offline filesystem inspection/repair,
not deleting markers to bypass it. Preserve the image and repair log first;
only acknowledge `boot.repairing` after repair and a clean superblock plus
`e2fsck -fn` verification. Rescue displays a status line over the old panel
contents. Physical hard-reset recovery passed once on the reference runtime;
new-kit first installation remains unverified.
Do not use Amazon Factory Reset, forced reboot, bootmode/IDME edits or a
guessed `/dev/sdX` as a repair shortcut.

The included `kindle-linux.py` provides existing installed-system maintenance
(`inspect`, `rescue`, `deploy-root`, `resume`, `put-documents`); read its help.
Its `package` command now delegates to the same maintained public source
allowlist; it refuses to embed private images. Keep original private candidate and
backup data outside the public repository. Full Amazon uninstall/restore after
cleanup has not been certified.

## Public source and contributions

Create only the reviewed source archive, never a copy of your working directory:

```sh
python3 tools/package/public-source.py /path/to/new-pw3-source.tar.gz
python3 tools/package/verify-package.py /path/to/new-pw3-source.tar.gz \
  --extract-to /path/to/new-empty-check-directory
```

This contains no root image, binaries, token headers, keys, private captures,
book library or device serials. See [licenses](../reference/licenses.md) and
[contribution requirements](../reference/contributing.md). No command publishes a repository
or release. The current qualification limits are in [STATUS.json](../status/STATUS.json).

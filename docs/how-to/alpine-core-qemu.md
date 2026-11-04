---
diataxis: how-to
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# Separate Alpine core and validate it on the host

Work on the host while the reference PW3 is blocked before Alpine init. These
commands do not connect to USB, mount a host block device, change p1, or deploy
an image. The QEMU root is a **VM profile**, not a PW3 installation candidate.
Read [safe reinstall](safe-reinstall.md) before returning to a device.

The sequence is: measure the existing root, compose a clean Alpine core, export
optional apps without personal state, boot ARM QEMU, exercise app lifecycle and
recovery, then decide whether a same-function Buildroot comparison is useful.
The tracked results are in the [evidence report](../evidence/alpine-core-qemu-2026-11-05/README.md).

## Core, profile and app boundaries

| Layer | Contents | Current boundary |
| --- | --- | --- |
| Alpine core | BusyBox init/shell, musl, gw-system supervisor, gwctl, gw-appctl, graphical shell, public app trust key and libcrypto | No KOReader/Tailscale, Python, GDB, user credentials or reader history |
| Alpine maintenance | apk/libapk, its TLS/zlib/CA dependencies, musl-utils/scanelf and baseline config | Retained from the pinned minirootfs; not a compiler/toolchain; can be evaluated separately after measurement |
| VM adapter | Debian ARM kernel/modules, initramfs, virtio disk, serial console, synthetic power/touch input, harmless framebuffer stub | Only for host verification; kernel/modules stay outside the core image |
| PW3 profile | Existing Amazon kernel/U-Boot, matched bootstrap/guardian/shutdown, hardware preparation, USB rescue, Wi-Fi/input/EPDC/power support | Not replaced by the VM adapter; reintegration and fresh hardware acceptance pending UART recovery |
| KOReader app | Reader code, native adapter, fonts, dictionaries and compatibility libraries | Separate signed development package; settings/history/cache/screenshots excluded |
| Tailscale app | tailscaled and a foreground userspace-networking entrypoint | Separate signed development package; VPN state/auth keys excluded; background coexistence/authentication not accepted |

Core builds use `GW_READER_BUILTIN=0`, so the `koreader` app ID uses generic
launch/removal. Legacy builds retain their default built-in-reader behavior.
Core uses a bounded 4096-member package ceiling: the exported reader has nearly
1000 files and does not fit the legacy 512-member ceiling. Signature, path,
link/special-file, size and duplicate-member checks remain enabled.

The UI still has a reader shortcut and legacy metadata integration referring
to `/opt/koreader`. A missing app does not become part of the core, but reading
progress and hardware-managed reader integration need adaptation before PW3
release. Do not equate a separable package with complete app/session isolation.

## Inventory the actual source root

Use an offline copy of the latest saved root image, not the older release
currently selected on the device. For the reference inventory the source is
SHA256 `1937813d3fb09be4772552141cae2898521e08cc6d33ab592f863a49f8f4840c`.
Extract it read-only with `debugfs` to a new private directory; unprivileged
extraction may omit special device nodes. Retain the source image and extraction
log. Never extract over an existing tree or run a repair on the only copy.

```sh
python3 tools/build/rootfs-inventory.py \
  --root work/private/core-qemu-20261105/source-root \
  --image work/private/bench/2026-11-03-jb/backup-p4/gleworks-alpine/system.ext3 \
  --output /absolute/new-source-inventory.json
```

The inventory groups every regular file and records symlinks, actual logical
bytes, unique inode bytes, package ownership, declared apk sizes and missing
registered files. Categories aid review; they are not an ELF dependency proof.
The source registry still lists Python/GDB/debug packages whose executable or
bulk files were already deleted. Do not add their declared sizes and call that
measured bloat. In particular the source has no Python or GDB executable.

## Build a new core from the pinned Alpine input

Requirements: Python 3.11 or newer and the [ARM build environment](build-arm-binaries.md), local musl
cross toolchain/LVGL inputs, e2fsprogs and the pinned archive from
`os/alpine/source.json`. `DYN_ROOT` supplies the dynamic musl linker input;
its default is the retained prepared root described in `toolchain.sh`. This is
a reproducible composition workflow, not a certified clean-room toolchain build.

Correction: the core builder now uses the separate ARMv7 musl hard-float
toolchain pinned in `tools/build/core-toolchain.json`. Download its archive,
verify the pinned SHA256 and extract under
`work/toolchains/bootlin-armv7-musl-2026.08/`, or set `CORE_TOOLCHAIN` to that
extracted toolchain directory. It supplies matching headers, startup objects
and GCC runtime; the legacy soft-float prefix is for bootstrap work only.
Core composition rejects soft-float binaries. See the
[hard-float evidence](../evidence/core-hardfloat-2026-11-05.md).

Each output directory must be new. Enter the documented shell:

```sh
nix-shell tools/build/nix-shell.nix
```

Then build service tools and a fresh static UI without cleaning/replacing the
existing workspace binaries:

```sh
OUT="$PWD/work/build/core-next-binaries" GW_CORE_UI=1 \
  env -u CC -u STRIP sh tools/build/build-core-binaries.sh

python3 tools/build/core-image.py \
  --rootfs work/downloads/alpine-minirootfs-3.24.3-armv7.tar.gz \
  --binaries work/build/core-next-binaries \
  --shell work/build/core-next-binaries/gw-shell \
  --output work/build/core-next
```

The builder starts from the clean pinned archive instead of pruning a private
live root in place. It installs only the core files, normalizes image ownership,
creates static console/null nodes and the relative BusyBox init link, runs
`e2fsck -fn`, and emits `inventory.json`. A 64 MiB ext3 file provides headroom;
its file length is not the amount of core content used. No private app state,
PW3 guardian token or hardware module is taken into this VM image.

## Export optional applications

For **VM validation only**, sign with the committed test fixture key. It is
public test material and gives no production trust. Build a fresh test-key
service binary set; the core image itself retains its production public key.
Only the disposable cloned VM disk gets test binaries/key.

```sh
OUT="$PWD/work/build/core-next-vm-binaries" \
  GW_APP_PUBLIC_KEY="$PWD/os/system/tests/keys/test-release.pem" \
  GW_CORE_VM_SHELL=1 env -u CC -u STRIP sh tools/build/build-core-binaries.sh

python3 tools/build/external-apps.py \
  --root work/private/core-qemu-20261105/source-root \
  --output work/private/core-next-apps \
  --key os/system/tests/keys/test-release.key
```

The exporter preserves code/resources and places compatibility libraries inside
the reader package, refuses symlink inputs, excludes personal data, and emits
`.gwapp`/`.sig` pairs plus a file hash/exclusion report. `0.1` is the development
packaging revision, not the upstream application's version. Production
packaging needs a private release key and a separately reviewed clean app
source. Neither test packages nor private captures are a public release.

App data uses `GW_APP_DATA`; the reader maps `KO_HOME`/`HOME` there. Uninstall
retains appdata. KOReader's real display/input launch needs the device profile
to provide `GW_TOUCH_DEV` and the managed power/display contract. Tailscale's
entrypoint runs a foreground daemon with private state/socket paths. It is not
yet a background-service integration alongside the reader; no VPN account or
network authentication is exercised by the VM smoke check.

## Run QEMU against a disposable image copy

Use a matching ARM kernel and complete module set. The recorded run uses Debian
`6.12.107+deb13-armmp` (`linux-image-6.12.107+deb13-armmp_6.12.107-1_armhf.deb`),
with the package's `usr/lib/modules/6.12.107+deb13-armmp` directory. Kernel hash:
`b508abca3127cb0d9e1b3aba37f400cbd7dac170040641756807e0e496039de8`.
This kernel is **not** a proposed PW3 kernel replacement.

```sh
python3 vm/qemu-system/core-check.py \
  --core work/build/core-next \
  --kernel work/downloads/qemu-arm/vmlinuz \
  --modules work/downloads/qemu-arm/kernel/usr/lib/modules/6.12.107+deb13-armmp \
  --test-binaries work/build/core-next-vm-binaries \
  --external-apps work/private/core-next-apps \
  --output work/private/core-next-check --timeout 180
```

The harness clones the image, builds a minimal VM initramfs, loads matching
virtio/ext4/evdev/uinput modules, mounts ext3 and switches into BusyBox init.
It boots a 512 MiB `virt` ARMv7 guest with networking disabled. Real app checks
expand only the disposable disk to 256 MiB; the core artifact remains 64 MiB.

A supervised console fixture drives signed app checks as the actual shell PID:
empty app list, install/run/stop, update with retained old version, nonzero exit,
SIGSEGV termination, signature rejection, manual active-link rollback,
rerun/removal and retained appdata. The reader app ID is installed/launched/
removed generically. Rollback is an explicit version-link operation in this
probe; it is not a new app rollback UI or automatic OS A/B updater.

The fixture then execs the freshly built **real graphical shell**, which runs
its normal event/render loop with EPDC/touch acceptance flags off. Synthetic
evdev devices satisfy discovery; the framebuffer path is a VM-only `/dev/null`
stub. No panel pixels, touch gesture quality or EPDC timing are proven. Real
KOReader/Tailscale bundles are installed, run with `--version` and removed;
that proves packaging/loader paths, not reading or VPN operation.

After app removal, the harness records guest meminfo, df and a 10-second
`/proc/stat` interval while the real shell remains alive. `result.json` includes
the checks, metrics, kernel hash and core hash before/after. Preserve failures,
`command.json`, `boot.log`, overlays and reports; reruns require a new output.

## Verify explicit recovery console

```sh
python3 vm/qemu-system/core-check.py \
  --core work/build/core-next \
  --kernel work/downloads/qemu-arm/vmlinuz \
  --modules work/downloads/qemu-arm/kernel/usr/lib/modules/6.12.107+deb13-armmp \
  --test-binaries work/build/core-next-vm-binaries \
  --recovery --output work/private/core-next-recovery
```

`gleworks.recovery=1` on the **VM kernel command line** bypasses the supervisor
and UI, retaining the BusyBox serial shell. The harness checks a shell command
and confirms gw-system is absent. Missing early-boot readiness also prevents
supervisor startup. This is a console recovery path for the VM root, not the
independent PW3 USB rescue, an automatic boot-attempt rollback, or repair of p1.

## Interpret measurements and decide about Buildroot

Compare bytes of actual files, core dependencies and the same functionality.
Keep image capacity, filesystem used blocks, regular-file payload, packages'
registered sizes and app installation headroom separate. RAM uses
`MemTotal - MemFree - Buffers - Cached`; CPU busy uses the delta of the first
eight `/proc/stat` counters, excluding idle and iowait. The report preserves raw
inputs. A short VM sample with a new Debian kernel does not predict PW3 RAM,
battery consumption, touch latency, rendering smoothness or boot time.

At this baseline, shell, BusyBox, musl and app signature crypto dominate the
8.3 MiB payload. The remaining Alpine maintenance closure is much smaller than
the removed apps; no compiler/Python/GDB runtime is bundled. Keep Alpine for
now. Removing apk/TLS/CA utilities is a smaller optional experiment before
changing the distribution. Buildroot is justified only if a measured remaining
cost violates the product budget; any comparison must retain the same shell,
app verification, install/removal/update/rollback and recovery functionality,
and report which maintenance capability was lost. No Buildroot build or speed
advantage is claimed by this work.

## Return to PW3 only after UART recovery

1. Inspect/restore the actual p1 via a separately verified UART procedure. Check
   real `/control`, `/newroot`, `/modules`, current init and paired guardian/
   shutdown before attempting a userspace boot. Retain Amazon U-Boot/kernel.
2. Prove mount/root layout/pivot and a fresh native boot with the exact p1/root
   pair. Qualify independent USB rescue and restore before promotion.
3. Adapt the core to the PW3 profile, including device nodes, firmware/modules,
   USB console/recovery and hardware initialization. Do not deploy this VM root.
4. Test real panel refresh, touch, reader handoff, power button/frontlight,
   battery reporting, Wi-Fi/reconnect, suspend/wake and USB reconnect.
5. Exercise candidate failure/fallback on actual storage and measure resources
   again. Keep [safe reinstall](safe-reinstall.md) as the installation authority.

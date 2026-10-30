# Alpine PW3 research — 2026-10-29

## Latest verified milestone — 2026-10-30

Alpine3.24.3 BusyBox/musl ran as actual global PID1 with root in /dev/shm on
unchanged stock3.0.35;44 fresh-nonce proof observations through90.33s. Existing
Amazon processes temporarily retained for Wi-Fi/SSH. Deadline reset followed;
fresh authenticated SSH proves changed bootID, restored UpstartPID1 and cleared
RAM trial. See [proof](../../../linux/pid1-handoff/pid1-proof.json) and
[method and limits](pid1-handoff/README.md). Not independent cold boot, clean
Amazon-free runtime, modern Tailscale or permission to erase stock rootfs.
Persistent cutover remains NO-GO until ordinary-USB rescue independent of the
rootfs being replaced is demonstrated. Modern-kernel DMA handoff still NO-GO.
Earlier entries below are chronological evidence, not the current status.

Scope: retain installed U-Boot, independently boot Alpine, reading first, SSH
client/server and HTTP document transfer over Tailscale, ordinary USB only.
Research only: no device writes or boot changes, no Ponder dependency.

## Decisions supported by this investigation

1. Use Alpine v3.24 ARMv7 as a candidate userspace branch. Official release
   listing includes 3.24.3 ARMv7 minirootfs and U-Boot archives. A minirootfs is
   userspace, not a Kindle kernel or bootable appliance. Release bytes, signatures
   and dependency closure have NOT been downloaded/verified here. Pin the exact
   point release and hashes during acquisition; do not use moving latest-stable.
2. Prefer a modern compatible kernel with TUN support for ordinary Tailscale
   networking. Current captured config explicitly disables TUN, KEXEC, DEVTMPFS
   and EXT4. Userspace Tailscale can avoid TUN but does not remove the runtime's
   minimum Linux version requirement. Go 1.24 requires Linux >=3.2, whereas this
   device runs 3.0.35. Do not claim current Tailscale is supported on that kernel
   or fix this by silently pinning an obsolete runtime.
3. Keeping U-Boot does not necessarily require an additional bootloader. The
   reviewed ARM handoff in lib_arm/bootm.c:83–180 builds ATAGs and calls the
   kernel with their address. A candidate is an ARM zImage with appended PW3 DTB,
   wrapped in legacy uImage, with ARM_APPENDED_DTB/ARM_ATAG_DTB_COMPAT support.
   Linux has ATAG-to-FDT conversion code. This is an engineering hypothesis,
   NOT a verified image recipe: validate exact selected kernel config, memory
   layout, command-line precedence, custom vendor tags and board data first.
4. Recoverability is a separate gate. Existing Fastboot boot handler does not
   launch downloaded RAM data. A valid diagnostics-kernel CRC does not establish
   usable diagnostics rootfs or a cable-only restore path. No mode switch/flash
   is justified by this research. No software-only recovery guarantee found.

## Hardware support matrix

| Area | Existing evidence | New Alpine/kernel work still needed |
| --- | --- | --- |
| CPU/RAM/eMMC | i.MX6SL ARMv7, 512 MiB; captured layout | PW3 device tree, storage/regulator initialization |
| Display | mxc_epdc_fb + proprietary mxc_epdc_eink | Public compatible driver, panel waveform/calibration handling, refresh API |
| Touch | cyttsp4 active | Controller reset/interrupt/power mapping and driver port |
| Power/frontlight | MAX77696 family | Correct regulators/charger/backlight, suspend/resume and measured consumption |
| Wi-Fi | ath6kl_sdio, SDIO 0271:0301 | Board firmware/calibration, power sequencing and reconnect |
| USB | Stock mass storage works | Recovery availability independent of normal OS; gadget config for new kernel |
| Reader | Stock Kindle KOReader accepted earlier | musl build/runtime and device integration without Amazon services |

Public katadelos/linux-3.0.35-lab126 repository explicitly lists PW3 support.
It is useful as a hardware reference, NOT a selected modern production kernel.
A complete publicly downloadable modern PW3 port has not been verified in this
pass. Ponder's inaccessible patches are not assumed available. Generic i.MX6SL
or Kobo support alone does not establish PW3 support.

## Userspace and service compatibility

- Official Alpine package index lists ARMv7 OpenSSH client and OpenRC server
  integration. Resolve exact package versions/dependencies from signed APK indexes
  before building. Tailscale package detail fetch failed; related indexed
  subpackages are insufficient proof of the desired OpenRC installation set.
- KOReader upstream issue #12226 discusses musl/non-SDL framebuffer integration
  on Kobo. It is relevant engineering context, not PW3 acceptance. Do not simply
  copy the glibc Kindle binary and assume it works on Alpine. Check interpreter,
  native libraries, refresh ioctls, input, frontlight and power service adapters.
- Existing ours/web/app.js supports sequential multi-file selection/uploads,
  token auth and status text. It does NOT implement drag/drop handlers or actual
  byte-progress reporting. These remain implementation tasks, not completed
  features. Review ours/device/server.lua with the chosen Alpine Lua runtime
  before reuse; no code changes were made during this research.
- Use OpenSSH over the tailnet for ordinary administration; this does not require
  enabling the separate Tailscale SSH feature. Validate actual LAN isolation,
  authorized tailnet access and reconnect after suspend on device.

## Acquisition/build sequence

1. Obtain the public vendor kernel reference and a selected modern kernel source;
   record commit IDs, review PW3 wiring and identify missing driver patches.
2. Fetch pinned Alpine ARMv7 rootfs plus published checksums/signature; resolve
   signed package indexes for OpenRC, OpenSSH, networking and Tailscale. Inspect
   artifacts on host before running anything. ARM user-mode emulation can check
   ABI/userspace, not prove old-kernel compatibility or physical device behavior.
3. Create a compatible kernel/DTB configuration and build with recorded toolchain
   and logs. Validate image header/payload sizes against measured layout and
   memory use. Do not change primary bootloader or choose write offsets by guess.
4. Establish the ordinary-USB installation/recovery procedure before persistent
   device mutations. Only then schedule on-device boot, display/power and service
   acceptance. Until then host builds are experiments, not install packages.

Host git ls-remote for the vendor reference failed with DNS resolution error
in this session. Web research worked; full source acquisition/build did not.
No source snapshot of the selected modern kernel exists locally yet. No ARM
rootfs boot or package-runtime test was performed.

## Primary sources consulted

- https://alpinelinux.org/releases/
- https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/armv7/
- https://pkgs.alpinelinux.org/contents?arch=armv7&branch=v3.24&name=openssh-client-default&repo=main
- https://pkgs.alpinelinux.org/package/v3.24/main/armv7/openssh-server-common-openrc
- https://github.com/katadelos/linux-3.0.35-lab126
- https://github.com/torvalds/linux/blob/master/arch/arm/boot/compressed/atags_to_fdt.c
- https://go.dev/doc/go1.24#ports
- https://tailscale.com/docs/concepts/userspace-networking
- https://tailscale.com/docs/install/linux
- https://github.com/koreader/koreader/issues/12226

Local evidence: stock-kernel.config, uboot-lab126/lib_arm/bootm.c,
BOOT-CAPTURE-REVIEW.md and ../web/app.js. Web sources establish upstream
availability/requirements, not boot success on this device.

## User-selected reference — 2026-10-30

Retain [schuhumi/alpine_kindle review](ALPINE-KINDLE-REFERENCE.md) and the
clean local checkout at commit dc87d5b63856596e440f89363e2dcd63eb9797b3.
Its reported PW3 chroot deployment informs Alpine userspace, desktop/input
integration and USB storage ownership. It depends on the stock kernel/X11
and Amazon Upstart; it does not qualify independent boot or recovery.


## Prepared candidates — 2026-10-30

The historical acquisition status above is superseded by
[Alpine artifact review](../../../linux/alpine-boot/host-review.json) and
[rootfs provenance](../../../linux/alpine-boot/rootfs-review.json): pinned official bytes and a
minimal modern-kernel Alpine initramfs are now host-built, not device-booted.
[Memory derivation](../../../linux/alpine-boot/memory-review.json) ends at0x807b6b10; no DMA GO.

A second [host-only candidate](stock-alpine-boot/README.md) preserves the exact
stock kernel and passes a separate raw Alpine initramfs through legacy MULTI.
Source/format and captured decompressor arithmetic pass host checks. Keeping
stock Linux3.0.35 preserves driver compatibility but not current Tailscale
support; independent PID1, USB and recovery still require hardware validation.
The generated image is not an install package and remains off the device.

Current next hardware step is the [temporary control session](control-session/README.md):
returning Alpine ABI/staging diagnostics and authenticated Wi-Fi SSH, requiring
one physical launcher activation. Chroot diagnostics remain distinct from
native boot. No Amazon OS deletion or persistent boot mutation has occurred.

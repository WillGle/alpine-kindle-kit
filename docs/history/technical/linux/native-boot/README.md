---
status: historical
date: 2026-11-02
superseded_by: ../../../../status/CURRENT.md
---

# Native diagnostic boot preparation

Status: kernel, embedded initramfs, USB observer and load-only readiness batch
built and host-checked. No new kernel has executed on Kindle. This is not an
Alpine product image and not a boot launcher.

The separate build preserves the previously device-tested diagnostic/staging
artifacts. Linux6.18 source remains clean at the previously pinned commit.
The new kernel embeds a static ARM PID1 and appends the investigational PW3
DTB. Forced command line avoids selecting the stock root filesystem. Storage,
display, networking, PM and watchdog drivers remain disabled. ATAG conversion
is disabled to avoid depending on stale stock ATAG memory; the board memory
description is in the appended DTB. Hardware USB/power/clock support is still
unvalidated. A disabled watchdog driver does not disable an armed hardware
watchdog.

PID1 mounts devtmpfs/proc/sysfs and waits for ttyGS0. It only responds to a
32-hex-digit challenge with a marker containing the running kernel release,
PID1 and the challenge. No shell, root-disk mount, package installation,
watchdog open, reboot or handoff command is present. The host observer requires
an explicitly selected USB tty and only confirms a fresh matching challenge
from a6.18 PID1 marker. This is a liveness/identity check in this controlled
experiment, not cryptographic kernel attestation.

## Consolidated device readiness batch

Copy readiness-bundle/* into the host-mounted Kindle documents directory,
eject safely, run **GleWorks Boot Readiness**, then reconnect. One launch gathers
the missing watchdog/reset state and stages the new initramfs-containing
image using the already device-tested load-only module/client. There is no
execute operation; running it will not boot the new kernel or reset the device.

The batch records current stock boot ID, uptime, root mount status, accessible
watchdog sysfs data and fd holders. With the expected imx2-wdt.0 MMIO resource,
the small static helper maps /dev/mem O_RDONLY/PROT_READ and reads only WDOG1
WCR/WRSR/WICR and SRC GPR9/GPR10. No MMIO writes, timeout changes, keepalive or
/dev/watchdog open. If access is denied, the failure is retained; no bypass is
attempted. These targeted reads run before the12MiB free-RAM staging guard so
the relevant hardware evidence can still be returned if staging is declined.
Temporary helper binaries/nodes are private and cleaned up normally. The
script retains the existing module/config/hash/taint guards and ordinary
module cleanup. Raw evidence stays private in /mnt/us/gleworks-boot-readiness-*.

Inspect host-review.json for exact image sizes and expected padded readback
checksums, readiness-artifacts.json for hashes, and the bundle manifest.
build-host.sh now rebuilds PID1/reader, kernel/DTB, checks the actual generated
CPIO, packages the appended image, refreshes script hashes and the whole bundle.
No stale manual packaging step is required. Build tools and prior toolchain
authentication/compiler/source limits remain documented in ../abi-probe/.

## Criteria before a real boot attempt

1. Current register state must support a credible watchdog reset path and not
   select an unexpected ROM download/boot source. A register snapshot alone
   does not prove reset or recovery.
2. Demonstrate watchdog reset returning to the unmodified stock system, with
   pre/post boot IDs and hardware reset evidence. Do not use a normal reboot
   or a constant-zero ioctl result as proof of timeout recovery.
3. Complete/review native3.0.35 handoff adaptation: device shutdown, IRQ/FIQ,
   cache/MMU teardown, relocation and correct DTB pointer. No execution wrapper
   is enabled by this readiness package. Existing stock soft_restart is absent.
4. Keep storage/display untouched during the first diagnostic boot; observe a
   live USB challenge response and then verify return to stock. Absence of a
   response means unconfirmed boot, not success or proof that it never started.

The agent currently has mounted USB storage access, no live Kindle shell or
working network administration session. It cannot launch an on-device script
from that storage view. The user must perform the existing launcher action;
there is no autorun/init-hook/boot-selector modification here.

When a real diagnostic boot trial is ready, the host observer can be started
in another terminal with an explicitly chosen USB ACM path:

```
python3 observe-usb.py --device /dev/ttyACM0 --seconds 180 --output usb-observation.json
```

Do not run that command now expecting this load-only batch to produce a boot
response. The marker appears only if the new kernel actually reaches PID1
and its USB gadget works. No ordinary-USB recovery or no-brick guarantee has
been established by building these artifacts.

## Readiness hardware result — 2026-10-29 21:01

Capture `gleworks-boot-readiness-20261029-210135-26717`: 19 private files,
16/16 manifest hashes verified and byte-identical to USB evidence. WDOG1
WCR=0x773d (enabled, 60s, STOP suspend, no WAIT disable), WRSR=0x10,
WICR=0x8004. SRC GPR9=GPR10=0. No watchdog fd holders captured, root ro,
no new dmesg lines, modules unchanged, taint1. WRSR0x10 is not proof of a
watchdog timeout. New-image staging stopped at the 12MiB free-RAM guard;
no module loaded and no native kernel executed.

Next: bounded watchdog reset qualification with driver-feed restoration on
abort/deadline, before native handoff. Idle/busfreq paths also feed watchdog;
no-reset outcome must remain inconclusive. No flash/boot selector changes.

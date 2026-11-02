---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# Barebox 2026.10.0 PW3 source review

Reviewed the actual downloaded source on 2026-10-29. Source working tree clean.
Annotated tag v2026.10.0 resolves to commit
5c46c205c8d1a7c57b9dbaa5e516758a6c0a4d80, matching the recorded download commit.
The clone warning is due to the tag object; detached HEAD is expected. This is
identity consistency verification, not cryptographic publisher authentication.

## Findings

1. No demonstrated USB-only entry/recovery route. Board documentation requires
   an existing U-Boot console for chainloading, or physical eMMC intervention for
   ROM USB download. Its special micro-USB UART cable is not an ordinary USB cable.
   init/usbserial starts ACM only AFTER execution reaches Barebox. A functioning
   USB console after boot is not a path into Barebox from the running stock OS.
2. Select barebox-kindle7-dp75sdi.img for this 512 MiB PW3. images/Makefile.imx
   lines 388–391 explicitly use flash-header-kindle-mx6sl-512m.imxcfg. The
   kindle6 example in documentation uses the separate 256 MiB setup (383–386).
   Do not copy the documentation's example image name blindly.
3. Default fallback is broken by source-level inspection: boot.default selects
   mmc_kernel, which calls `bootm -c /dev/mmc1.kernel`. The same release removed
   -c/-s (Documentation/migration-guides/migration-2026.10.0.rst and
   commands/bootm.c). Must adapt the fallback and verify it before relying on it.
   No source changes made; runtime reproduction unavailable without a build.
4. Defconfig enables CONFIG_IMX_IIM_FUSE_BLOW. A future experiment build should
   disable fuse programming. Merely enabling it is not evidence that boot burns
   fuses; no fuse-writing invocation was found in the reviewed board scripts.
5. PMIC init writes regulator registers. RAM chainloading is not free of hardware
   side effects. Its backlight condition names kindle6/7-dp75sdi whereas the
   shared DTS model is kindle-dp75sdi; verify the final runtime model/condition
   before assuming the backlight initialization executes.
6. Default boot remains the Amazon kernel. This configuration is a bootloader,
   not the requested independent Linux OS. The board DTS is not a full PW3 Linux
   display/touch/Wi-Fi/power port. Kernel/rootfs selection remains unresolved.

## Useful storage mapping from source, not a write recipe

arch/arm/dts/imx6sl-kindle-common.dtsi describes the boot0 userdata region at
0x5e000, size 0x1400, and kernel image in the eMMC user area at 0x41000,
size 0xe00000. board.c consumes board identity and bootmode from userdata.
These definitions must be checked against captured device contents before any
use. Do not map Barebox mmc1 numbering directly to Linux device numbering.

## Next investigation

Read this unit's stock U-Boot implementation to determine whether a software-
selected boot mode supports ordinary USB image upload and a return path without
changing the primary bootloader. Candidate public source:
https://github.com/katadelos/uboot-2009.08-lab126
A source branch is not proof it matches this firmware; compare implementation
and on-device boot image evidence. No idme write/reboot experiment is authorized
by mere presence of an executable or a source constant.

## Verification and boundary

Executed git status, rev-parse for HEAD and tag, cat-file type, and source reads
of the board startup code, image mapping, DTS, default environment, defconfig
and migration guide. No native/ARM build or device runtime trial performed.
No ARM GCC/binutils installation identified in the local Nix store scan.
No device writes and no edits to downloaded source. Goal remains incomplete.

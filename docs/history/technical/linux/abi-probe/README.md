---
status: historical
date: 2026-11-02
superseded_by: ../../../../status/CURRENT.md
---

# PW3 inert ABI probe candidate

Status: host libraries checked and inert ARM module built/inspected.
One normal device load/unload trial passed and was reviewed; no kernel
handoff is implemented.

The module only writes load/unload messages to the kernel log. No symbol
resolution, ioctl, device node, memory-patch, kexec or reboot interface exists.
Compile-time guards require kernel version 3.0.35 and the 292-byte module
descriptor observed in all 36 captured modules. Those checks and vermagic are
necessary evidence, not a complete ABI proof or guarantee against a crash.

Use the newly acquired linux-stock-5.16.2.1.1 source with the exact captured
stock-kernel.config in an out-of-tree build. Do not enable CONFIG_KEXEC in that
kernel configuration merely to compile this probe. Do not use force-load,
force-unload, fake Module.symvers or edited vermagic. Do not run a new cold-boot
6.18 kernel's module against the installed 3.0.35 kernel.

Before runtime consideration: review compiler executable/signature and host
dependencies; prepare matching generated headers; inspect warnings, ARM ELF,
vermagic, module descriptor layout/relocations and undefined symbols against
the captured stock modules/exports. Missing symbol-version CRCs do not verify
layout. Review source/config provenance and the proposed monitoring/unload
procedure before device loading. gleworks-abi-trial.sh is a prepared,
not-yet-executed bounded load/unload trial; it never force-loads/unloads.

Public GCC 4.9.4 kernel-only candidate:
https://kernel.org/pub/tools/crosstool/files/bin/x86_64/4.9.4/
It differs from recorded GCC 4.9.1. Any output remains a host candidate until
ABI/behavior review; these old tools are solely for matching the stock-kernel
probe, not the target Alpine product toolchain. fetch-legacy-toolchain.sh saves
the archive and detached signature without executing compiler binaries.

Host attempt: GCC driver runs and reports 4.9.4 / arm-linux-gnueabi, but cc1
requires libmpc.so.3, libmpfr.so.4 and libgmp.so.10. The Nix store has MPFR
SONAME 6, not 4; do not substitute a symlink across that ABI change. Fetch
MPFR 3.1.6 and MPC 1.0.3 sources with ../fetch-legacy-host-libs.sh for an
isolated host-only build (MPC should use the same legacy MPFR). No installation
into the host OS is required. Compiler PGP verification remains pending;
the saved SHA256 only detects changes relative to the acquired archive.

build-host.sh enters the output directory directly to avoid legacy Kbuild's
hardcoded /bin/pwd. Original source remains untouched. oldconfig drops
CONFIG_FB_MXC_EINK_REAGL=m because its Kconfig entry is absent in the public
release. References found in drivers/video/mxc/mxc_epdc_fb.c, none in the
searched include/ and arch/arm/include/ headers. This is a recorded source
gap, not permission to call the generated config byte-identical to the device.
Before any module load, complete structural comparison and investigate any
additional config drift. The earlier cc1 dependency failure is resolved by private host libraries.
No kernel source edits, fake Module.symvers or vermagic edits were used.

## Verified host result and proposed device trial

MPFR 3.1.6: 159 passed / 1 skipped / 0 failed. MPC 1.0.3: 64 passed / 0
failed. Upstream test suites only; no maintained tests added. Libraries installed
only into host-prefix/. build-host-libs.sh records the Nix host paths; extracted
host-sources/ inputs are needed before running it. PGP verification is pending.

Module: gleworks_abi_probe.ko, 2544 bytes; hash in module.sha256 and
abi-comparison.json. ELF32 little-endian ARM relocatable EABI5. Vermagic and
292-byte descriptor match all 36 stock modules. init_module/cleanup_module
relocations at 188/284 match the 31 stock modules exposing those relocations.
Only unresolved symbol is printk, exported in captured runtime kallsyms.
Disassembly shows only logging calls. All relocation types emitted here are
handled by the public stock ARM module loader. CONFIG_KEXEC stays disabled.

Kbuild warns that Module.symvers is missing. This is expected from
modules_prepare rather than a full kernel build, but must remain visible:
modpost did not validate imports against a full kernel symbol table. The
single import was checked independently against the runtime capture. No
symbol-version CRC was invented (stock CONFIG_MODVERSIONS is disabled).

To perform the reviewed inert trial, copy gleworks_abi_probe.ko and
gleworks-abi-trial.sh into /mnt/us/documents/ using the host's Kindle mount.
Safely eject, then launch GleWorks ABI Trial on Kindle through the existing
shell launcher. Reconnect USB afterwards for evidence review. This script
requires the known config hash, kernel, module hash, enabled module loading,
and unchanged baseline taint=1. It records dmesg/module/taint snapshots,
uses ordinary insmod then ordinary rmmod, and produces a private
/mnt/us/gleworks-abi-trial-* evidence directory. Failure stops without force,
bootmode edits, reboot, firmware writes or kexec. Logs can contain private
kernel addresses; do not publish raw captures.

Kernel module loading can still crash/hang despite these comparisons. The
shell cannot monitor or recover a frozen kernel; snapshots and sync preserve
some evidence only. No claim of guaranteed USB recovery or no-brick safety.
A successful load/unload will establish only this probe's runtime behavior,
not safety of a future kexec implementation or a working Alpine boot.

## Device trial verified: 2026-10-29

User ran GleWorks ABI Trial and reconnected. Capture
`gleworks-abi-trial-20261029-201039-20043`: COMPLETE present, all 9 manifest
entries pass. All 13 files copied byte-identically to private-captures/ with
0700 directory/0600 files. Module SHA256 matches the reviewed host artifact.
Normal insmod/rmmod completed. Module absent before, present during, absent
after. Stock module listings before/after are byte-identical. Taint stays 1.

Kernel circular buffer advanced: three old leading lines were replaced by a
partial line. All subsequent baseline lines remain unchanged. New tail only:

```
[15840.372781] gleworks_abi_probe: loaded; module descriptor=292 bytes
[15841.449962] gleworks_abi_probe: unloaded
```

Old userspace undefined-instruction/segfault entries predate this trial; no
new kernel error in the retained delta. See abi-probe/runtime-review.json.
This proves one inert probe's normal load/unload, not arbitrary module ABI,
kexec handoff, Alpine boot, USB recovery or long-term stability. No kernel
handoff, bootmode edit, firmware write or reboot requested by trial script.
Next research stage is a separate bounded RAM allocation/load/unload candidate
without execute/reboot. Its implementation and review remain pending; the
USB-only recovery gate still applies before any kernel handoff.

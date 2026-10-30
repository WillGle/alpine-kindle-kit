# PW3 boot preparation — 2026-10-29

Status: host diagnostic preparation only. On-device boot is blocked by the
unproven ordinary-USB recovery/entry route. No device command is included.

## Verified host result

Linux source: 7d0a66e4bb9081d75c82ec4957c50034cb0ea449, unchanged.
Out-of-tree build completed: zImage 1813344 bytes, DTB 20105 bytes. Both kernel
ELFs are little-endian ELF32 ARM without PT_INTERP or PT_DYNAMIC. zImage magic/
declared extent, DTB magic/extent and all three SHA256 manifest entries pass.
Resolved Kconfig confirms i.MX6SL, MMU, appended-DTB/ATAG support, forced
command line and built-in USB serial; storage/display/network/PM exclusions
were checked explicitly. No execution on the Kindle has occurred.

One dtc warning remains in the inherited upstream SoC description: duplicate
unit address for iomuxc-gpr and pinctrl at 0x020e0000. It is retained in dtc.log;
compilation is not a DT schema or electrical validation result.

Toolchain: target Clang 21.1.7, LLD 22.0.0, LLVM objcopy 21.1.7; exact versions
in toolchain.txt. Initial Nix compiler wrapper injected a host include option;
initial linker wrapper injected an x86 interpreter segment. Both target tools
now bypass wrappers. Structural ELF checks reject those interpreter/dynamic
segments before recording hashes. Host helper builds keep the Nix host wrapper.
The retained final build log is from the incremental verified build; generated
Kbuild command files remain in build/.

## Prepared artifacts

- probe.config: minimal Linux 6.18 diagnostic configuration for i.MX6SL,
  appended DTB plus ATAG compatibility, a forced diagnostic command line and
  USB ACM gadget. MMC, MTD, networking, display and power-management features
  are disabled. This is separate from the eventual Alpine product kernel.
- imx6sl-pw3-probe.dts: investigational 512 MiB SoC/USB description. USB device
  controller enabled; board PMIC/VBUS operation is still unvalidated. It is
  not a complete PW3 device tree. Inheriting the SoC tree avoids importing
  another reader's voltage or GPIO assignments.
- build-host.sh: host-only build using the pinned Linux tree and existing Nix
  tools. Records toolchain, resolved config, build logs and artifact hashes.
  Target clang bypasses the host-libc wrapper; HOSTCC retains its host wrapper.
- waveform-analysis.json: verified structural/checksum summary without panel
  identity. Original and derived private images remain under private-captures.
- stock-images.json: original uImage dimensions and CRC results. Extracted
  stock-main.uImage and stock-diags.uImage are private host files; extraction
  does not prove that diagnostics or restoration works.

No initramfs, final Alpine rootfs, combined zImage+DTB, legacy uImage wrapper,
flash commands or installer is supplied at this stage. A diagnostic kernel
needs an initramfs with PID 1 and an observable USB userspace channel before
it could serve as a useful first-boot experiment. USB ACM from this Linux
kernel would exist only after Linux starts; it is not bootloader recovery.

## Installed-loader contract

Actual captured stock uImages use load/entry 0x80008000. Source configuration
loads an eMMC image into RAM at 0x80800000 and reads a fixed 14 MiB region;
main image location is 0x41000 and diagnostics image location 0xe41000.
These are evidence for bounds review, not authorized write addresses.

Candidate future handoff: zImage with a PW3 DTB appended, wrapped as a legacy
ARM Linux uImage with the audited load/entry. Verify decompression memory
layout, combined size, initramfs placement and command-line behavior against
the installed loader before constructing a device trial bundle. Bootloader
ATAGs include vendor data which the generic converter does not translate.
Diagnostic CMDLINE_FORCE prevents inherited stock root= from selecting Amazon
rootfs; the intended eventual Alpine command line must be designed separately.

## Gates before an on-device trial

1. Establish an allowed entry method that launches the experimental RAM image
   and returns to stock boot if it fails, or demonstrate independent USB-only
   recovery. Existing Fastboot boot invokes stored bootcmd; it is not a RAM
   launcher. Downloading an image and issuing boot does not satisfy this gate.
   Persistent bootmode changes, kernel writes and diagnostics selection remain
   deferred under the accepted recovery requirement.
2. Confirm board power, USB peripheral path, clocks and watchdog handoff; the
   minimal DTB and successful compilation cannot establish those facts.
3. Add a small initramfs and validate ARM userspace, USB observation and failure
   behavior. The first trial should keep storage/display untouched and record
   host USB events plus a clear PID-1 success message. No blind repeat reboots.
4. Only after the first diagnostic boot, port board peripherals and validate
   EPDC voltage/waveform integration before adding Alpine product services.

For the target product, MAX77696 and CYTTSP4 still need modern driver ports;
EPDC still needs PW3 timing, voltage/algorithm metadata and mode handling.
Reading, Wi-Fi/Tailscale, touch, suspend and power consumption remain untested.
Public-source access is resolved; the remaining work is hardware integration
and a recoverable launch mechanism.

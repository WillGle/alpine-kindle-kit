---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# Linux replacement investigation — 2026-10-29

User authorized deep inspection toward an independent lightweight Linux system.
This supersedes the old application-only direction for this investigation, but
is not an instruction to flash, repartition, remove the jailbreak, or reset.
Existing appliance files are unchanged.

## Evidence collected

Host USB storage is present at /run/media/will/Kindle. This agent sees a read-only
mount and no /dev/sda device node. lsusb cannot initialize libusb. These are
access limitations, not evidence of a broken Kindle. No live device shell is
available to this session. Existing KMC root execution worked in previous probes.

boot-evidence.txt contains selected historical boot-log lines read from the
currently mounted Kindle. These establish historical, not live, hardware state:

- Freescale i.MX6 SoloLite Wario, ARMv7, 512 MB RAM.
- Linux 3.0.35-lab126; root=/dev/mmcblk0p1.
- eMMC 3.64 GiB, p1–p4, boot0 and boot1 each 4 MiB.
- Kernel console configured as ttymxc0,115200. This does not establish accessible
  UART pads, electrical voltage, bootloader interaction or recovery capability.
- mxc_epdc_eink declares a proprietary module license; mxc_epdc_fb display module.
- cyttsp4 touch controller probe succeeded in historical boots.

Replacing the OS independently of Amazon services is distinct from eliminating
all vendor-specific kernel code, firmware and panel calibration. The latter is
not demonstrated. No compatible replacement image has been verified. External
research was unavailable in this session (web tool error and host DNS failure).

## One device-side collection

The probe uses the existing root helper, without rerunning WinterBreak2. It
creates only a timestamped report on user storage. It reads kernel configuration,
block geometry, MBR table entries, bounded bootloader version strings, hardware
bindings, input/display/power metadata, firmware/module inventories and hashes,
init jobs and process paths, runtime libraries, CPU capabilities and recovery
command paths. It does not execute recovery commands, query hardware registers,
change services, mount partitions, flash, reboot or collect account credentials.

From a normal host terminal with Kindle connected:

```sh
cp -n /home/will/dev/kindle/ours/investigation/gleworks-linux-probe.sh /run/media/will/Kindle/documents/gleworks-linux-probe.sh
cmp /home/will/dev/kindle/ours/investigation/gleworks-linux-probe.sh /run/media/will/Kindle/documents/gleworks-linux-probe.sh
```

Proceed only if cmp succeeds; an existing different file is not overwritten.
Safely eject USB. Open **GleWorks Linux Hardware Probe** from the Kindle library,
using the existing scriptlet integration. If the library does not expose it,
open the existing GleWorks Terminal and run:

```sh
sh /mnt/us/documents/gleworks-linux-probe.sh
```

Wait for completion/return before reconnecting USB. The report is at the USB
root: gleworks-linux-probe-*.txt. Keep it local: it contains hardware and process
metadata, even though credentials and device serials are deliberately excluded.
Do not run the probe on the host: /proc and /sys must be those of the Kindle.

## Evidence still required after collection

One passive collection cannot prove secure-boot status, bootloader unlock,
working recovery, or successful custom kernel boot. Tool paths and version
strings establish presence only. Missing config/paths are reported, not bypassed.
The script reads only the first 1 MiB of each boot area for version strings;
a missing match is inconclusive. Kernel module hashes are not stock-integrity proof.

Before any OS replacement, separately establish a model-specific boot/recovery
procedure, preserve the full eMMC user area and boot areas off-device with hashes,
identify panel waveform/calibration locations, and determine how to obtain a
consistent backup while mutable filesystems are quiescent. USB book backup is
insufficient. Do not write a whole-device image into its own user partition.
Hardware trials must establish display, touch, storage, Wi-Fi, charging, suspend,
wake, and recovery before retiring the working OS.

## Validation

Host `sh -n` passed for the 142-line probe. No maintained tests were added.
The new probe has not yet run on Kindle; compatibility and report completeness
remain pending that single user-run collection. No device files were changed by
this agent session.

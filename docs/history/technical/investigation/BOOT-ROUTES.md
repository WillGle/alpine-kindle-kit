---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

> Current direction: keep installed U-Boot, replace Amazon OS. Bootloader
> replacement is deferred; earlier candidate choices and access blockers below
> describe the investigation at that point. See the final decision section.

# Custom boot research — 2026-10-29

User authorized bypass/custom-boot investigation to replace Amazon OS on their
own Kindle. Root access is already demonstrated. No flash operation performed.

## Primary documentation verified through web access

https://barebox.org/doc/latest/boards/imx/amazon-kindle-6-7.html

Barebox 2026.10 documentation explicitly lists PW3/Muscat WFO, i.MX6SL and
MAX77696A, matching the device probe. It documents a kindle-gen-6-7 build
configuration, RAM chainloading from an accessible U-Boot console, and hardware
USB recovery. This establishes a concrete candidate, not successful operation on
this unit. Standard kexec being absent does not preclude this bootloader route.

The documentation describes a special Micro-USB serial cable; that requires
physical wiring and verification of electrical levels, not a normal USB cable.
The alternative ROM USB path may require opening the device and temporarily
disabling eMMC. These are manual hardware tasks, not remotely executable here.
Do not substitute bootloader flashing for proving a recoverable boot path.

## Historical candidate — Ponder excluded from active implementation

https://forge.solarcene.community/smallsolar/Ponder_installation

Search-indexed project README describes stock U-Boot -> u-boot-wario -> Linux
6.15 -> Alpine on p2, and explicitly starts from WinterBreak2. This directly
matches the desired replacement direction. It reports PW3 display/touch/power
work, but still lists Wi-Fi improvements, power saving and reader integration as
unfinished. These are project claims, not our hardware acceptance.

Direct repository access redirected to a login page. Search-indexed content is
not sufficient to audit or execute the installer. README says second stage
permanently writes boot/kernel/DTB; do not execute a fetched moving-branch script.
The p2 role and /usr/share/nh mapping must be verified on this unit before any
installation; its apparent 64 MiB size alone is insufficient.

## Current boundary

Agent has read-only USB storage access, no interactive device shell or raw host
block-device access. Root scriptlets can collect device-side data with user-run
execution. Full backup, known recovery, pinned/audited source and verified boot
artifacts remain required before replacement. No instruction to remove Amazon
software should be implemented as deletion before a replacement boots.

## Updated user scope

The user accepts replacing Amazon OS and discarding on-device books/data, which
are already backed up externally. Target: independently booted lightweight Linux
for tinkering and e-ink reading. Preservation of Amazon OS and a full user-data
backup are no longer prerequisites imposed by this investigation. Preserve only
hardware-specific boot identity/calibration/firmware needed to keep the unit
functional, and establish a viable boot/recovery route. Do not represent a chroot
as achieving this objective.

Earlier investigation prioritized Ponder; that choice is superseded. The user
has no Solarcene/OpenID account and reports a subscription requirement. Ponder
installers, images and inaccessible PW3 patches are excluded from the active
implementation. Retain this section only as historical research. The current
route keeps installed U-Boot and builds an independent Alpine system from
publicly obtainable sources. Reader, display, touch, networking and power
behavior still require hardware validation.

Historical access result: forge access redirected to login and installer
source was not retrieved or audited. Do not ask the user to register, subscribe
or clone this repository as a prerequisite. Lack of Ponder access is not an
active task; obtaining or implementing compatible public PW3 kernel support
and establishing USB recovery remain unresolved technical requirements.

## Current decision and deferred bootloader work

Confirmed with the user on 2026-10-29: retain the existing bootloader and replace
the OS with independent Linux for tinkering and reading. Root/application
installation must be under the user's control without a runtime jailbreak or
Amazon service dependency. Ordinary USB only remains the hardware constraint.
Keeping vendor boot code is acceptable; application compatibility still depends
on ARMv7, RAM, drivers and the chosen Linux userspace.

Bootloader replacement is a future option, not an active task. Revisit only if
evidence shows the existing loader prevents the required kernel/image layout,
necessary boot features, or a workable recovery/update scheme. Before expanding
scope, document the specific limitation and compare less invasive solutions.
Do not infer permission for serial, opening the device or fuse changes; the
current user constraint excludes them. No guarantee against bricking follows
from retaining the bootloader.

Retained evidence and artifacts:

- [Barebox source review](../linux/BAREBOX-REVIEW.md): tag v2026.10.0, commit
  5c46c205c8d1a7c57b9dbaa5e516758a6c0a4d80. PW3 requires the kindle7 512 MiB
  image, not the kindle6 example. Stale bootm -c fallback and fuse-writing
  configuration need attention before any future build/deployment.
- [Stock-family U-Boot review](../linux/UBOOT-REVIEW.md): source commit
  4b6c52358630564562224b780bd75a5659fbc84b.
- [Actual boot-image review](../linux/BOOT-CAPTURE-REVIEW.md): two matching
  captures; actual BIST disassembly corroborates Fastboot boot limitations.
- Local source trees: ours/linux/barebox-2026.10.0 and ours/linux/uboot-lab126.
- Private captures: ours/linux/private-captures/. Preserve locally, do not
  publish. They are partial boot-area copies, not complete recovery images
  and do not include panel SPI data.

Any future bootloader replacement needs the exact board/image and RAM match,
validated preservation of board identity/panel data, an available recovery
method compatible with then-current user constraints, and actual hardware
verification. No bootloader replacement or OS replacement has been performed.

## Accepted device role

The replacement OS targets a low-power e-ink edge device with offline reading
as its primary role and an outbound SSH terminal to the user's server. Minimal
Linux plus KOReader and an SSH client is the accepted direction. The active
[implementation plan](../../BACKLOG.md#history-007)
records product requirements and unverified hardware acceptance. Bootloader
replacement remains deferred.

## Network management clarification

Alpine is the selected OS. User allows ordinary USB cables and additionally
requires wireless administration over Wi-Fi plus Tailscale, including when no
cable is available or the cable is charge-only. SSH client/server and private
HTTP document transfer are in scope. This does not expand authorization to UART
or opening the device, and wireless management is not recovery from a failed
boot. The active plan records the current networking and power requirements.

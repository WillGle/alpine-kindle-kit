---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# Actual PW3 boot-image review — 2026-10-29

## Capture integrity

Two captures, 17:03:54 and 17:04:11 local, reached COMPLETE. Each contains boot0
4 MiB, boot1 4 MiB, the 32 MiB pre-p1 region and a 10160-byte idme ELF. All four
files passed their supplied SHA256 checks and local-copy readback hashing. The
two independent captures have identical hashes for all four files. Private copies
and manifests are under private-captures/, directories 0700 and raw files 0600.
Do not publish these files: board identity/provisioning data are included.

## Observed binaries

- boot0 offset 0x15020: production U-Boot version string dated Aug 01 2023
  12:41:23; offset 0x3b518: BIST string dated 12:41:47, both 2009.08-lab126.
- Production environment contains bootcmd_fastboot=bist fastboot; BIST contains
  bootcmd_fastboot=fastboot. This agrees with reviewed source configuration.
- Boot selectors at source-described userdata offsets contain main and normal.
- boot1 is uniform-byte data; no independent recovery program demonstrated there.
- Kernel uImage at user-area offset 0x41000: 3828980-byte payload, Linux
  3.0.35-lab126, load/entry 0x80008000. Header and payload CRC32 both valid.
- Second uImage at 0xe41000: 2634968-byte payload, same version/load/entry;
  header and payload CRC32 both valid. Location agrees with the source's diags
  kernel region, but this does not prove a working diagnostics rootfs/recovery.
- Captured userspace idme is ARM EABI5, soft-float ELF. Embedded usage advertises
  --bootmode; it has not been executed by this analysis.

CRC and duplicate hashes establish consistency, not authenticity or bootability.
This is a partial boot-area preservation, not a full system or panel-SPI backup.

## Fastboot boot handler: actual binary corroboration

For static analysis only, wrapped the source-described 256 KiB BIST region
(boot0 offset 0x1e000) in a local ELF with virtual base 0x80400000. No instructions
were executed. Disassembled with installed LLVM 21.1.7 llvm-objdump, ARM target.
Derived ELF/disassembly are retained locally beside the private capture.

At 0x8041c3c4–0x8041c41c the command dispatch compares four bytes with the boot
string, sends a response, loads the bootcmd string through literal 0x8041c608,
and calls the environment lookup followed by command execution. Its sequence
matches the reviewed source's run_command(getenv("bootcmd"), 0). There is no
handoff of the downloaded payload pointer in this branch. Thus the source-level
warning about Android-style RAM boot is corroborated by the actual BIST bytes,
not just a matching version string. Function names are inferred from source and
call structure; the original binary is stripped.

At 0x8041c420–0x8041c448, continue sets an exit flag after replying; the adjacent
reboot handler calls reset. Neither observed branch restores persistent bootmode.
No on-device Fastboot session has been attempted.

## Decision under USB-only/no-brick constraint

There is stronger evidence for software-selected Fastboot, but no proven
nonpersistent custom-image boot path. Merely entering Fastboot is not progress
toward a safe RAM boot and can remove the currently available scriptlet access.
Do not set bootmode or flash a kernel merely to discover the return path.

Replacing the stored kernel/bootloader remains a possible research direction,
but it would be persistent mutation with recovery not demonstrated through the
allowed ordinary USB cable. Barebox's documented hardware entry routes remain
outside user scope. No claim that independent Linux is impossible follows;
no claim that current evidence makes the requested transition safe follows.

No further routine hardware inventory is needed for this conclusion. The next
useful work is an explicitly verified software boot/recovery mechanism or kernel
port development; repeating inventories does not remove this blocker.

All analysis was host-side. No bootmode, firmware, partition or device setting
changed. No flashable replacement OS has been built or tested.

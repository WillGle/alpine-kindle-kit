---
status: historical
date: 2026-11-02
superseded_by: ../../../../status/CURRENT.md
---

# Consolidated normal-kexec staging experiment

Status: one consolidated device staging batch passed and was reviewed; a
subsequent launch stopped at the free-RAM preflight before loading the module. User authorized
consolidating preparation and one device attempt where feasible. This batch
stages the real diagnostic zImage plus appended DTB, checks stored data, and
unloads everything. It cannot execute the image. No image is written to its
logical destination unless that page was first obtained from the allocator.

The original reference module is not loaded. stock-normal-core.inc retains
only normal allocation/staging/free functions from the exact-label Amazon
kernel/kernel/kexec.c. stock-normal-core.diff records all changes. Crash-kernel
direct writes, the syscall entry, vmcore/crash setup, relocation assembly and
machine handoff code are omitted. machine_kexec_cleanup is empty, matching
the stock ARM cleanup. No machine_kexec, kernel_kexec, restart/shutdown or reset
entry is linked. CONFIG_KEXEC is defined only for this module compilation;
the installed kernel and captured configuration are not changed.

Corrections and bounds applied to the isolated copy:

- kimage_normal_alloc no longer frees an image already returned to its caller
  on control/swap allocation failure. Caller now performs complete cleanup.
- A source page is freed if adding its indirection entry fails.
- Metadata is validated from a single kernel snapshot; the core does not copy
  destination metadata again from mutable user memory.
- One segment, fixed logical destination/entry0x80008000, flags0, payload at
  most2MiB with exact page-rounded memory size. No crash/preserve mode.
- Order-zero low-memory page allocations only, NOWAIT/NOWARN, at most1024
  owned pages (4MiB). Controlling structure allocation is also NOWAIT. No
  reserve access or reclaim/IO wait is requested by these allocations.
- CAP_SYS_BOOT required on open and every ioctl; exclusive open, mode0600,
  module-owner reference, mutex protection, cleanup on final fd close and
  module exit. Page-budget fault controls affect this module only.

No symbol-address resolver, hooks, patched vermagic or fake Module.symvers.
All21 imports have captured runtime exports. Missing Module.symvers warning is
retained; import checking was performed independently. Stock module descriptor
size292, vermagic and init/exit offsets188/284 match. Emitted relocation types
2/28/43/44 are supported by the stock loader. The static ARM EABI helper has
no dynamic interpreter, dynamic segment, libc dependency or undefined symbol.
Its raw syscall numbers were checked against stock ARM headers.

## One device batch

Copy bundle/* into /mnt/us/documents/, eject safely, launch **GleWorks Kernel
Load Batch**, and reconnect afterwards. The launcher script must be directly
in documents/. It copies the helper to a private mode0700 directory in /tmp
to avoid reliance on executing binaries from the FAT USB volume. Device node
is private mode0600; the helper opens it without invoking the stock reboot
syscall or a system kexec tool.

Preflight: exact module/helper/image hashes, captured stock config hash,
kernel3.0.35-lab126, modules enabled, baseline taint1, 32MiB free document
storage and12MiB currently free RAM. A no-argument helper smoke run occurs
before insmod. Any failure stops without a forced retry. RAM availability
can change after the snapshot; this threshold is not a safety guarantee.

The one helper session checks:

- Exclusive open and rejection of an unknown execute command.
- Invalid request pointer, flags/crash request, multiple segments, misaligned
  destination, overflowing destination and oversized image.
- Bad payload pointer with cleanup, then eight local allocation-budget
  failures0..7 including failures after several source pages are staged.
- Denial on an inherited fd after a child drops root with setuid65534.
- Stage/read back/unload prefixes4096,65536,262144,1048576 bytes and the full
  1833449-byte kernel+DTB input. Compare padded FNV1a checksums in the client;
  all expected values are independently recorded in host-review.json.
- Second load rejection, zero owned-page accounting after each unload,
  empty-state verification, and automatic cleanup on final fd close/reopen.

The script captures private module/meminfo/taint/kernel snapshots and helper
results, normally removes the module, and hashes evidence into
/mnt/us/gleworks-kexec-load-batch-*. Cleanup is armed before insertion and
checks actual module presence, including the successful-insmod signal window.
No force-load/unload, storage image write, boot selector change or reboot.
Failure logs may include expected "Could not allocate" messages from the
deliberate local budget cases; review them with helper results. Raw captures
can contain private kernel addresses and must not be published.

## Evidence limits and build

The new module accesses struct page through the exact-label public headers.
Prior probes do not establish every field's runtime ABI. GCC4.9.4 differs from
stock4.9.1; public source lacks the captured REAGL option; PGP authentication
of acquired toolchain/dependency inputs remains unverified. These limits are
retained in host-review.json. A kernel ABI mismatch can still crash/hang the
device; shell cleanup/logging cannot rescue a frozen kernel.

The input is the previously built diagnostic Linux6.18 zImage concatenated
with its diagnostic DTB. It lacks an initramfs and is not a complete Alpine
boot bundle. Staging success does not establish a valid handoff, boot memory
placement, shutdown/cache/MMU/watchdog sequence, working USB/power, hardware
drivers or ordinary-USB recovery. No handoff will be enabled by this batch.
The accepted USB-only recovery gate remains unresolved.

build-host.sh rebuilds module/helper using the prior ABI-probe headers and
private libraries. The bundle and its generated launcher are a reviewed
snapshot: after rebuilding, regenerate hashes/template substitution and redo
host checks before copying. The source/core diff, build logs, host-review.json
and bundle manifest preserve this snapshot. No maintained host tests added.
No host ARM emulator was available; helper startup remains a device preflight.

## Consolidated device staging result verified: 2026-10-29

User reconnected; found two capture directories, not two executed batches.
First `gleworks-kexec-load-batch-20261029-203458-24145` completed: all16
manifest entries pass, all19 files copied byte-identically into private-
captures/ (0700 directory/0600 files). Recorded module/helper/image hashes
match the reviewed snapshot. Static helper startup succeeded.65 PASS lines,
including privileges/exclusive open/invalid inputs/local fault budgets0..7,
five real-image staging/readback/unload checks, and final-close cleanup.

Independent expected padded FNV1a results match all five sizes:
4096:7339d2a3;65536:43949f3c;262144:9b41e798;1048576:845f71ab;
1833449:c6efd470. Owned/peak page counts4/19/67/259/451 equal the rounded
payload pages plus3 control/indirection pages. All tested failures and unloads
report zero owned pages; final module removal reports owned pages=0. Module
list before/after identical, taint unchanged1. New kernel tail25 lines contains
only batch messages plus the two expected deliberate control/swap allocation
failure messages. No unexpected line/error in retained delta. Circular buffer
lost26 old prefix lines; all remaining baseline lines unchanged.

First-batch MemFree before/before-module-unload/after:13684/13368/13244 KiB.
These whole-system counters do not isolate allocations or prove leak freedom.
Normal/failure cleanup evidence is the exercised control flow, readback,
module-owned page accounting and completed unload, not a free-RAM equality.

Second `gleworks-kexec-load-batch-20261029-203518-24662` stopped at preflight:
MemFree8888 KiB below12288 threshold; no helper or module loaded. All10 manifest
entries pass;13 files preserved privately. Before/after kernel log and module
list identical; taint1. No automatic retry or extra device attempt by agent.

See kexec-load-probe/runtime-review.json. The normal stock-derived real-image
staging/readback/release path is now observed working for this batch, including
multi-source-page failure cleanup. This is not an exhaustive struct-page ABI,
leak or future kexec safety proof. No execution command was present/invoked,
no kernel handoff/reboot/firmware/boot-selector mutation occurred through this
batch, and no Alpine boot or ordinary-USB recovery was established. Remaining
boot work includes first userspace/initramfs, watchdog/USB/shutdown/cache/MMU
validation and the accepted USB-only recovery gate. A successful staging test
does not remove that gate or justify enabling handoff automatically.

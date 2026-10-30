# PW3 bounded RAM experiment

Host-built research candidate; one normal device trial passed and was reviewed. This is a temporary
experiment under the accepted RAM load/unload research stage, not a kexec
implementation or a bootable Alpine image.

The module requests 16 separate order-zero low-memory pages, 64 KiB total,
using __get_free_page(GFP_NOWAIT | __GFP_NOWARN). This has no reclaim/IO waits,
no high-memory requests, no higher-order contiguous allocation and no access
to allocator emergency reserves. Allocation may fail even when reclaim could
make more memory available; failure returns -ENOMEM after releasing all pages
already obtained. No retry or memory-pressure escalation is attempted.

Each owned page receives its fixed byte pattern (0xa5 XOR page index). All
65536 bytes are read through volatile pointers during init and again during
exit. A mismatch returns -EIO at init after cleanup; exit logs a mismatch but
still releases every owned page. No arbitrary pointers, struct page layout
access, physical address selection, executable payload, ioctl, parameters,
device node, worker, symbol lookup or boot/reboot interface is present.

Allocation APIs return virtual addresses; this experiment does not establish
physical placement suitable for a kernel image. It does not validate shutdown,
cache/MMU teardown, watchdog handling, recovery or safe kernel handoff.

Build with `sh build-host.sh` after the existing ABI-probe header/library
preparation. Source trees and captured config remain unchanged. Uses the same
candidate GCC4.9.4; stock reports4.9.1. The generated stock config drops the
public source's absent REAGL option as documented in abi-probe/. Publisher
PGP verification remains pending. Missing Module.symvers warning is retained;
no fake symbol table, CRC or vermagic modification was used.

Host review: ARM ELF32 little-endian relocatable EABI5; vermagic and 292-byte
descriptor match stock. init/exit relocation offsets188/284 match the prior
probe and stock modules. All four imports (__get_free_pages, free_pages,
memset, printk) have exports in the captured runtime. All emitted relocation
types are handled by the public stock module loader. Disassembly shows bounded
loops, allocation order0/GFP value512, and the expected allocation/readback/free
paths. See host-review.json, build.log and module.sha256. No maintained tests
added; checks cover build/structure/source only, not runtime fault injection.

For the device experiment, copy gleworks_ram_probe.ko and
gleworks-ram-trial.sh to /mnt/us/documents/, eject safely and launch GleWorks
RAM Trial through the existing launcher. The script requires exact module and
stock config hashes, known kernel, enabled module loading and baseline taint1.
It uses normal insmod/rmmod and records private dmesg, module, meminfo and taint
snapshots in /mnt/us/gleworks-ram-trial-*. No forced loading/unloading, firmware
write, boot selector change or reboot. Reconnect for evidence review afterwards.

Kernel code can still crash/hang if assumptions are wrong. Logging cannot
recover a frozen kernel. No guarantee against bricking or proof of USB-only
recovery is implied. Free-memory counters fluctuate with other activity;
they cannot prove absence of a small leak on their own. Review control flow,
normal unload and logs together; COMPLETE means the script completed, not
that a future kexec module or Alpine boot is safe.

## Bounded RAM device trial verified: 2026-10-29

Capture `gleworks-ram-trial-20261029-201745-22555`: COMPLETE present; all12
manifest entries pass. All15 files copied byte-identically into private-
captures/ with directory0700/files0600. Module hash matches host candidate.
Normal load/unload returned. Module present only during trial; stock module
entries unchanged afterwards; taint stays1. New kernel tail contains only:

```
[16266.512458] gleworks_ram_probe: loaded; 16 pages, 65536 bytes verified
[16267.606472] gleworks_ram_probe: unloaded; 65536 bytes reverified and released
```

Circular buffer advanced: four old prefix lines replaced by a partial line;
remaining baseline lines unchanged. No new error in retained delta. MemFree
before/during/after:14444/13924/12356 KiB. Whole-system counters do not isolate
probe allocations; their decline does not establish a leak, and normal unload
does not prove absence of leaks. Source/disassembly and trial log support the
normal owned-page cleanup path; failure cleanup has not been runtime tested.
No additional trial performed. See ram-probe/runtime-review.json.

This validates one64KiB allocation/pattern/readback/release exercise. No
arbitrary payload loading, kexec placement/shutdown/cache/MMU/watchdog sequence,
recoverable kernel handoff or Alpine boot established. Trial did not mutate
boot/firmware. Next substantial task is adapting/reviewing the kexec loading
path with execute absent/disabled. Another allocation probe alone would not
establish a boot route. USB-only handoff/recovery gate remains unresolved.

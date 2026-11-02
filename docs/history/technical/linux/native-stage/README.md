---
status: historical
date: 2026-11-02
superseded_by: ../../../../status/CURRENT.md
---

# Native stage: returning preparation trial

Status: ARM module built; independent review found no must-fix defect.
Device trial passed78 checks; see runtime-review.json. Native kernel execution remains NO-GO.

This distinct module preserves the proven kexec-load-probe module and image.
The allocator records each successful low-memory page allocation, role change
and free. The bounded validator checks that registry before reading the chain.
GW_PREPARE accepts only the fixed 1,832,609-byte diagnostic image, SHA256
8d6046f8a401dd82f9dec0c70ab1209b5db782ecc674ed7dbd20b5f132813f92.
It prepares the owned control page and returns. No callback resolution,
shutdown function, execute ioctl or transition entry is linked.
Relocation bytes are copied as data, never called by this module.

SHA256 is a private subset of the exact stock crypto/sha256_generic.c with
its copyright/license retained; it does not require an unavailable stock
SHA256 crypto provider or register an algorithm. Hashing reads staged pages
under the same mutex after ownership validation. User memory is not trusted
as the post-copy identity of the image. Padding is excluded from SHA256.

The static helper retains allocation-budget faults, privilege checks,
exclusive open, size/readback and final-close cleanup cases. It additionally
checks empty/wrong-size/corrupted images, unknown prepare argument, successful
fixed-image preparation, repeated preparation and unchanged readback.
Exercised runtime behavior passed: fixed-image preparation/readback, rejection, allocation faults and cleanup. This does not prove native handoff safety.

Build/package on host:

```
sh ours/linux/native-stage/build-host.sh
python3 ours/linux/native-stage/package.py
```

Copy all five files plus SHA256SUMS from bundle into Kindle documents,
then safely eject and launch GleWorks Native Stage once from the existing
script launcher. Do not launch a different GleWorks trial concurrently.
The package keeps the 12 MiB free-RAM guard; if unavailable it declines
without loading a module. It does not kill processes or lower that guard.
Reconnect USB after completion and review /mnt/us/gleworks-native-stage-*.
This is a preparation trial, not a boot attempt or reset trial.

The launcher reads driver/resource associations to help finish the DMA audit;
it does not unbind a controller, write MMIO or open the watchdog. It writes
only input copies/temporary device node/logs and temporarily loads this module.
Cleanup closes the image and removes the module normally; no forced removal.
Its retained cleanup chain walker assumes internally constructed chain memory
is intact; it is not a corruption-recovery mechanism for arbitrary kernel bugs.

Remaining boot blockers are concrete: USB gadget, SDHCI, SDMA and PXP stock
platform drivers lack shutdown callbacks. PXP DMA_TERMINATE_ALL only moves
software lists and does not establish a hardware-idle fence; PXP suspend
waits unbounded for ENABLE clear and must not be called blindly. The active
USB resources also include EHCI/OTG alongside the gadget. Each DMA producer
needs a bounded, source-supported stop and a safe dependency order.
The observed stock restart does not prove watchdog recovery after shutdown
or after the new kernel changes clocks. No flash/bootloader/selector changes.

# Compile-only native ARM handoff candidate

Status: ARM objects and180-byte physical relocation blob built and reviewed.
No loadable transition module, execute ioctl or device launcher exists here.
Proven load-only module and minimal kernel/image remain unchanged.

Captured stock binary confirms cpu_v7_reset0xc021cfc0 only MOV pc,r0;
cpu_v7_proc_fin clears I/C/A while leaving MMU M enabled. This differs from
the assumptions of a generic transition. `captured-callbacks.json` records
exact captured code fingerprints for9 relevant callbacks, not live calls.

`transition.c` prepares an owned control page with list/entry/DTB fields and
contains the prospective no-return sequence. It follows stock restart/device
shutdown, identity-map, inner/outer-cache teardown order. Physical control
address is preserved across callbacks in callee-saved r4 in the compiled C.
The copied position-independent assembly clears SCTLR M/C/I while instruction
VA=PA, invalidates translation/instruction/predictor state, copies pages,
passes r0=0,r1=~0,r2=DTB and jumps. No stack/calls/external relocation in blob.

`chain.c` separately validates the entire bounded chain against an internal
owned-page registry, exact flags, single destination, source uniqueness,
termination, cycles and control/list/source overlap. Self-copy is allowed;
cross-destination source is rejected. Its registry must come from actual
allocator bookkeeping, not user input or inference from the chain itself.
It is not yet wired into the existing staging allocator or transition.

Host checks:

```
sh ours/linux/native-handoff/build-host.sh
python3 ours/linux/native-handoff/verify-host.py
```

Build emits transition.o, chain.o, relocate.o/bin and disassembly only.
Blob parameters at0xa8/0xac/0xb0; blob no linker relocations. Ordinary object
imports match captured exports. ARM module phys/virt `.pv_table` patching
must be preserved when integrating; standalone objects are not runnable.

Disposable actual-blob interpreter checked10 size/indirection combinations
through448 pages, boot registers and MMU/cache bit prefix. It implements
only the used instruction subset and is not an ARM hardware emulator. A
source-inclusion host mock checked preparation guards. Chain source-inclusion
ASan/UBSan probe covers valid pages/indirection/self-copy and invalid/unowned/
duplicate/cyclic/unterminated/overlap input; LSan disabled because host ptrace
prevents its operation. No maintained tests added and no hardware boot claim.

Independent review found no definite instruction-level must-fix; integration
still requires registry/chain/image binding, live callback identity/outer-cache
validation, safe callback lifetime, decompressor workspace/reserved-memory
bounds and a complete device/DMA shutdown audit.

Source findings requiring care: stock USB arcotg_udc platform driver and
sdhci-pltfm driver have no shutdown callback in their registration structs.
Thus kernel_restart_prepare/device_shutdown alone does not prove those DMA
engines are stopped. EPDC shutdown blanks/powers down; watchdog shutdown stops
feeding and changes deadline to128s. Reboot notifier boot_globals writes a
warm-restart marker in RAM. None of these paths is invoked by this host work.
Do not solve unknown DMA state with blind controller-register writes.

Actual Image is4016156bytes; kernel BSS end0x80408b00. It deliberately overlaps
old kernel text after the no-return jump. The decompressor relocates itself
and appended DTB, with its own stack/malloc needs; compressed-image staging
does not validate this layout. `memory-review.json` records facts and unknowns.

User permits discarding Amazon OS, retaining installed bootloader. Required
recovery is independent access, not preserving Amazon as the final system.
Actual stock restart/return has been observed, but timeout cause and fallback
after device shutdown/new kernel remain unqualified. No boot attempt here.

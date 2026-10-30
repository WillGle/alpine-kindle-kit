# Compile-only handoff candidate review

Review transition.c/.h, relocate.S, build-host.sh and generated disassembly.
No git at workspace root. No .ko, user ioctl, callback resolver or launcher
links this code. Requirements: prepare identity-mapped owned control page;
validate ranges, then eventually shut down devices/caches/IRQ/FIQ and jump
physical stub. Stub must turn MMU off, relocate validated source chain, pass
r0=0,r1=~0,r2=appendedDTB, no stack after MMU transition.

Critical discovery: captured stock cpu_v7_reset at0xc021cfc0 = MOV pc,r0 only;
proc_fin clears I/C/A, leaves M on. Exact flash bytes and kallsyms checked;
captured-callbacks.json contains9 fingerprints. Source stock machine_kexec
must not be used verbatim. setup_mm_for_reboot idmaps0..TASK_SIZE=3GiB.
PW3 RAM0x80000000..0xa0000000 falls in that range.

Safety: old load-only module unchanged. Candidate not eligible for on-device
execution. Integration still needs full relocation-chain validation, callback
identity resolver, DMA/device shutdown + watchdog effect audit, fixed image
hash and final USB observer/return procedure. Do not confuse preparation
success with a complete executable handoff.

Focus: actual compiled ARM instructions, high physical addr identity mapping,
cache/MMU ordering and page alias conflicts, DTB envelope/alignment, any
pointer lifetime across irreversible restart_prepare and source-vs-binary
differences. Report must-fix separately from pending integration gates.

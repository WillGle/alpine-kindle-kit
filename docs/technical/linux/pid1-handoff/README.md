# RAM-only Alpine PID1 handoff

This experiment replaces global PID1 with pinned Alpine3.24.3 BusyBox/musl
inside a confirmed tmpfs root while keeping stock3.0.35, IRQs and drivers.
Existing Amazon processes keep Wi-Fi/SSH alive during the short trial. This
is a native userspace initialization experiment, not clean independent cold
boot or an installed Amazon OS replacement. It does not qualify modern
Tailscale, kernel handoff or deleting the existing rootfs.

`build-host.sh` builds freestanding ARM helpers and an exact-ABI guardian module.
No device operation is performed by that build. All24 module imports were
matched to exported symbols in the captured running kernel. No ABI overrides,
forced loading/removal, boot-selector writes or guessed register writes.

The tracer saves/restores8bytes of code and512bytes of temporary stack for
failed exec. Valid exec is recognized through PTRACE_EVENT_EXEC. Injection PC
is distinct from the stopped syscall continuation to avoid stock ARM
restartblock rewriting. Old-kernel wait4 requires explicit __WALL for PID1.
Ordinary termination signals are blocked before PID1 attachment; SIGKILL cannot
be blocked. Four child cases (TRACEME/ATTACH × valid/ENOENT) passed on hardware.

Authorization for transfer requires a live token-bound kernel ioctl response,
state2 and at least60seconds remaining; marker files cannot authorize it.
Bootstrap also checks the root's fresh trial nonce before chroot and init exec.
The independent kernel thread has a90s deadline, pins its module while armed,
and invokes the exact stock emergency_restart path in process context. TEST
waits2s and returns without resetting. CANCEL releases the pin outside real
firing. State/deadline snapshots and wake checks handle cancel/re-arm.
The reset callback can still block on PMIC/I2C; returning tests do not prove
hardware reset. This mechanism applies only to the unchanged stock kernel.

No automatic execute/installer launcher is supplied. Host monitoring records
inputs and challenge before arming; streams proof to host before RAM is lost.
The first PID1-stop attempt failed before editing memory because __WALL was
missing; guardian cancelled and Upstart rechecked intact. The corrected trial
produced a fresh parent1 sysinit proof, actual /proc/1/exe matching pinned Alpine
BusyBox, /proc/1/root in /dev/shm, and stock kernel identity. See
`trial-request.json`, `trial-runtime.log`, `pid1-live-proof.log`, `pid1-proof.json`.
These evidence files record the actual recovery result when observed.

Persistent cutover still requires clean independent services, a cold-boot
recipe and ordinary-USB rescue independent of the Amazon rootfs being replaced.

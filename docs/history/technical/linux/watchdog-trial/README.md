---
status: historical
date: 2026-11-02
superseded_by: ../../../../status/CURRENT.md
---

# Bounded stock-watchdog qualification

Status: the sleeping-wait version ran on Kindle and restored through the
150-second deadline, without resetting. The runnable-wait candidate is
host-built, not yet device-tested. This is a reset/recovery trial, not native
boot. No bootloader, partition, firmware,
watchdog timeout or SRC-selector writes. A timeout may restart the stock OS.

Stock imx2_wdt.c open removes its kernel feeding timer; NOWAYOUT close pings
once without restoring that timer. This module queues recovery before opening
the real watchdog and owns that file until restoration. It closes the file,
then calls stock imx2_wdt_reinit(void) in process/workqueue context to restart
feeding. That nonexported function is resolved using exported kallsyms lookup
and must match the exact captured firmware address 0xc04542c8. This is a new
runtime dependency, not qualified by prior inert/staging module success.

The module has no caller-supplied address, payload, timeout or execute operation.
Root CAP_SYS_BOOT and an exclusive guard lease are required. At most two arms
per module load: an immediate abort callback check, then a timeout trial.
The callback check proves returned state only; periodic feeding is unobserved.
Final lease close or normal unload restores feeding. A 150-second delayed
worker also restores it if the stock scheduler/workqueue still runs. That
deadline is conditional; no absolute no-brick or hardware recovery guarantee.
No device shutdown or CPU-handoff operation is performed.

Expected watchdog timeout is 60 seconds. Its first pretimeout IRQ feeds it once,
so reset can take about 118 seconds. Stock idle/bus-frequency paths may also
feed it and STOP suspends it. The script temporarily prevents screen saver,
restoring the prior zero value on exit. The candidate keeps the helper
runnable with a bounded CPU loop to avoid CPU idle, using monotonic elapsed
time and polling guard state each second. It preserves normal scheduling,
interrupts, clock settings and kernel idle code. The client exits by160s while
stock scheduling and the monotonic clock function; the unchanged kernel guard
restores by its150s workqueue deadline while that workqueue runs. No reset is an
inconclusive result; the program then reports restoration and unloads normally.

Launch requires the captured kernel/config/taint, exact artifact hashes,
root ro, battery at least 40%, 4MiB free RAM, expected watchdog device,
WCR0x773d/WICR0x8004 and zero GPR9/GPR10. It syncs private pre-reset evidence
and an intention marker before arming. The marker does not prove arming.
No script auto-run or persistent startup hook is installed.

Copy bundle/* into the mounted Kindle documents directory, eject and select
**GleWorks Watchdog Trial**. Wait up to three minutes without manually rebooting.
If Kindle restarts, select **GleWorks Watchdog Trial** once more before
reconnecting. With the pending marker present, that launch only collects
post-reset boot ID/registers/logs and never arms another trial. If it does not
restart, reconnect after three minutes; the no-reset logs need review.

A changed boot ID alone is insufficient. Review pre/post hardware WRSR, prior
intent, helper log and absence of a user reboot. Linux6.18 imx2_wdt.c identifies
WRSR bit1 as timeout; the observed initial0x10 has that bit clear. Bootloader
handling may obscure cause, so missing cause evidence remains unproven.

Host checks: stock ABI build with normal imports, all imports in captured
exports, fixed captured callback symbol, shell/Python syntax, four bundle
hashes. Disposable source-inclusion simulation exercised permissions, queue/
open failure, explicit restore, lease close, deadline restore, normal unload
and attempt cap. It mocks driver/scheduling and proves no hardware behavior.
No maintained tests added. Prior compiler/source/signature limitations remain.

Independent review found comparison errors could be mislabeled as changed
boot ID; fixed by prior-file validation and handling cmp statuses 0/1/error.
Immediate callback check wording was narrowed to match observed evidence.

## Watchdog trial capture — 2026-10-29 21:20

`gleworks-watchdog-trial-20261029-212005-29569`: 13 files privately copied,
11/11 manifest hashes pass. Boot ID matches the21:01 readiness capture;
modules and dmesg byte-identical before/after. Registers remain WCR0x773d,
WRSR0x10, WICR0x8004, GPR9=GPR10=0. MemFree14416KiB and stock ext3 rootro.
No module/helper/intent evidence; the script stopped in preflight before
loading the guard. Neither watchdog reset nor native boot was attempted.
The exact failed condition cannot be inferred from the silent guard chain.

Added stage labels, bounded preflight shell tracing and readonly diagnostics
for modules_disabled, /proc/misc, watchdog node type/number and powerd battery/
screen-saver properties before those guards. No protection relaxed, no guard
module/helper change. Repackaged; both shell syntax checks and4/4 bundle hashes
pass. A further launcher run is needed to identify the actual failed check;
agent has mounted storage only and has not written to the device.

## Watchdog node mismatch confirmed and corrected — 2026-10-29 21:23

Capture `gleworks-watchdog-trial-20261029-212337-30206` stops at
watchdog-node-type: /dev/watchdog and /dev/watchdog0 absent; actual node is
/dev/misc/watchdog, character10:130. Battery100, preventScreenSaver0.
Modules unchanged, boot ID still matches readiness. Dmesg ring-buffer prefix
advanced with USB mass-storage disconnect lines appended; stock segfault
messages already existed before this run. No guard module loaded, no watchdog
reset attempt and no native kernel executed. Capture:16 private files and
14/14 manifest entries verified; watchdog-trial/runtime-review.json records it.

Corrected only watchdog node paths in module open and launcher type/number
checks/diagnostics. No new node/symlink created and no guard relaxed. Stock
ABI module rebuild/import checks pass; source-inclusion control-flow and
collector simulations pass (host mocks only), shell syntax and4/4 bundle
hashes pass. Binary open path and script guards checked against observed
node. Requires copying the full bundle because module hash also changed.
Actual watchdog reset recovery and native boot remain unproven.

## Watchdog ran without reset; bounded runnable candidate — 2026-10-29 21:25

Capture `gleworks-watchdog-trial-20261029-212558-30606`:21 private files;
SHA256SUMS18/18 and PRE-RESET4/4 verified. All preflight guards passed;
watchdog c10:130 opened. First arm/restore callback returned; second arm
20360.081781, restoration20510.568925:150.487144s. Helper exit3 NO RESET.
Module list restored to baseline, boot ID unchanged, no native execution.
Callback return/state and normal cleanup are hardware-observed; resumed
periodic timer feed and timeout reset/return remain independently unproven.

Source-supported hypothesis: low-bus idle/busfreq wdg_prep keeps feeding.
No direct trace attributes this result. Candidate changes only helper wait:
preemptible volatile CPU loop, ARM syscall263/CLOCK_MONOTONIC, poll state
once per monotonic second, client cap approximately159-160s. Clock failure
closes guard lease. No interrupt/preemption/clock/idle-code changes; the
150s guard/module and readonly reader match the device-tested input hashes.
Busfreq transitions can still feed; a no-reset outcome remains inconclusive.

Fresh independent review found no must-fix. Minor reporting limit: the old
"worker missed deadline" message is stronger than a once-per-second client
observation with a pre-arm timebase; treat it only as the client wait limit.
Disposable helper source-inclusion simulation (mock syscalls/time) passed
auto-restore, clock failure before/after arm, and160s worker-missed limit;
all paths closed the lease. Guard mock simulation, actual ARM build/import
checks, shell syntax and4/4 bundle hashes pass. Not hardware execution.
User must copy full candidate bundle and launch; native handoff is absent.

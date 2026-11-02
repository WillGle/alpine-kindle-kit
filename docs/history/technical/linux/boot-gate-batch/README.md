---
status: historical
date: 2026-11-02
superseded_by: ../../../../status/CURRENT.md
---

# Expanded boot gate batch

See [TEST-MATRIX.md](../../../BACKLOG.md#history-011) for31 checks/routes, mandatory gates and
explicit unknowns. Current native boot decision remains **NO-GO**: handoff is
unimplemented and watchdog reset/return unproven.

Host preparation:

```
python3 ours/linux/boot-gate-batch/package.py
python3 ours/linux/boot-gate-batch/audit-host.py
```

`host-audit.json` reports10 host validation groups individually. Exit0 means
those host checks passed; it is not permission or proof to execute a kernel.
The9-file bundle reuses the existing minimal image, readonly MMIO helper,
normal load/fault/ownership suite and bounded runnable watchdog candidate.

One device run:

```
cp /home/will/dev/kindle/ours/linux/boot-gate-batch/bundle/* /run/media/will/Kindle/documents/
sync
```

Eject and select **GleWorks Boot Gate Batch**. Allow three minutes. If it
restarts, select the same launcher once more for collection only, then reconnect
USB. Without a reset, reconnect after the batch returns. The new launcher
replaces separate Readiness/Watchdog launches for this run; do not launch them
concurrently. Existing child evidence remains in separate private directories,
with the wrapper's stage exit results and capture list in gleworks-boot-gate-*.

The package does not include a native execution interface, installer, autorun
hook, selector/partition writes or a bootloader replacement. Stock watchdog
open/close/reinit is active hardware behavior and may reset the device; its
150s recovery depends on stock workqueue scheduling. Wrapper-only signals
need not immediately stop the child. The runnable wait temporarily consumes
CPU; it leaves IRQs/preemption/clocks enabled and retains the battery guard.

Independent review found a collection race: invoking an active-capable child
after checking a marker could re-arm if another run removed the marker.
Fixed by adding a child `--collect-only` latch, also preserved across su.
Disposable source-inclusion mock scenarios cover stage ordering, clean stage
failure, leaked/preloaded modules, parent-only pending, collection, and marker
disappearance; they do not simulate actual hardware. No new device run has
occurred with this wrapper. Source, helper and module limitations in child
READMEs remain applicable.

Expanded host-audit review: fixed manifest/JSON membership enforcement and
stock uImage declared-payload envelope checks. Disposable RED/GREEN inputs
reproduced both missing checks and reject both after fix. Also linked actual
compressed blob in zImage -> decompressed Image -> exact generated CPIO bytes;
CPIO/init is therefore checked in the actual image, not just a sidecar.
Source revision/generated config still do not independently prove build
provenance; this limitation is explicit. Final actual host audit10/10 pass,
7 wrapper mock scenarios and collection-only latch pass;3 shell and2 Python
syntax checks pass. No new device run.

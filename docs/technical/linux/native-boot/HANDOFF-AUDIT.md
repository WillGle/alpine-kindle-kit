# Handoff and recovery audit — 2026-10-29

- **Readback obtained; reset recovery unresolved.** The 21:01 capture reads
  WCR0x773d (enabled, 60s, no WAIT disable, STOP suspend), WICR0x8004 and
  GPR9=GPR10=0. Stock config selects builtin IMX2_WDT/NOWAYOUT and its driver
  feeds through a kernel timer. No actual watchdog reset/return is proven.
- **Unsafe query pattern avoided.** imx2_wdt_open calls imx2_wdt_start, which
  deletes the kernel ping timer after initial startup. With nowayout1, close
  pings once and does not restart that timer. Merely opening /dev/watchdog to
  query GETTIMEOUT and closing can therefore cause an eventual reset. The
  supplied helper never opens this device.
- **Misleading reset-status ioctl.** Stock imx2_wdt_ioctl returns literal0
  for both GETSTATUS and GETBOOTSTATUS; these cannot prove watchdog recovery.
  Readonly WDOG1 WRSR and pre/post boot IDs are potential evidence, subject to
  bootloader/reset semantics still requiring validation.
- **Warm-reset boot source matters.** U-Boot generic.c clear_mfgmode_mem and
  do_switch_mfgmode document SRC GPR10bit28 / GPR9 boot-source override. Cleanup
  before Linux is conditional on CONFIG_CMD_IMX_DOWNLOAD_MODE. Do not assume
  the actual unit cleared it based only on this source. Read GPR9/GPR10, never
  write them just to try discovery. Stock quickboot/abnormal-reset branches
  also require care; normal boot-selector preservation is not reset proof.
- **Shutdown extends watchdog deadline.** Public imx2_wdt_shutdown deletes
  kernel feeding, sets128seconds and pings. This can provide a prospective
  fallback window, but only if the watchdog clock/counter and boot path remain
  valid. A new kernel may affect them. No timeout/recovery trial performed.
- **Native machine handoff remains unimplemented in the load-only module.**
  Stock ARM machine_kexec uses setup_mm_for_reboot, cache/outer-cache teardown,
  cpu_proc_fin and cpu_reset plus its relocation stub. It does not use the
  soft_restart required by the3.8 reference module. Non-exported functions
  cannot be treated as ordinary imports; prior staging success does not
  validate their shutdown/MMU/CPU-reset behavior. Keep execution absent.
- **USB is a second dependency.** The DTS enables the generic i.MX6SL USB
  peripheral controller; PW3 PHY/VBUS/power behavior has not been exercised
  by the new kernel. PID1 reaching USB would be positive evidence; no USB
  response cannot distinguish early kernel failure from gadget failure.

All findings are source/host evidence. No watchdog device opened, MMIO write,
timeout change, reboot, persistent boot change or new-kernel execution occurred
in this audit. Readiness batch supplies targeted current evidence only; it
does not itself qualify or exercise recovery.

## Independent review and corrections

A fresh read-only reviewer examined init, observer, MMIO reader and generated
readiness shell/artifact consistency. Two important communication bugs fixed:

- PID1 now closes/reopens ttyGS0 on EOF. Kernel gadget disconnect invokes TTY
  hangup; retaining the fd would otherwise return EOF forever after reconnect.
  Disposable host syscall simulation reproduced the stuck fd before the fix
  and recovered/answered the challenge after it.
- Observer now catches termios.error as well as OSError during tty setup.
  Disposable injected unplug/configure simulation failed before the fix and
  retried/accepted a fresh response after it.

PTY simulations also rejected a wrong nonce and accepted the matching nonce.
These are host protocol checks, not hardware boot evidence. Kernel/PID1/bundle
were rebuilt after the PID1 correction. Packaging was integrated into the build
to prevent a new kernel leaving old image/hash snapshots behind. No maintained
tests added. Remaining review limitations are actual ABI/USB/watchdog/reset
behavior and ordinary-USB recovery; none was resolved by simulation.

## Readiness hardware result — 2026-10-29 21:01

Capture `gleworks-boot-readiness-20261029-210135-26717`: 19 private files,
16/16 manifest hashes verified and byte-identical to USB evidence. WDOG1
WCR=0x773d (enabled, 60s, STOP suspend, no WAIT disable), WRSR=0x10,
WICR=0x8004. SRC GPR9=GPR10=0. No watchdog fd holders captured, root ro,
no new dmesg lines, modules unchanged, taint1. WRSR0x10 is not proof of a
watchdog timeout. New-image staging stopped at the 12MiB free-RAM guard;
no module loaded and no native kernel executed.

Next: bounded watchdog reset qualification with driver-feed restoration on
abort/deadline, before native handoff. Idle/busfreq paths also feed watchdog;
no-reset outcome must remain inconclusive. No flash/boot selector changes.

## Actual restart and stock return verified — 2026-10-29 21:51

Post captures boot-gate215114-6747 and watchdog215115-6792 privately preserved
and all manifest entries verified (see boot-gate-batch/runtime-review.json).
Collection-only branch exit0; new boot ID differs from pre-trial; uptime188.61s,
stock kernel boot log, user confirms normal home screen. No GleWorks module
remains and pending markers cleared. Actual reset/stock return confirmed.

Post WDOG1 WRSR remains0x10, timeout bit1 clear; GPR9=GPR10=0, WCR0x773d.
Therefore reset cause is not independently confirmed as watchdog timeout.
No repeat hardware test requested merely to re-confirm the visible restart.
Native execution remains absent; a stock-system reset does not validate
watchdog clocks/recovery after a new-kernel handoff. Next engineering gate
is the missing native ARM handoff and its host/source safety review, with
reset-cause semantics unresolved. User permits discarding Amazon OS while
retaining bootloader; do not interpret stock return as a required final OS.

## Native handoff adaptation: compile-only milestone — 2026-10-29

User said continue after reset/stock return observation. Used executing-plans
and independent code review; preserve Amazon-disposable/bootloader-retained
intent. No new device attempt, mounted write, boot mutation or flash.

Critical stock binary fact: cpu_v7_reset0xc021cfc0 is MOV pc,r0 only,
proc_fin0xc021cfa4 clears I/C/A but leaves MMU M. Exact captured flash bytes
match source. Generic stock kexec sequence/reference cannot be copied blindly.
Nine callback code fingerprints captured in native-handoff/captured-callbacks.json.
Prepared native-handoff/relocate.S, transition.c/h, chain.c/h as ARM objects
only. No obj-m/execute ioctl/launcher.180-byte PIC physical blob with no
linker relocations clears M/I/C at identity VA=PA then copies source pages,
passes r0=0,r1=~0,r2=appendedDTB. Header params0xa8/0xac/0xb0. C metadata guards
and whole-chain validator remain separate, not wired into staging allocator.

Disposable actual-blob subset interpreter:10 page/indirection cases through
448pages pass including MMU bits/boot args. Actual C preparation host mock:
valid plus12 invalid metadata/range cases pass. Chain actual-source ASan/UBSan
probe: valid multi/list/self-copy and malformed/unowned/duplicate/cycle/end/
overlap cases pass; LSan disabled because ptrace unsupported. These are host
simulations, not hardware/cache behavior. ARM object build, blob structural
verifier/import exports, shell/Python syntax pass. Existing staging .ko and
minimal image hashes unchanged. No maintained tests added.

Independent review: no definite instruction-level defect in blob/transition;
chain sound under trusted allocator-owned registry contract. It requires
page mapping ownership and mutation exclusion through final jump, exact
image/head/control binding, callback identity/lifetime, `.pv_table` module
fixups, decompressor workspace and device/DMA quiescence before integration.
Review found1032-byte chain stack warning; reduced1024-byte visited array to
128-byte bitmap, rebuilt and reran source sanitizer probe; current build log
has no warnings/errors. This fixes resource footprint without changing guards.

memory-review.json records compressed image1,832,609bytes including DTB,
inflated Image4,016,156bytes at0x80008000 and BSS end0x80408b00. Decompressor
self-relocates, but exact scratch/DTB bounds remain to derive. Stock code
overlap is intentional only after no-return; no live destination write.

Concrete shutdown blocker: arcotg_udc and sdhci-pltfm platform registrations
lack shutdown callbacks. device_shutdown alone does not prove USB/eMMC DMA
quiescent. EPDC shutdown blanks panel; watchdog shutdown stops kernel feed
and sets128s; boot_globals reboot notifier sets a warm marker in RAM.
These are source facts, not executed actions. Do not use blind raw-register
writes or call shutdown just to test it. No execution-ready bundle produced.
Next: integrate owned registry/chain/image checks, audit concrete DMA stop
and watchdog clocks/shutdown, bound decompressor workspace before first
native execution trial. Actual kernel staging and live6.18 USB PID1 proof
remain absent. No need to repeat the already observed stock restart.

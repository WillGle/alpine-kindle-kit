# Kexec module feasibility — 2026-10-29

Status: research route accepted; no loadable candidate approved or built.
No module was loaded and no handoff/device-state change was attempted.

## Source comparison

Reference: https://github.com/amonakov/kexec-module (README targets 3.8 ARM/x86).
Raw master files inspected via web: kernel/Makefile, Kbuild, kexec-mod.h,
kexec-drv.c, kexec.c, machine_kexec.c. No commit resolved or local checkout:
host git network access failed DNS. This is a design reference, not a pinned
build dependency yet. The module imports kexec implementation files from KDIR;
its header defines CONFIG_KEXEC for that module's compilation only.

Running config permits modules, has no symbol versioning, uses ARMv7 uniprocessor,
PREEMPT and GCC 4.9.1. KEXEC is disabled. No symbol versioning means reduced
ABI enforcement, not permission to ignore ABI matching. A matching vermagic
string alone does not prove compatible structures or machine shutdown behavior.

Against the older local 3.0.35 vendor reference:

- Reference module init unconditionally resolves soft_restart for ARM; this
  vendor source has no definition. Its native machine_kexec instead uses
  setup_mm_for_reboot, cache teardown, processor callbacks and cpu_reset.
  Dropping that symbol check is not a substitute for adapting the handoff.
- module init also resolves machine_shutdown, kernel_restart_prepare and
  memblock_is_region_memory. Runtime availability/lifetime must be verified;
  memblock code annotations can depend on configuration. kallsyms_lookup_name
  is GPL-exported in the local vendor source, not yet checked on the running
  kernel. The older source is not assumed identical to firmware 5.16.2.1.1.
- The sample has no module_exit cleanup, and class/device failure paths do not
  unwind all registrations. It is unsuitable as an unloadable compatibility
  probe without repair. A first runtime probe should have no kexec execution
  interface and should cleanly unload; it still needs matching ABI first.
- The execution ioctl calls kernel_kexec directly; any adapted interface must
  enforce CAP_SYS_BOOT for each privileged operation and restrict the device
  node. Retain the actual source's memory/segment validation; do not import
  the sample's stubbed insert_resource as a correctness guarantee.
- Active watchdog uses NOWAYOUT. Device shutdown, IRQ/FIQ, cache/MMU state,
  eMMC writes in flight and watchdog servicing need an explicit handoff design.
  A kernel compiled for cold boot does not establish successful kexec entry.

## Better matching source located

Amazon lists Kindle_src_5.16.2.1.1_4097470002.tar.gz specifically under Kindle
Paperwhite (7th Generation):
https://digprjsurvey.amazon.com/csad/help/node/200203720
Direct link:
https://kindledownloads.s3.amazonaws.com/Kindle_src_5.16.2.1.1_4097470002.tar.gz

This is a published source-release candidate matching the firmware label,
not yet acquired/inspected or proven to reproduce the installed binary.
fetch-stock-source.sh obtains and checks the archive without extracting it.
No Amazon/Solarcene account or subscription is involved. Host download attempt
failed DNS. No external signature/hash was found in the inspected listing;
the fetcher records its own hash without claiming signature verification.

## Next bounded steps

1. Acquire the exact-label Amazon bundle and kexec-module checkout, record hashes
   and revision, inspect archive paths/nested kernel/toolchain inputs on host.
2. Run gleworks-module-capture.sh through the existing launcher: collect running
   kernel symbols, module-control settings and stock module binaries. This is
   targeted module ABI evidence absent from the previous hardware/waveform
   capture. It never reads /dev/mem, loads a module, patches memory or reboots.
   Preserve results privately; symbol addresses and vendor modules are not
   publication artifacts. A successful capture does not establish ABI matching.
3. Compare exact build config, module metadata, required exports and shutdown
   implementations. Select a compatible cross toolchain. Modern Clang used for
   the 6.18 diagnostic build is not a validated compiler for the old module.
4. Only after host checks, build an inert ABI probe with unload support; review
   before device loading. Loading kernel code can still crash a mismatched
   kernel. Do not force-load or bypass vermagic checks to get a success result.
5. Develop bounded load/unload of RAM segments before considering execution.
   Build first-boot initramfs and audit USB/watchdog/reset behavior before any
   kernel handoff. Preserve stock boot selectors/images. RAM-only loading is
   not proof of recovery from a hang or accidental storage writes.

No claim that this route already works on PW3. The existing recovery gate still
applies to device handoff; this research identifies a way to investigate it.

## Acquisition and runtime evidence verified

User supplied the downloads and reconnected after module capture.

- kexec-module-reference is clean at
  f010e4145a35f45264912eff653c525d7489eb32. Local code agrees with the
  previously web-inspected files, including the soft_restart requirement.
- Amazon source bundle is 331107525 bytes and passes the fetch manifest.
  Selected nested linux-3.0.35.tar.gz is 119420718 bytes, SHA256
  fa89453deb9947700cbebde2e840230e69969549be2116a0a72331ac5d2fe6c9.
  Extracted with Python tarfile data filtering into linux-stock-5.16.2.1.1;
  no source code executed or edited. Input hashes in stock-source-inputs.sha256.
  Published build instructions select imx60_wario_defconfig and Linaro GCC.
  Toolchain HOW-TO describes Poky 1.8.1; no cross compiler binary is included.
- Capture gleworks-module-capture-20261029-194813-17967: COMPLETE present,
  all 11 manifest entries pass; private host copies match USB files. The
  1962496-byte module archive contains 36 ARM ELF32 module binaries.
- All 36 have vermagic `3.0.35-lab126 preempt mod_unload ARMv7 p2v8 ` and a
  292-byte .gnu.linkonce.this_module section. Captured config.gz decompresses
  byte-identically to stock-kernel.config. Summary: module-abi-summary.json.
- modules_disabled=0, kptr_restrict=0. Baseline tainted=1 was recorded; it is
  not a new probe failure or a reason to clear/alter the setting.
- Runtime symbols confirm machine_shutdown, kernel_restart_prepare,
  memblock_is_region_memory and setup_mm_for_reboot; no soft_restart entry.
  printk and kallsyms_lookup_name have visible export entries. Several handoff
  functions lack visible export entries and cannot be treated as ordinary
  directly linkable module imports. No arbitrary address was called.
  Summary: kexec-symbol-summary.json, with addresses omitted.
- Native machine_kexec.c in the newly acquired kernel is byte-identical to
  the older reference's file and uses the older cache/MMU/reset sequence.

Prepared abi-probe/gleworks_abi_probe.c and its Kbuild Makefile, with load/unload
logging only, version/descriptor-size compile guards and no execution interface.
No .ko built. No validated ARM GCC 4.9 cross compiler is installed locally.
The kernel.org GCC 4.9.4 kernel-only archive is a candidate, differing from the
running kernel's recorded GCC 4.9.1. fetch-legacy-toolchain.sh downloads the
archive and detached signature without extracting/executing either. Directory
listing verified via web; host curl failed DNS. Signature verification, host
runtime dependencies, header preparation, ABI comparison and build remain
pending. A compatible toolchain version alone will not prove a safe module.

Host checks: source archive SHA256, safe nested extraction, captured manifest
and byte comparison, config comparison, all-module ELF/metadata inventory,
symbol presence/export inventory, fetcher sh -n. No new maintained tests.
No device loading, memory patch, bootmode change, reboot or firmware write.

## Follow-up: legacy compiler acquired; host dependency blocker

GCC archive 9581828 bytes passes saved SHA256 and xz integrity checks. Driver
executes via existing nix-ld and reports GCC 4.9.4 / arm-linux-gnueabi. This is
still a candidate, not stock GCC 4.9.1. Detached PGP signature has issuer
60AB47FFC9095227, but signer key retrieval failed and signature verification
is pending; the acquisition hash is not publisher authentication.

Added abi-probe/build-host.sh for out-of-tree generation without modifying
Amazon source. Direct KBUILD_SRC invocation avoids legacy /bin/pwd, absent on
this host. oldconfig completes, dropping only CONFIG_FB_MXC_EINK_REAGL=m,
whose Kconfig definition is absent from this public source. Its references
were found in the EPDC implementation, not the searched generic/ARM include
headers. Retain this source gap for ABI review; generated config is not
byte-identical to the captured config. CONFIG_KEXEC remains disabled.

modules_prepare fails at cc1 startup: libmpc.so.3 is unavailable to the loader;
ELF inspection also shows required libmpfr.so.4 and libgmp.so.10. Local Nix
store contains libmpc.so.3/GMP10 but only MPFR SONAME6, not required SONAME4.
No cross-SONAME symlink or compiler patch applied. No .ko produced. Host curl
cannot resolve keyserver.ubuntu.com. Prepared fetch-legacy-host-libs.sh to
acquire official MPFR3.1.6 and MPC1.0.3 sources/signatures for isolated host
libraries, without building/executing them. Signature verification pending.

Checks: shell syntax for both scripts, archive hash/integrity, GCC version/
target, config diff, ELF dependency inspection and captured preparation log.
No Kindle module load, memory patch, reboot, bootmode or storage modification.

## Follow-up: inert ABI probe built; bounded trial prepared

Newly supplied MPFR3.1.6/MPC1.0.3 archives pass saved SHA256 and compression
checks; safe data-filtered extraction into abi-probe/host-sources/. Built with
local Clang and GMP6.3, installed only to abi-probe/host-prefix/. Existing
upstream suites: MPFR159 passed/1 skipped/0 failed, MPC64 passed/0 failed.
No maintained tests added. Publisher PGP signatures remain unverified.

GCC4.9.4 now prepares stock kernel headers and builds the inert module without
editing supplied kernel source. Exact captured config used as input; generated
config still drops only the absent public REAGL option. CONFIG_KEXEC remains
disabled. Module is 2544 bytes, SHA256
786a8c7fc58b34adfa66c4c6cc745d89c408710da0782f3067a7df8e3d2d063f.
ARM ELF32 relocatable EABI5; vermagic and 292-byte descriptor match all36
captured modules. init/exit relocations at188/284 match31 stock modules with
those entries. Only import printk has a captured runtime export. Disassembly
shows logging only; emitted relocation types are supported in public stock
ARM loader. See abi-probe/abi-comparison.json. Retained missing Module.symvers
warning: modpost alone did not validate imports; no fake symbols/CRC supplied.

Prepared gleworks-abi-trial.sh with module/config hashes, current-kernel and
baseline checks, ordinary load/unload, private before/during/after snapshots,
no forced retry or boot changes. Shell syntax passes; runtime untested. Agent
has not copied anything to USB or loaded a module. Kernel hang remains possible;
logging cannot recover a frozen kernel. USB-only handoff recovery gate remains.

## Device trial verified: 2026-10-29

User ran GleWorks ABI Trial and reconnected. Capture
`gleworks-abi-trial-20261029-201039-20043`: COMPLETE present, all 9 manifest
entries pass. All 13 files copied byte-identically to private-captures/ with
0700 directory/0600 files. Module SHA256 matches the reviewed host artifact.
Normal insmod/rmmod completed. Module absent before, present during, absent
after. Stock module listings before/after are byte-identical. Taint stays 1.

Kernel circular buffer advanced: three old leading lines were replaced by a
partial line. All subsequent baseline lines remain unchanged. New tail only:

```
[15840.372781] gleworks_abi_probe: loaded; module descriptor=292 bytes
[15841.449962] gleworks_abi_probe: unloaded
```

Old userspace undefined-instruction/segfault entries predate this trial; no
new kernel error in the retained delta. See abi-probe/runtime-review.json.
This proves one inert probe's normal load/unload, not arbitrary module ABI,
kexec handoff, Alpine boot, USB recovery or long-term stability. No kernel
handoff, bootmode edit, firmware write or reboot requested by trial script.
Next research stage is a separate bounded RAM allocation/load/unload candidate
without execute/reboot. Its implementation and review remain pending; the
USB-only recovery gate still applies before any kernel handoff.

## Bounded RAM experiment prepared (host only)

User accepted the next RAM allocation/load/unload research step after the inert
ABI trial passed. New ram-probe/ is a separate research candidate, preserving
the prior module/trial. Fixed16 order-zero low-memory allocations, 64KiB total,
GFP_NOWAIT|__GFP_NOWARN: no reclaim/IO waits or emergency reserve use. Module
writes a fixed per-page pattern, reads every byte back at init/exit, frees all
owned pages on allocation/verification failure and on normal exit. No arbitrary
address writes, physical placement, struct page dereference, ioctl, parameters,
workers, symbol lookup, executable payload, cache/MMU changes or boot action.

Built successfully using existing prepared stock headers and candidate GCC4.9.4.
Module3664 bytes, SHA256
e794c29cb81132bed70bf09cb198ff6025209221fba3e608c5eac51e37d10f38.
ARM ELF32 relocatable EABI5; stock vermagic, descriptor292 and init/exit188/284.
Four imports (__get_free_pages,free_pages,memset,printk) all have captured runtime
exports. Relocation types2/28/43/44 handled by public stock source. Inspected
disassembly: bounded16/4096 loops, order0/GFP512, byte reads and cleanup paths.
Retained missing Module.symvers warning; no fake symbol table/CRC/vermagic.
Source REAGL gap/compiler version difference/PGP verification limits persist.

Prepared gleworks-ram-trial.sh using the prior trial structure, exact new hash,
normal load/unload, stock config/kernel/taint guards, and private dmesg/module/
meminfo/taint snapshots. Shell syntax and artifact/hash consistency pass.
No new maintained tests. No device copy/load performed by agent. Runtime
allocation/readback/release remains untested; no claim of leak-free operation,
physical kernel placement, safe kexec, Alpine boot or USB-only recovery.
See ram-probe/README.md and host-review.json. Existing handoff gate remains.

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

## Consolidated real-image staging batch prepared (host only)

User asked to prepare everything feasible and combine one attempt to accelerate
progress. Prepared kexec-load-probe/ and bundle/, preserving all prior sources
and probes. Derived only normal staging/allocation/free code from exact-label
stock kernel/kexec.c; removed crash/syscall/execute/relocation/reset machinery.
Corrected early kfree ownership and unlinked-source cleanup in the isolated
copy, recorded stock-normal-core.diff. Validated metadata snapshot prevents
second-copy race. One segment, fixed0x80008000, <=2MiB payload, exact rounded
memsz, flags0. Page allocations order0/lowmem/NOWAIT with1024-owned-page cap;
small image structure is NOWAIT too. No forced ABI checks or kallsyms resolver.

CAP_SYS_BOOT each operation/open, exclusive fd, mode0600, auto-close/module-exit
cleanup. Local allocation-budget controls affect this module only. Batch covers
malformed inputs, absent execute command, inherited-fd privilege drop, bad user
payload cleanup, budget failures0..7, five real-image sizes through full kernel
with appended diagnostic DTB, readback checksum, unload/zero-page accounting,
and final-close cleanup. No maintained host test suite added.

Build/host review: module10028bytes, static ARM helper5436bytes, staging input
1833449bytes. All21 module imports exported in captured runtime. Descriptor292,
init/exit188/284, stock vermagic; emitted ARM relocations supported. Helper has
no interpreter/dynamic segment/undefined symbol. Forbidden execute/handoff
symbols absent. Independently computed five padded FNV checksums. Bundle
manifest4/4 passes, template/hash substitution and shell syntax verified.

Used requesting-code-review skill for independent read-only reviewer. Found
and fixed an insertion/signal cleanup gap: pre-arm load_attempted and inspect
actual module presence, then normal rmmod. Reviewer confirmed fix and no
remaining must-fix finding. REVIEW.md records scope/limits. Runtime struct-page
ABI, GCC4.9.4/4.9.1 difference, REAGL source gap and PGP verification remain
uncertainties. New module/helper not yet executed. No emulator available.

Prepared one launcher with hash/config/kernel/taint/storage/free-RAM guards,
static-helper startup smoke before module load, private temporary executable/
character node, evidence capture and ordinary cleanup. Agent has only mounted
USB storage access, no established live device shell; no USB copy or runtime
execution performed. User must copy/eject/launch once, then reconnect.

This batch cannot execute the kernel: execution code is absent. Input lacks
initramfs and is not complete Alpine. No firmware write/boot-selector mutation,
shutdown/MMU/cache/watchdog validation, actual handoff or USB-only recovery
established. Existing recovery gate remains in force. No claim of guaranteed
no-brick safety. Complete preparation of the replacement OS remains unfinished.

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

## Continuous boot-readiness audit and embedded diagnostic image

User requested continuous preparation/audit until real boot confirmation,
combining a run where possible. Continued host work rather than another inert
probe. Created native-boot/ separately, preserving prior tested artifacts.
Built Linux6.18 with embedded1928-byte static ARM PID1; actual generated CPIO
contains matching executable/root-owned init and console5:1. Kernel1812504B,
DTB20105B, appended image1832609B; SHA256
8d6046f8a401dd82f9dec0c70ab1209b5db782ecc674ed7dbd20b5f132813f92.
Static ARM ELF/no interpreter/dynamic segment checks pass. Storage/network/
watchdog/PM/display drivers disabled; ATAG conversion disabled to avoid stale
stock ATAG dependence. Source checkout remains unchanged. Hardware untested.

PID1 mounts only devtmpfs/proc/sysfs and responds over USB ttyGS0 with running
kernel/PID1/fresh32hex challenge. Host observer requires matching live nonce
and6.18 release. It is liveness evidence, not cryptographic attestation. No
shell/root-disk mount, reboot or handoff in PID1. No native boot observed.

Ruling: preserve recovery gate and do not open /dev/watchdog for supposedly
read-only queries. Public stock open deletes kernel ping timer; NOWAYOUT close
pings once without restarting it. GETBOOTSTATUS always returns0. A blind query
could itself cause reset and would not prove its reason. Instead built static
readonly /dev/mem MMIO reader for WDOG1 WCR/WRSR/WICR and SRC GPR9/GPR10.
Ruling: do not assume watchdog reset returns to main; source documents GPR
warm-reset override and conditional cleanup. Need actual state and eventual
reset/return proof; speculative handoff risks losing current device access.

Consolidated readiness bundle reads targeted watchdog/reset/stock boot data
before free-RAM staging guard, then uses the tested load-only module/client
on the new initramfs-containing image. Exact hashes/config/taint checks,
private temps/evidence, normal cleanup. No execute path/flash/boot selectors
or reset command. Agent cannot launch via mounted mass storage; no live shell
or network management session established. User launch is the next external
step. No extra device attempt or mounted-USB write by agent.

Fresh independent review found PID1 EOF reconnect and host termios-error
handling bugs. Both reproduced failing then passing in disposable host
simulations; wrong nonce rejected/correct accepted in PTY simulation. These
are not hardware boots. Rebuilt kernel after fixes. Integrated archive/ELF/
hash/template/bundle packaging into build-host.sh to prevent stale artifacts.
Five bundle manifest entries pass; four shell scripts parse; source checkout
clean. No maintained tests added. HANDOFF-AUDIT.md records source findings,
review corrections and remaining limits.

Remaining: actual watchdog/SRC readback, watchdog reset/return qualification,
native handoff adaptation/review, board USB/power and genuine kernel/PID1
observation. No Alpine image/runtime or ordinary-USB recovery confirmed. This
turn did not satisfy the user's final boot-confirmation objective.

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

## Bounded watchdog qualification package — 2026-10-29

Prepared `watchdog-trial/bundle/`: fixed stock-watchdog guard module, static
ARM helper, readonly register reader, launcher and four-entry hash manifest.
No device execution or mounted-device write. All 14 ordinary module imports
match captured exports. One nonexported callback, imx2_wdt_reinit(void), is
resolved by exported kallsyms and restricted to captured address0xc04542c8.
Captured stock-main gzip image was independently decompressed/disassembled:
that function calls printk, imx2_wdt_start, then imx2_wdt_timer_ping(0), and
returns normally. `watchdog-trial/callback-review.json` records provenance;
this supports source interpretation, not live callback/recovery proof.

Recovery work is scheduled before standard watchdog open; owned file close
then stock timer reinit on explicit abort, final guard close, normal unload
or150s deadline while scheduler/workqueue remains functioning. Two arms max
per load (immediate callback-state check then reset trial), no timeouts/boot
selectors/flash changes. Screen-saver block is temporary, requires prior0
and battery>=40%. Idle/busfreq feeds may prevent reset; no forced idle/clock
change or CPU busy loop. No-reset remains inconclusive. The immediate check
only proves callback return/state; periodic feeding remains unobserved.

Fresh independent review found cmp errors mislabeled as boot ID differences.
Disposable simulation reproduced before fix; after prior-file validation and
explicit0/1/error handling all four collector cases passed. Module source-
inclusion simulation with mocked kernel functions passed permission/input,
queue/open failure, explicit restore, lease-close, deadline, normal-unload
and attempt-cap paths. These are not hardware tests. Three shell syntax
checks, Python syntax, ARM/static helper and4/4 bundle hashes passed.
No maintained tests added. Absolute no-brick assurance is not established.

Next external action: copy bundle to documents and launch Watchdog Trial; if
stock restarts, launch it once more to collect evidence only before USB
reconnection. Changed boot ID alone is insufficient; review hardware WRSR
timeout bit1 and prior intent/logs. Native handoff remains unimplemented and
boot6.18/Alpine remains unconfirmed.

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

## Expanded consolidated boot suite — 2026-10-29

User authorized multiple approaches in one run, then requested an expanded
suite covering every feasible path to improve first-boot decision quality.
Prepared boot-gate-batch/TEST-MATRIX.md:31 identified checks/routes, including
source/ABI/image/CPIO/DTB, RAM faults/ownership/readback, CPU/IRQ/memory/thermal
baseline, abort/reset recovery, native handoff, generic3.8 module, Fastboot,
persistent-image/vendor-kernel/chroot alternatives, USB native liveness and
Alpine/product gates. Known nonviable paths are documented rather than retried.
No numerical boot-success probability can be inferred.

Host audit10/10 structural groups pass,9 packaged artifacts verified. Existing
C modules/helpers/image retained. Wrapper runs readonly snapshot, existing
65-check staging suite, then bounded runnable watchdog candidate last. A
leftover module blocks watchdog; staging guard is not relaxed. No autorun/
flash/boot selector or native execution interface. One manual post-reset
launch is needed to collect evidence, because reset terminates the process.

Independent review identified a marker race: an active-capable child could
re-arm if its pending marker disappeared between wrapper check and launch.
Added --collect-only latch to the child, preserved through su. Disposable
mock reproduction shows unlatch fallthrough versus fixed collection exit;
7 wrapper ordering/failure/leak/pending cases pass. Wrapper-only signals need
not immediately abort a child; existing lease/deadline govern its cleanup.
Three generated shell and2 Python syntax checks pass. These simulations are
not hardware boots; no device command or mounted-device write by agent.

Overall NO-GO remains: native handoff absent and watchdog timeout/reset/stock
return unproven. The batch can clear prerequisites, never automatically
execute an unreviewed kernel. See boot-gate-batch/host-audit.json and plan
ours/docs/superpowers/plans/2026-10-29-boot-gate-batch.md.

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

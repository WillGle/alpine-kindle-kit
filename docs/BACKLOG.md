# Backlog duy nhất — Kindle Alpine

Lịch sử được giữ nguyên theo các nguồn nhập bên dưới; các mốc NO-GO cũ có thể
đã được bằng chứng về sau thay thế. Trạng thái hiện hành nằm ở
[guides/CURRENT.vi.md](guides/CURRENT.vi.md), không suy từ checkbox lịch sử.
Ngày ghi trong từng nguồn là ngày gốc; thứ tự nhập không phải thứ tự thời gian.
Các đường dẫn trong transcript/lệnh cũ là snapshot lịch sử, không phải lệnh để
chạy lại. Hướng dẫn thao tác hiện hành ở guides/.

## Công việc còn mở

| ID | Trạng thái | Việc cần làm |
| --- | --- | --- |
| B01 | Chưa làm | Release mới gồm full refresh và dọn locale; xác minh gói/ảnh trước triển khai |
| B02 | Chờ kiểm chứng | Đọc nhiều trang thực tế sau đổi chính sách refresh |
| B03 | Một phần | Wi-Fi native + auto-reconnect đã đạt; Tailscale/SSH sau xác thực còn mở |
| B04 | Chưa triển khai | Web UI upload tài liệu có xác thực qua VPN |
| B05 | Một phần | Mem suspend/RTC wake/đường nút đã đạt; bấm nút thật và đo điện năng còn mở |
| B06 | Chưa kiểm chứng | Boot Amazon fallback sau dọn; không tự thử chỉ để đóng backlog |
| B07 | Một phần | App runtime: package/installer/UI đã đạt; còn generic launch, UID/limit/fd handoff, crash/OOM, data image và checklist `AS-*`. Contract: [2026-10-31-app-session-contract.md](technical/superpowers/specs/2026-10-31-app-session-contract.md); kế hoạch: [2026-10-31-app-runtime.md](technical/superpowers/plans/2026-10-31-app-runtime.md) |
| B08 | Một phần | App manager + signed USB installer/update UI đã đạt; OS A/B, catalog online và UID riêng còn mở |
| B09 | Chờ nghiệm thu thiết bị | UI polish Phase 0–7 đã đạt trên host (2469 check/0 fail, heap 69.8%, ARM 3.064.832 byte); còn deploy, kiểm tra hình ảnh/ghosting và paint-quality regression trên PW3 |

Đây là danh sách theo dõi, không tự cấp phép thay đổi boot hay mở rộng task.

## Mục lục lịch sử nhập từ repo

- [ours/linux/EXECUTION.md](#history-001)
- [ours/APPLIANCE-PLAN.md](#history-002)
- [ours/EXPANSION-PLAN.md](#history-003)
- [ours/PLAN.md](#history-004)
- [ours/VERIFICATION.md](#history-005)
- [ours/docs/superpowers/plans/2026-10-29-boot-gate-batch.md](#history-006)
- [ours/docs/superpowers/plans/2026-10-29-independent-linux.md](#history-007)
- [ours/docs/superpowers/plans/2026-10-29-popup-errors.md](#history-008)
- [ours/docs/superpowers/plans/2026-10-30-alpine-safe-native-boot.md](#history-009)
- [ours/docs/superpowers/plans/2026-10-30-stock-kernel-alpine.md](#history-010)
- [ours/linux/boot-gate-batch/TEST-MATRIX.md](#history-011)
- [ours/linux/bootstrap/PLAN.md](#history-012)
- [ours/linux/native-display/PLAN.md](#history-013)
- [ours/linux/native-handoff/PLAN.md](#history-014)
- [ours/linux/native-stage/VERIFICATION.md](#history-015)

<a id="history-001"></a>

## Nguồn lịch sử: ours/linux/EXECUTION.md

# SDD ledger — plan: ours/docs/superpowers/plans/2026-10-29-independent-linux.md

## Current status pointer — 2026-10-30

Read [native-kit/CURRENT.vi.md](guides/CURRENT.vi.md) first. Persistent native
Alpine + KOReader and independent USB rescue have since been physically verified.
Ghosting refresh policy and locale trimming are live/source changes after v4;
the v4 archive is a frozen earlier release. Entries below remain chronological
history, including NO-GO decisions superseded by later evidence.

## Historical milestone — early 2026-10-30

Host coldstart qualification now advances beyond hardware RAMPID1: exact captured
stock kernel + stock bootargs boots Alpine asPID1 in PW3/MuscatQEMU. Corrected
init symlink/staticdevnodes, sysinitRC0, guardian90s and USBserialdriver readiness
verified; physicalUSB enumeration remains unverified. Officialsigned sameversion
FB03 restores both missinginit and corruptroot syntheticdisks, byteidentical to
signedoriginalrootfs. This is emulatorrescue, not physicalUSBrescuequalification.
See bootstrap/emulator/signed-recovery-review.json and appendedexecutionnotes.

Alpine3.24.3 BusyBox/musl ran as actual global PID1 with root in /dev/shm on
unchanged stock3.0.35;44 fresh-nonce proof observations through90.33s. Existing
Amazon processes temporarily retained for Wi-Fi/SSH. Deadline reset followed;
fresh authenticated SSH proves changed bootID, restored UpstartPID1 and cleared
RAM trial. See [proof](../linux/pid1-handoff/pid1-proof.json) and
[method and limits](technical/linux/pid1-handoff/README.md). Not independent cold boot, clean
Amazon-free runtime, modern Tailscale or permission to erase stock rootfs.
Persistent cutover remains NO-GO until ordinary-USB rescue independent of the
rootfs being replaced is demonstrated. Modern-kernel DMA handoff still NO-GO.
Earlier entries below are chronological evidence, not the current status.

## Execution — 2026-10-29

Scope: remaining unchecked execution items and accepted product requirements.
Earlier checked investigation steps were read, not reimplemented. Spec read:
the latest accepted scope in the plan; `ours/docs/technical/investigation/FINDINGS-2026-10-29.md`
and `BOOT-ROUTES.md` provide historical evidence.

Ruling: use this directory for the ledger and host artifacts, with no worktree
or commits — the plan explicitly states there is no usable repository metadata
and restricts changes to ours/linux; `git status` confirms no usable root repo —
cost: no root-level commit history or automatic git-based task tracking.

Ruling: honor the user's no-new-tests instruction and use syntax, source and
runtime checks — it supersedes skill TDD defaults — cost: no new regression suite.

Pre-flight: kernel/image layout feeds boot handoff; kernel drivers and ABI feed
Alpine/KOReader/networking; recovery gates every persistent boot mutation.
Host-only acquisition/review can proceed independently of the recovery gate.

| Plan item | Result this run |
| --- | --- |
| USB installation/recovery | Blocked: no demonstrated USB-only recovery from a failed replacement kernel; no bootmode changes or Fastboot entry attempted |
| Kernel selection/patch/DT audit | Partial: public upstream audit baseline pinned, missing upstream PW3 DT target confirmed, acquisition blocked by host DNS; complete public port still required |
| ARM toolchain/out-of-tree build | Pending: no audited complete PW3 kernel revision selected; no barebox build substituted |
| Independent boot/recovery trial | Blocked by recovery and artifact gates |
| Reader/network/power acceptance | Pending a booted independent OS; appliance results not reused as acceptance |

Additional source checks: installed-family U-Boot at
`4b6c52358630564562224b780bd75a5659fbc84b` routes bootmode in
`board/imx60_wario/imx60_wario.c:1774` onward. Fastboot setvar writes IDME,
boot executes bootcmd, and reboot does not itself clear bootmode
(`drivers/usb/gadget/fastboot.c:708,1054,1072`).
`check_haptic_and_recovery` is haptic I2C error recovery in
`post/board/imx60_wario/inventory.c:490`, called for Icewine variants; it is
not a newly discovered Muscat OS recovery button. Main/BIST configs set
`failbootcmd=panic`, not an automatic USB rescue. Source alone does not prove
every behavior of the installed firmware; the prior actual-binary review still
supplies the stronger Fastboot-boot evidence.

Decision: no boot entry or persistent writes while the plan's recovery gate is
unmet. Do not mark any remaining execution checkbox complete. Independent Linux
is not demonstrated impossible; safe replacement is not established here.

Final review found the plan had changed during execution to exclude Ponder and
inaccessible smallsolar patches. Removed the newly added Ponder fetcher and
replaced it with a pinned public upstream fetcher; kernel review now follows
the public-source scope. Earlier Ponder access failures are historical only.

Ruling: pin vanilla Linux v6.18 only as an audit reference, not a target image —
its public i.MX DTB list has no PW3 target and full support remains unverified —
cost: porting or rebasing onto a suitable stable revision remains necessary.

Host acquisition failures, shell checks and capture integrity results are kept
in `execution-checks.txt`. No maintained tests added. No device configuration,
bootloader, filesystem or remote server changed.

Verification: three shell syntax checks passed; disposable existing-directory
and wrong-commit checks passed using real local Git. All eight private capture
files passed their existing SHA256 manifests; both retained source checkouts
remain clean at their recorded revisions. Actual public fetch failed DNS
(exit 128); successful upstream acquisition/snapshot/build paths remain untested.

Final review: one fresh reviewer identified the superseded source dependency;
the fix pass removed it and verified the replacement fetcher's refusal paths.
No deferred minor findings. Ledger retained because the plan remains unfinished;
no root repository exists for commits or branch integration.

## Follow-up: public kernel acquired

User supplied successful fetch. Live checks confirmed the v6.18 commit, clean
checkout and snapshot SHA256. Full-tree driver-name scans and targeted Kconfig,
SDIO ID and ARM boot-compatibility inspection completed; findings appended to
KERNEL-REVIEW.md. Missing PW3 DT, MAX77696, CYTTSP4 and i.MX EPDC support prevent
a device-complete build. The baseline download gate is resolved; the driver
port and USB recovery gates are not. No device writes.

## Follow-up: vendor kernel reference checked

Validated clean commit 7ac2585f384573da8cbdcc987a3503e2e6ff1fb4. Found concrete
MAX77696, CYTTSP4 and Muscat board source. Origin is 5.10.1.1, older than this
unit's firmware; full equivalence is not assumed. Named REAGL routines have
references/declarations but no C/header implementations in this tree. Review
recorded in VENDOR-REFERENCE-REVIEW.md. No build or device mutation.

## Follow-up: public EPDC candidate found

Inspected public akemnade/linux kobo/drm-merged-6.15 EPDC files via raw GitHub.
Found i.MX6SL matching and 5-bit waveform handling; no PW3 runtime claim.
Recorded source links, mutable-revision limitation, waveform extraction lead
and inkwave's 5-bit limitation in EPDC-CANDIDATES.md. No source checkout,
build, flash or bootmode change in this research step.

## Follow-up: EPDC fetch and waveform capture prepared

Resolved public branch via git ls-remote to
8165a8cc247ab741a163c2d1b42d95e616c10d83. Clone failed DNS; no acquired tree.
Prepared fetch-epdc.sh with exact-commit check using the existing fetcher style.
Prepared gleworks-waveform-capture.sh with read-only proc inputs, duplicate
waveform reads, local hashes and explicit completion boundary. Shell syntax
checks passed; shellcheck unavailable. Device execution and vendor format/
checksum validation remain pending. USB mount is outside writable roots;
user must copy/run the script. No device mutation performed.

## Follow-up: waveform and EPDC acquisition verified

User reconnected after capture. All 16 manifest entries and both duplicate
waveform pairs verified; private host copy matches USB files. EPDC checkout
is clean at the pinned commit and archive checksum passes. Panel waveform
CRC matches over its declared length; active waveform CRC does not, despite
stable duplicate reads. Detailed sizes and evidence boundaries recorded in
WAVEFORM-CAPTURE-REVIEW.md. No device writes or builds.

## Follow-up: analyze waveform and prepare diagnostic boot artifacts

User authorized analysis and boot preparation. Added read-only host analyzer;
all capture hashes, duplicate reads, panel CRC, mode/temperature pointer bounds
and 112 voltage-record checksums pass. Panel trailing bytes are zero. All
voltage-control records share the same first seven fields; decoded VCOM and
raw fields retained privately. Public EPDC's mode_gc4 mapping differs from
vendor mode version 25; voltage/algorithm integration remains unresolved.

Built Linux 6.18 diagnostic zImage (1813344 bytes) and minimal investigational
USB DTB (20105 bytes) outside clean source tree. Retained config, toolchain,
logs, hashes, host build script and structural checks in boot-prep/. Fixed
host-wrapper contamination by using raw target clang and linker; final kernel
and decompressor are ARM ELF32 without interpreter/dynamic segments. Manifest
verification passes. Upstream DT duplicate-unit-address warning retained.
No maintained tests added. No complete PW3 peripheral port or Alpine image.

Extracted private stock main/diags uImages from the existing capture, verified
header and data CRC plus private SHA256 manifest. These are preservation
artifacts, not demonstrated recovery. No initramfs or combined trial uImage
created: kernel compilation does not supply the missing recoverable launch
mechanism. No writes to Kindle, bootmode changes, reboots or hardware trial.
Next gate remains an ordinary-USB entry/recovery mechanism; first-boot userspace
and board USB/power/watchdog validation are also required before trial.

## Follow-up: kexec-module route investigated

User accepted researching temporary RAM boot. Inspected public reference files
via web and compared local 3.0.35 source: soft_restart requirement mismatches,
unload/error cleanup and privileged ioctl checks need adaptation. Located the
Amazon PW3 source release matching firmware label 5.16.2.1.1/4097470002; actual
archive and kexec checkout acquisition failed DNS. Prepared host source fetcher
and targeted read-only module/symbol capture script. See KEXEC-REVIEW.md.
No module built/loaded, forced ABI checks, memory writes or boot operations.

## Follow-up: kexec inputs and captured module ABI checked

Verified exact-label source archive and clean kexec reference commit
f010e4145a35f45264912eff653c525d7489eb32. Safely extracted selected kernel
source; no edits to supplied trees. All 11 capture manifest entries pass;
36 stock module binaries share vermagic and 292-byte module descriptor.
Running config matches the saved config; runtime soft_restart is absent.
Prepared inert load/unload-log-only ABI probe source. Compiler not acquired:
GCC 4.9.4 candidate/download-only fetcher documented; host network DNS failed.
No module build or device loading performed. KEXEC-REVIEW.md records details.

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

Expanded host-audit review: fixed manifest/JSON membership enforcement and
stock uImage declared-payload envelope checks. Disposable RED/GREEN inputs
reproduced both missing checks and reject both after fix. Also linked actual
compressed blob in zImage -> decompressed Image -> exact generated CPIO bytes;
CPIO/init is therefore checked in the actual image, not just a sidecar.
Source revision/generated config still do not independently prove build
provenance; this limitation is explicit. Final actual host audit10/10 pass,
7 wrapper mock scenarios and collection-only latch pass;3 shell and2 Python
syntax checks pass. No new device run.

## Reported restart; post-reset evidence not yet collected — 2026-10-29

User reports automatic restart after consolidated run. Current USB exposes
boot-gate214557-32214 and watchdog214602-1418 pre-reset captures. Both pending
markers remain; no collection capture, new boot ID or post-reset WRSR yet.
Private copies preserved; watchdog PRE-RESET4/4 verified, final manifests
absent as expected for interrupted/reset runs. Watchdog helper reached runnable
wait after immediate callback-state check; no deadline-return result saved.
This is consistent with interruption/reset but insufficient to attribute cause.
No native kernel executed. Need one collection-only launcher action after
stock restart, then reconnect; do not re-arm the test.

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


## Owned-chain/fixed-image preparation integration — 2026-10-30

Implemented isolated native-stage module, preserving proven load-only module
and minimal native image unchanged. Allocator records actual owned lowmem
pages and roles on alloc/free/list/control/source/collision transitions.
Whole-chain validation precedes readback and SHA256 dereferences under mutex.
New GW_PREPARE binds fixed1,832,609-byte image to SHA256, fills owned control
page using reviewed relocation data and returns; no gw_enter_handoff/restart
callback or execute ioctl linked. Exact-stock private SHA256 subset avoids
assuming CONFIG_CRYPTO_SHA256 provider availability.

ARM build clean; captured export imports match. Disposable actual-source
SHA256 ASan/UBSan host check passes33 hashlib comparisons. Five-artifact
bundle hashes and shell syntax pass. Integration and final bundle independent
reviews report no must-fix. Minor deferred: early output/log failure can leave
a /tmp lock before trap installation; no module loaded then.

Prepared native-stage/bundle launcher with exact input hashes, existing12MiB
free-RAM guard, allocation/fault/readback/close cleanup cases plus fixed-image
prepare, repetition, wrong-size/argument/corruption rejection. Collects device
driver/resource associations read-only. No live trial run, watchdog arm or
boot/flash write. User must copy/eject/launch then reconnect to obtain fresh
runtime evidence; currently no native-stage capture on USB.

Additional source audit: SDMA and PXP platform drivers also lack shutdown.
PXP terminate_all changes software lists only; suspend waits unbounded for
ENABLE clear before reset. Thus neither is a general safe DMA fence. USB
active associations include gadget, EHCI and OTG; stop order needs review.
head.S/linked-symbol derivation bounds relocated code/DTB/stack/64KiB heap
below0x805ad6e0, within declared RAM; reserved-memory/DMA qualification remains.
Native execution NO-GO until device preparation, complete bounded DMA stop,
watchdog recovery across shutdown and native physical USB are qualified.


## Native-stage device qualification — 2026-10-30 00:29

User requested check after reconnect. Capture native-stage002935-10358:
25 manifest hashes verified;28 private files copied byte-for-byte;4 launch
inputs match reviewed host artifact identities. Helper reports78 PASS lines.
New fixed image1,832,609bytes staged with451 owned pages; registry/chain,
fixed SHA256 and control-page preparation return successfully. Repeat prepare,
post-prepare readback, wrong-size/argument/corrupt-image rejection, fault budgets
0..7, inherited-fd privilege check and final-close cleanup passed. Normal unload
logs owned_pages=0; module lists identical and taint remains1.

MemFree112096→111104KiB; the delta is not an independent leak measurement.
31 new normalized kernel lines all expected staging/fault/cleanup messages.
Undefined-instruction messages in stock userspace already exist in before-log;
none new in this captured trial interval. Watchdog register snapshot unchanged
WCR773d/WRSR10/WICR8004/GPR9=GPR10=0; reader did not arm watchdog.
Native kernel execution/Alpine boot not attempted. Task1 returning preparation
qualified for exercised cases. Next: bounded complete DMA stop and credible
watchdog/reset recovery across shutdown before enabling execute.


## Alpine host preparation and autonomous-control prerequisite — 2026-10-30

Ruling: retain the planned modern-kernel route, and investigate stock-kernel
plus independent Alpine PID1 as a separate native-userspace route. A running
chroot is not completion. Neither route permits deleting Amazon rootfs before
independent cold boot and ordinary-USB recovery are demonstrated.

Pinned official Alpine3.24.3 ARMv7 minirootfs acquired:3,183,009bytes,
SHA256 dc1ddfaf03ede4be79ad7475490f6b7859dbcbbbbb85150aa1ee013b4bc2f0c1.
Published SHA256/SHA512 match via verified TLS; no publisher-signature proof.
Selected BusyBox/musl ELF dependencies inspected; QEMU Cortex-A9 user-mode
shell/applet checks passed on the host kernel, not Linux3.0.35 hardware.
Isolated Linux6.18 Alpine kernel built with embedded1,251,328-byte CPIO;
actual embedded contents verified. Combined image2,722,513bytes SHA256
16c9be187c4ddefe61b2b5e184b03085db557f19acc9a9191c036cfa947ab92f.
Alpine candidate uses nohlt/poll idle; this is not watchdog-clock proof.
Diagnostic image and proven modules retained unchanged.

Separate alpine-stage cap3MiB/budget1024 pages, fixed-image SHA/DTB identity,
returning preparation only, clean ARM build and export checks. Independent
review found no must-fix. Actual chain host ASan/UBSan check passes665-page
image,768-page cap and invalid termination/size rejection. Device preparation
and native boot have NOT run; DMA/recovery/decompressor qualification remains.

Stock boot-source review supports external uncompressed initramfs through
legacy MULTI + ATAG_INITRD2; exact stock config supports INITRD but disables
compression and DEVTMPFS. This is not a tested production boot-image recipe.
No stored image was replaced. Extracted vendor Upstart archive from exact
firmware source:990,540bytes SHA256
55b4d76d93a69c5d0e7b25a1cbea7465aed158d8c8708dfbfb4c5a4f98d8ea2c.
init/main.c registers no SIGTERM reexec handler; util/telinit.c sends SIGTERM
for u/U anyway. Newer upstream Upstart reexec behavior is inapplicable.
No telinit u, PID1 signal, ptrace or mount takeover has been attempted.

Ruling: prepare a temporary authenticated Wi-Fi SSH session to remove repeated
USB launch/collect cycles. Current physical connection is mass storage only;
there is no agent-accessible shell and copying a script cannot execute it.
control-session captures selected init/stock-library evidence, runs Alpine
BusyBox/musl ABI-only chroot and prepare-only Alpine staging, then opens SSH
with separate keys, pinned server identity, exact host-source firewall rules,
no passwords/forwarding and bounded listener lifetime. Host-native key/pinning
check passed and missing-key authentication was rejected. Initial host check
under /tmp was rejected by stock Dropbear parent-directory permissions; repeat
inside private host directory passed. KOReader's device build has its own
permission patch; actual device execution is still required. No host firewall
or authorized_keys changes. No new maintained tests added.


Temporary-control final review identified four Important findings, now fixed:
launcher waits for post-sync readiness instead of returning while captures
write; cleanup snapshots/stops only its private executable's descendants,
checks process start times before TERM/KILL and leaves unrelated processes;
owned port-specific INPUT chain accepts this host and drops other sources;
exact child stage launcher is included in the pinned manifest (and itself
checks all four staged inputs). Exact-source disposable host process-tree
cleanup check passed including descendant removal/unrelated-process survival.
Host SSH key/pinning/missing-key tests passed. Shell/Python syntax, all nine
input hashes and readiness/firewall/pinned-child structural checks passed.
No assertion of actual Kindle execution or native Alpine boot follows.


## Prepared USB handoff and second boot-image hypothesis — 2026-10-30

Copied17 files into mounted Kindle documents: Alpine prepare-only bundle and
temporary-control launcher/nine pinned inputs. Every destination readback hash
matches host; all pre-existing files were retained byte-identical. No script
execution occurred via mass storage. Host observer waits only for an incoming
SSH-banner connection, then pins device key and requires root/stock PID1/nonce.
One safe-eject and physical GleWorks Alpine Session launch is still required;
no accessible remote execution channel exists yet.

Alpine modern-kernel decompressor arithmetic now reproduced by review-memory.py:
prior diagnostic's six recorded addresses match; new inflated end8050c81c,
relocated code8050d100..807a5b00,DTB807a0c68,stack807a6b10,
64KiB-heap end807b6b10. Within declared RAM, no hardware-reservation/DMA proof.
Original diagnostic is unchanged.

Built separate private stock-alpine-boot host-only MULTI candidate:5,209,408bytes,
SHA256 e7c852f86b1332e9efeaaa12124de095e4e0051030683fe8782300fa6797cd41.
Exact captured stock kernel3,828,980bytes preserved; rawCPIO1,380,352bytes
contains Alpine3.24.3 BusyBox/musl, PID1 symlink/inittab, staticconsole/null,
three exact captured USB modules and fresh-nonce proof. No Amazon rootfs mount,
network/storage command or flash recipe. Actual captured head opcodes and LC0
corroborate source overlap arithmetic:relocated code806b4000..80a5aca0,
stack80a5bcb8,heap80a6bcb8; source-loaded initrd80ba6d40..80cf7d40
is outside this derived envelope. This does not qualify reserved memory or boot.
Independent disposable parser checks actualMULTI CRCs,size table,kernel bytes,
rawCPIO init/modes/devices/module count/release and script syntax. Hardware ABI,
productionMULTI handling, earlyUSB and cable-only rescue remain unvalidated.
Candidate stays PRIVATE HOST ONLY; flash NO-GO; no boot change/device execution.


## Alpine ABI and larger-image hardware qualification — 2026-10-30 01:05

Reconnect produced session010508-14264 and stage010508-14349. Copied52 files
privately with byte/hash readback; all23+25 supplied manifest entries verified.
Session's nine copied-input checks pass; module/helper/image/reader input hashes
match host. Alpine3.24.3 BusyBox+musl successfully executed in minimal chroot on
actual3.0.35; /proc/1/exe remains/sbin/init.exe, capturedversionUpstart0.6.6.
This is ABI/runtime proof only, not an independent boot.

Alpine-stage helper78PASS:2,722,513-byte fixed image,668owned pages,FNV a9dc841e;
whole-owned chain, fixedSHA256/control-page prepare/repeat/readback passed.
Wrong-size/unknownarg/corruption and budgets0..7 reject/clean correctly; final
close/unload leaves0owned pages. Before/after module lists equal, taint1→1.
MemFree93616→91260KiB is not a leak measurement. Dmesg ring wrapped; unique
50-line before-tail anchor survives in after-log, followed by31new expected
stage/fault/cleanup lines only. Existing stock undefined-instruction/Thread-80
faults predate trial. Watchdogread-only773d/10/8004,GPR9/10zero unchanged.
Returning Alpine-image staging is qualified for exercised cases; do not repeat
unchanged. Native execution, DMA shutdown/recovery and independentPID1 remain.

Control capture reports Dropbear started01:05:14 at192.168.1.200:2222. Host
pinned-key SSH attempt fails before authentication:No route to host; neighbor
resolutionFAILED. No evidence proves a key/server fault; source and actual
network captures are insufficient to establish association/current route or
USB-mode/AP behavior. Observer gains --address for bounded retries against
this exact captured endpoint (no LAN scan), preserving pinned host key/nonce.
No rootfs/bootloader/kernel/bootmode/partition write. User is instructed to
unplug USB and keep Kindle awake/on Wi-Fi so the existing session can be used.


## Authenticated Wi-Fi and RAM PID1 preparation — 2026-10-30 01:25–01:34

After USB unplug, pinned-key root SSH to the captured endpoint succeeds with
fresh challenge; PID1 remains /sbin/init.exe, kernel3.0.35. Device control no
longer requires a physical launcher for each returning experiment. USB mass
storage attachment correlated with failed neighbor resolution; cause unproven.
Prior unreachable observations remain historical, not current status.

Ruling: investigate live RAM-only Alpine PID1 while retaining the stock kernel,
IRQs and device drivers. This avoids the unqualified DMA/kernel transition for
this experiment; it does not qualify cold boot, modern Tailscale or Amazon OS
removal. Old Amazon processes temporarily retained for Wi-Fi/control are an
explicit limitation, not independent-system completion.

pid1-handoff builds freestanding ARM trace/bootstrap and a separate exact-ABI
kernel deadline module. All24 imports have captured exported symbols. Latest
child qualification passes four TRACEME/ATTACH valid-exec and ENOENT rollback
cases, including nanosleep/restartblock continuation. Absent and idle guardian
reject before PID1 attachment. Returning2s kernel deadline fires(state4), cancel
returns idle/unpins, next deadline cancelled remains idle, normal rmmod succeeds;
taint unchanged. Dmesg499 before-tail lines match, followed only by two expected
undefined-instruction child traps and guardian idle/test/removal messages.
Harness output assertions required correcting grep state= formatting and
allowing automatic udev node removal; these were not device module failures.

Review fixes: distinct injectedPC avoids signal.c restartblock rewrite;
marker-only authorization replaced by live compiled-token ioctl(state2 and
>=60seconds remaining); bootstrap binds root trialnonce; userspace sync/reboot
replaced with process-context kernel emergency_restart on exact stock PMIC path.
No PID1 attachment or actual guardian reset yet. RAM root and bind mounts are
prepared in confirmed /dev/shm tmpfs; /tmp is not assumed RAM-backed.


Ruling: combine the first RAM-only PID1 experiment and first real kernel-guardian
reset qualification in one90s monitored interval, on unchanged running stock
kernel/drivers. Returning timer/state checks and exact stock PMIC reset source
are proven; real reset deadline is not pre-proven. This minimizes reboot cycles
per user request; if the reset callback blocks, physical power-button recovery
may be needed. No stored boot image/selector/partition/rootfs is altered. This
is not authorization to use the same guardian across a kernel/DMA handoff.
Reviewer findings fixed: mutex snapshot+state/deadline wake predicate eliminates
obsolete wait; real tracer blocks ordinary termination signals during PID1
attach-to-detach. SIGKILL remains unmaskable. Latest returning qualification
passes ARM→CANCEL→TEST2s, fired-test cancel, immediate-test cancel, normal unload,
absent/idle rejection and all four child transfers. Inputs and fresh challenge
recorded in pid1-handoff/trial-request.json before execution.

First attempt aborted before PID1 memory editing: PTRACE_ATTACH accepted but
wait4 returned failure immediately. Tracer exited/detached; live recheck shows
UpstartPID1,TracerPid0, unchanged registers not yet edited. Kernel guardian
cancelled beforedeadline, screen-saver property restored. Exact kernel/exit.c
eligible_child excludes non-SIGCHLD exit_signal unless __WALL is explicit;
stock init is created by kernel_thread without SIGCHLD. Added __WALL to wait4
and waiterror diagnostics; four child qualification cases pass again. This is
an evidenced old-kernel wait semantics fix, not a blind retry.


Ruling: combine the first RAM-only PID1 experiment and first real kernel-guardian
reset qualification in one90s monitored interval, on unchanged running stock
kernel/drivers. Returning timer/state checks and exact stock PMIC reset source
are proven; real reset deadline is not pre-proven. This minimizes reboot cycles
per user request; if the reset callback blocks, physical power-button recovery
may be needed. No stored boot image/selector/partition/rootfs is altered. This
is not authorization to use the same guardian across a kernel/DMA handoff.
Reviewer findings fixed: mutex snapshot+state/deadline wake predicate eliminates
obsolete wait; real tracer blocks ordinary termination signals during PID1
attach-to-detach. SIGKILL remains unmaskable. Latest returning qualification
passes ARM→CANCEL→TEST2s, fired-test cancel, immediate-test cancel, normal unload,
absent/idle rejection and all four child transfers. Inputs and fresh challenge
recorded in pid1-handoff/trial-request.json before execution.


## Actual Alpine global PID1 in RAM — 2026-10-30

Corrected __WALL trial succeeded: PTRACE_EVENT_EXEC followed by detachedPID1;
/proc/1/exe=/dev/shm/gleworks-pid1-handoff/root/bin/busybox and /proc/1/root is
that tmpfs root. Pinned BusyBox SHA256df4dd71f... and musl2acf3c82... rechecked
through /proc/1/exe and /proc/1/root. Freshnonce cbea358187e1b5776a2f65fb6e4c213a matches
sysinit proof parent=1, /bin/busybox, Linux3.0.35-lab126, Alpine3.24.3.
PID1 TracerPid0,Threads1, UID0, RSS412KiB; taint remains1. Authenticated Wi-Fi
SSH remains usable. This advances beyond the earlier ABI-only chroot.

Exact scope: global PID1 now Alpine; old Amazon processes and stock kernel
remain. No independent cold boot, kernel6.18 boot, modern Tailscale, clean
Amazon-free system or safe-rootfs-erasure claim. Guardian90s remains armed;
reset and stock return still under observation. Evidence saved on host before
reset in pid1-handoff/{trial-runtime.log,pid1-live-proof.log,pid1-proof.json}.


Recovery observation:44 matching AlpinePID1 proof samples through host90.33s,
last guardian remaining1s. SSH then stops responding. User initially reports
blank/frozen screen, then explicitly "đã về màn chính" (returned to home).
This supports successful real deadline reset/stock recovery for the unchanged
stock kernel userspace experiment. PMIC callback itself was not streamed;
post-reset bootID still pending. The old host SSH monitor socket did not close
promptly and was terminated locally after device home report; its exit code
is not evidence of the device reset cause. User reopened temporary-control
launcher; awaiting fresh pinned-key root connection. No stored boot asset or
Amazon rootfs removal occurred.

Fresh post-reset pinned-key rootSSH verified; new bootID be056fcd-6bef-4e6f-8608-e61933992a57 differs from cf4f90e2-0a26-4dbd-bd61-0bea9aed728c. UpstartPID1 /sbin/init.exe restored, stockkernel3.0.35, root disk ext3ro, RAMtrialroot absent, guardianmodule absent, taint1. Real reset and stock return now confirmed beyond user report. This qualifies the exercised unchanged-kernel RAMuserspace recovery, not newkernel/DMA handoff or recovery after Amazon rootfs removal.


## Narrowed target: native Alpine on the existing kernel — 2026-10-30

User accepts retaining installed bootloader/kernel and only needs nativeAlpine;
reader/Tailscale/WebUI are deferred in this delivery. Design and plan recorded
under ours/docs/superpowers/{specs,plans}/2026-10-30-stock-kernel-alpine*.md.
User's prior opt-out of repeated approval persists; no new design question.

Live checks: cmdline root=/dev/mmcblk0p1 ro; /sbin/init shell wrapper directly
execs /sbin/init.exe. p2 mountedrw at/usr/share/nh containsNh_* data, not a
usable diagnosticsroot. Both CONFIG_MXC_REBOOT_MFGMODE and
CONFIG_MXC_REBOOT_ANDROID_CMD are disabled; do not use reboot-fastboot/recovery
as a recovery promise. GPR/IDME mutation and Fastboot entry remain unperformed.
No current independentcable restore path established.

Separate native-rootfs host-only fullAlpine3.24.3 armv7 trialroot built,
64MiB ext3,SHA256 ce037e49f76976cc4b28e9daaea87222c7622c03a3b150b6a2955bc09d5af208. DirectBusyBoxPID1, apk3.0.8, stockUSBmodules,
90s guardian beforeUSBsetup, RAMtrialdirectories and readonlyroot requirement.
Actualimage e2fsck passes;530 supplied/root inodes ownershipchecked;8 files
extracted/bytecompared, initsymlink checked. apk executes in hostARM emulation,
not actual stockkernel apk compatibility proof. Earlier artifacts untouched.
Pre-guardian earlyboot failure still has no qualified recovery. No image/install
command copied to device; no p1/bootselector/kernel/bootloader change.

During this work SSH timed out and noUSBdevice appeared on host. Exactendpoint
observer retries only known192.168.1.200. User told to wake/reopen existing
controlsession; no speculative boot mutation. Installation remainsNO-GO until
independent USBrescue qualified, per accepted no-brick constraint.

FreshSSH returned on same post-reset bootIDbe056fcd. Full pinnedAlpine root copied only to tmpfs /dev/shm/gleworks-native-rootfs-89049ab24127; archivehashverified, actualstock3.0.35 chroot runs apk3.0.8-r0 and validates both startupscript syntaxes. PID1 remainsUpstart; noUSBmode/module/bootmutation. This closes fullrootfs/apkABI only. USBstorageLUN backingfile empty while unplugged; g_serial exact module present. Hostlsusb has noKindle; physicalUSBdata connection pending before USBverification.


## Exhaustive remaining routes and new reference — 2026-10-30

Goal active: nativeAlpine on Kindle and removeAmazonOS. Previous check yielded
new verified USBinterface evidence(storageonly), not bootprogress. Revalidated
current1949:0004 massstorage with mountedFAT; no serial/Fastboot endpoint.
New safeaction: publicsource search found ChrisPei/PaperOS, pinned8dc24d6;
staticbootstrap implements independent image+one-shot marker+pivotroot, without
changing storedkernel/bootloader. It changesp1init and retainsAmazonfallback;
not yet safe to copy/install onPW3. Its claimedKT3hardware result isn't ours.

A more useful nextgate than repeating currentUSBstorage checks is coldboot/error
path emulation of actualstock3.0.35. Acquired publicQEMU Wario/Muscat source
pinnedb1a5347 and enabledhostbuild. GitHubarchive lacks subprojectcontent;
resolved exactpinnedkeycodemapdb/softfloat/testfloat viaGitHubmirrors, no mocked
files/ABI. Pythonbuildvenv needed providedsetuptools/wheel. Compileinprogress.
No deviceboot/init/rootfs/IDMEwrites. Sourceaudit in PAPEROS-REFERENCE.md.

## Exact stock-kernel coldstart emulation — 2026-10-30

Public WarioQEMU b1a5347 built successfully (1993targets); exact captured
stock-main.uImage boots synthetic Muscat/512MiB, no physical device writes.
Important correction: captured kernel actually embeds a104-entry initramfs,
/init -> bin/recovery-util. GPL buildconfig alone did not reveal its content.
Extracted actual zImage gzip at0x462c, CPIO at inflated0x1f838. recovery-util
is stripped staticARM, not source-verified. Private extraction retained under
bootstrap/emulator; no actualdeviceidentity supplied to emulator.

Negative control direct-mmc.img had wrong p1size64MiB: recovery-util rewrote
MBR, erased first32768bytes ofp1,p2,p4 and formatted userdata, then exported
USB in FATmenu. This happened ONLY to syntheticdisk. Therefore preserving
exact stockpartitiongeometry is an installation requirement, not cosmetic.

correct-mmc.img uses3909091328bytes, starts65536/987136/1118208/1249280,
sizes921600/131072/131072/remainder, userdata nestedMBR+FAToffset8192.
Capturedstockkernel verifies MBR unchanged, skips absent updates, runs kinit,
mounts Alpine ext3p1readonly and executes /gleworks-direct-init asglobalPID1.
Serialproof: Alpine3.24.3, /proc/1/exe=/bin/busybox, Linux3.0.35-lab126,
apk3.0.8-r0. No Amazonrootfs supplied; initramfs vendorrescue remainsinkernel.
Evidence bootstrap/emulator/correct-serial.log. This is actual kernel execution
in emulated PW3, not physical coldboot, USBrescuequalification or goalcomplete.
Requestedinit override was used; untouched bootargs/defaults and failurecases
remain next. Separateoriginal native-rootfs artifact preserved.

Default-init matrix: absolute /sbin/init symlink fails stockkinit's pre-chroot
accesscheck; relative symlink succeeds with exact capturedstockcmdline and no
init= override. GPLklibc1.5.25 source find_init confirms chdir('/root') then
access('sbin/init') without chroot. Builder minimally corrected relativeinit
and root-owned /dev/console(5:1,0600),null(1:3,0666); originalimage untouched.
Corrected candidate-v2.ext3 e2fsck passes. Actualcandidate reaches guardian
reset at96.499s; observed-inittab copy gives stronger directevidence:
observed-kmsg-serial.log: sysinitRC0, /proc/1/exe=/bin/busybox, Alpine3.24.3,
state2/remaining90, /run/usb-ready=ready and all exactUSBmodulesloaded.
95.381s stockemergencyrestart follows. No physicalUSBproof fromemulator.
Freshreview_stage_bundle reviewed changes, found no mustfix; warned that
unobservedcandidate log alone provesrootmount, notfullstartup. Observedcopy
closes startup/guardian/driverreadiness evidence only.

Failurematrix: missinginit and corruptp1 panic, noautomaticUSBrescue. Invalid
update*.bin oncorruptp1 is examined BEFORE kinit but rejected atSP01header,
deleted, failurelogged and rebootrequested. Notarestorationproof. Recoverymenu
exportsuserdataFATonly. Do not triggerdestructiveMBRrepair onphysicaldevice.
Public official PW3sameversion recoverybundle URL returns200/270243071bytes;
rangeheader SP01/cert2 followedbyFB03, distinctfromunsupportedOTAbundles.
Hostdownload inprogress for emulator-only signedrestoration experiment.

## Official signed restore on failed roots — 2026-10-30

Acquired official HTTPS recoverybundle270243071bytes, SHA256
 e72dc88dc1b61406d0ef2c3ab65604a1e257eba3e140e8e02ba10737a808d869.
SP01cert2/FB03,target4097470002,platform7,code26Bmatches syntheticPW3.
Unmodified actualcaptured recovery-util ACCEPTS signature evenwhen p1mount
orversionreadfails. Both syntheticmissinginit and corruptp1 trials complete
restore, report bundlesuccess and requestreboot. They rewrite bootloader,
kernel ANDrootfs; no physicaluse authorizedbythis evidence. Kernelpayloadhash
matches exactcurrentcapturedstockkernel d99091ac...; no accountneeded.

Readback ofboth restoredroots yields identicalSHA256
88cc91944cfbd0f9cb60139a53ced0fee9442ab2ea99f0fd377097784dda7ef9.
Independently decoded signedbundle using publicKindleTooltable in/tmp;
compressedrootSHAa218da07... matches stockverifier, uncompressed460800000byte
rootmatchesbothreadbacks. etc/version.txt identifiesmuscat409747, stockinit
wrapperrestored. e2fsck-fn returns4: inode265lost+found has i_size16384 versus
17408. This issueexists inbyteidenticalsignedoriginal, notrestorediskcorruption;
no repairperformed. Evidence signed-recovery-review.json, official-bundle-
provenance.json and signed-{corrupt,missing}-serial.log underbootstrap/emulator.

Remainingrescuegap is concrete: afterfailedboot, ordinaryUSBmustprovideaccess
forplacingthatbundle andtriggeringstockrecovery. CurrentnormalUSBexportp4 and
serial-onlyembeddedmenu do notprovethis. Missing/corruptroot alonepanic.
Do notclaimphysicalrescueGO oreraseAmazonOS basedonemulatorrestore.

Builderitself rerun unchangedcode with separateartifactbase native-rootfs-v2-
emulator; rootimageSHA2560d400ac6e8085dd8db4fb5d9e7d7678f65dba3cfdd6267a6271284473f854671.
e2fsckpassed, actualrelativeinit androot-ownedconsole/null statchecked. Original
native-rootfs/.private imageandproofsretained. No newmaintainedtests added.

Latest physicalconnectivity recheck:1949:0004 storage, /dev/sda1 mounted at
/run/media/will/Kindle. PinnedSSHknownendpoint192.168.1.200:2222 returns
No route to host. No devicecommandsorwritesperformedthisexecution. Ordinary
massstorage access cannot execute the nextRAM/hardwareacceptance trial.
Currenthostproofs leave goalactive, installNO-GO pending physicalrescueentry.

## Minimal bootstrap rescue spike — 2026-10-30

Previousgoalturn wasprogress: exactkernel/defaultcoldboot and signedrestore
changed nextaction. PhysicalSSH remainsNoRoute whileUSBstorageisattached.
Ruling: explore an immutable smallp1 bootstrap withAlpine in ap4file, so failed
Alpineimage canfall back toUSB withoutAmazoninit/services. This remainswithin
nativeAlpine goal/retainedbootloader+kernel; host/emulatorspike only. No
persistentdeviceentry, install orAmazonrootfsremoval. p1/bootstrapcorruption
remainsseparate failureclass, not solvedby filefallback. Spikeplan bootstrap/PLAN.md.

StaticARM muslbootstrap nowcompiled Werror; --check works underqemu-arm,
nonPID1 invocation rejects. No devtmpfsassumption; earlytmpfsdevice nodes,
loop_info64ABI232, FAToffset8192, one-shotrequest consumedandsynced before
readonlyext3root. Missing/badimage fullyunmounted before g_file_storagep4
export; root/control detached via pivot_root withoutstockinit. Small rescuep1
containsourinit/USBmodules only. No forcedmoduleload or inventedphysicalMMIO.

Actualstockkernel WarioQEMU: no-request -> USBrescue driver boundtop4;
missingimage -> USBrescue; corruptimage -> USBrescue; validimage -> Alpine
BusyBoxPID1/3.24.3/sysinitRC0/guardianstate2/USBserialready. Deadline reset at
96.125s. FATreadback ofall3 confirms one-shotrequestabsent aftertrial. Separate
coldstart ofthe same validimagedisk afterdeadline isobservingrescuefallback.
Evidence bootstrap/rescue-emulator/*-serial.log. QEMUprints a UDC onlineevent
warning becausephysicalUSBhost/charger isnotpresent; binding/readymarker is
notphysicalUSBenumeration, externalhostfiletransfer or acceptedcablerescue.
Host-only source bootstrap/bootstrap.c and build-host.sh; hardwareunchanged.

Bootstrapafter-reset coldstart nowconfirmed: noone-shotrequest -> USBrescue
readymarker. host-review.json records4cases andexactinputSHA256. Hostcompiled
ARMstatic binary rejectsnonPID1, noELFinterpreter, loopstructABIassertpasses.
Bootstraphostspike succeeds withoutAmazonrootfs; physicalAlpine/nativeOSremoval
stillunverified. Pre-guardianAlpinefailure mayneedmanualpowerreset; markeris
alreadyconsumed. Executable/bootstrapp1corruption remainsseparate rescuegap.

Review/stopstate: requestedfreshbootstrapreview agent, but liveagentlisting
containsrootonly and mailboxwaittimedout; nofreshreviewresultreceived. Do not
callthisspikehardware-ready/review-approved. Ownseparatecodeinspection retains
explicitpre-guardian/manualreset andp1corruptionlimits. Rescueimageinitwas
extractedandbytecomparedtofinalcompiledbinary. Last inspectedhostemulator
stopped; disks/logsretained. USBphysicalstill1949:0004; nofreshSSHroute.

## Physical connection blocked audit — 2026-10-30

Previousgoalturn classifiedprogress: independent minimalbootstrap coldstart,
missing/corruptimagefallback and reset-to-rescue emulation were completed.
Currentturn revalidated physicalstate:1949:0004, mounted /dev/sda1 FAT;
pinned192.168.1.200:2222 exits255 NoRouteToHost. ControlsessionREADME confirms
SSH requires physical launcher and doesnotautostart afterreset. Sameconnection
condition recurred across three consecutivegoalturns; priorhostprogress is
retained, but nextmeaningfulacceptance requiresactualdevicecontrol/USBtrial.
No alternative callableexecutionchannel or livejob iswaiting. No p1/bootmode
mutation or erase justified bystorageaccessalone. Goalblocked pending safeUSB
eject/unplug and opening existing GleWorksAlpineSession. NativephysicalAlpine
andAmazonOSremoval remainincomplete; emulatorproofisnotcompletion.

## Actual device reconnected and USB runtime transition — 2026-10-30

PinnedrootSSH nowlive afterUSBunplug, samebootIDbe056fcd, UpstartPID1. Resume
blocked-audit resets; currentturn isprogress. New staticbootstrapSHA90d5d0...
executes --check onactualstockkernel and rejectsnonPID1, RAMcopyonly.
Baseline confirms p4FAT mounted /mnt/base-us via /dev/loop/0, FSP /mnt/us,
USBstorageLUN backingempty; g_file_storage loaded. Do not enter fullbootstrap
againstphysicalp4 whileoldstockFAT mounts/automounters stilllive: its freshboot
assumptions do not hold for arbitrarylivePID1 replacement. BinaryABI/USBdriver
checks are separatefrom physicalbootstrapcoldboot acceptance.

Preparedexactknown guardian/control + capturedg_serial/g_file_storage onlyin
/dev/shm. Firstprobe stoppedbeforearming because stockhotplug auto-created
miscnode betweencheck/mknod; trapcancelled/unloadedownidle guardian, noUSBchange.
Worker corrected toverify device major/minor onauto-created nodes. Second
actualtrial armed90s kernelguardian, normallyunloadedstorage, loadedserial,
createdverifiedttyGSnode and opened60s wiredproofwindow. No cableappeared on
hostduring85s observer. Workerclosedownserialreader, normallyunloadedg_serial,
restoredstockg_file_storage withcapturedbaselineparameters, cancelled/unloaded
guardian and removedownnodes. FreshSSH verifiesunchangedbootID/UpstartPID1,
rootext3ro, taintstill1, storageLUNempty, no g_serial/guardianloaded. Noreset,
forceoperation, p1/kernel/bootloader/IDMEwrite. Evidence bootstrap/device-
{baseline,abi,usb-parameters}.log, usb-runtime-return.log, device-runtime-review.json.
PhysicalUSBnonce and independentcablerecovery stillunverified. Userreadiness
question pending for thenext60s cablewindow; no blindautomaticretry.

## Cable coordination blocked audit — 2026-10-30

The actualUSB-runtime turn madeprogress (moduletransition/return andbinaryABI)
but itsphysicalproofwindow endedwithnocable. Same missingphysicalconnection
remains on twosubsequentgoalturns: lastfreshcheck lsusb1949:/0525:a4a7 bothempty,
pinnedSSH stillroot onUpstart/bootIDbe056fcd. Readinessquestion remainsunanswered.
Thus condition recurred onthreeconsecutivegoalturns sinceUSBtrial; no live
observer/armedguardian/resetjob iswaiting and no blinddriverretry isjustified.
Nextmeaningfulacceptance requiresuser atdevice toattachUSB duringserialwindow.
Host/emulatorwork and actualruntimeproof retained; nativecoldboot, externalUSB
nonce andAmazonOSremoval remainunverified. Goalblocked onphysicalcoordination,
not permission; userauthorization remainsvalid.

## Physical USB proof and pretrial backup — 2026-10-30

USB ordinary cable now present. Normal host filesystem unmount followed by
UDisks Drive.Eject restores pinned SSH with the cable retained; no raw host
writes or power-off command. Physical g_serial challenge/response succeeded
under stock Upstart: bootstrap/physical-usb-serial-proof.json. Normal module
cleanup restored g_file_storage; boot ID remained be056fcd and taint stayed1.
This is physical transport evidence, not independent bootstrap rescue.

Full current readonly p1 captured before any persistent mutation:
bootstrap/.private-pretrial-backup/p1.ext3,471859200bytes,
SHA256 c33cdeb6fbcd832fd49adac6ec1467dfa3ea67cb014e482201b62e34a862d94a;
e2fsck -fn returned0. Exact original init wrapper SHA256
ad624cbad3090bf15e65c8420a4e451ee10fd067a40b1bfe662965e82d932d0a.
Original dispatcher emulator covers default stock, missing bootstrap, Alpine
PID1/reset, corrupt image; subsequent coldstarts select stock after request
consumption. Artifacts bootstrap/dispatcher-emulator retained.

Fresh reviewer review_coldboot_dispatch identified: pre-guardian stalls,
missing concrete atomic installer/default-stock coldboot gate, and barrier=0
power-loss durability limits. No claim of review approval. Ruling: move the
90s guardian into supervised bootstrap before FAT/image/USB operations; reboot
on explicit early failures; Alpine adopts the already armed guardian. Preserve
all Amazon files and require default-stock coldboot before arming a trial.
Consumed request protects normal reset selection, not arbitrary power loss or
p1 corruption. Those remain outside demonstrated recovery. No new maintained
tests: existing exact-kernel emulator exercises fresh disposable image copies.
Physical p1 is still unchanged at this entry; live SSH verifies original PID1.

## Supervised stock-default entry review — 2026-10-30

Updated exact-kernel emulator: early guardian marker precedes Alpine exec and
corrupt-image rescue; real90s deadlines fire. Missing guardian explicitly
reboots. Readback shows try.once absent/try.used present, subsequent boots for
valid/corrupt/missing-guardian select original stock wrapper. Fresh reviewer
accepted early guard logic and resolved physical-reset concern against
pid1-handoff/pid1-proof.json. Installer review found unchecked temporary or
dangling symlink destinations and preexisting return token. Fixed absence+
no-symlink checks, actual-device preflight and staged manifest pass.

Ruling: perform only stock-default atomic dispatcher entry next, keeping the
entire Amazon root and exact original wrapper. This exception to the earlier
blanket p1 hold permits a bounded, reversible wrapper change; it does not
permit erasure or rely on arbitrary-p1 rescue. Detailed procedure now in
bootstrap/PLAN.md. A hash-pinned existing 1h host-restricted control launcher
can run once after stock return, gated by a separately created p3 token and
180s wait. No standing new network daemon. This enables authenticated return
observation without asking user to reopen the launcher on every reboot.
Current exact original init hash/bootID, rootro, pinnedSSH and battery100%
reverified. Full p1 backup retained. Neither p1 nor FAT boot request created.
Remaining limits: short pre-guardian setup stalls, p1 corruption and abrupt
power loss are not fully covered. Installation is not native boot acceptance.

Physical stock-default dispatcher installed successfully. device log and full
readback: bootstrap/default-install-{device,readback}.log. Original wrapper
retained exact SHAad624c..., dispatcherSHA79a269..., bootstrapSHAab7b29....
Root remountedro, PID1 still /sbin/init.exe, no try.once. One-use return job
registered stop/waiting. Created only the p3 return-control token and requested
normal /sbin/reboot; host observer is waiting for changed bootID/Upstart and
safe ordinary-USB eject. No Alpine request and no bootloader/kernel writes.
Fresh observed-Alpine emulator sysinit returns0, PID1/bin/busybox, existing
guardianstate2; deliberate pre-sysinit stall is reset by early guardian.

## Physical default coldboot accepted; native trial requested — 2026-10-30

bootstrap/default-coldboot-proof.json: authenticated Upstart after bootID
be056fcd ->779d08b4, unchanged installed hashes, rootro, trial request absent.
One-use return job consumed p3 token and relaunched original bounded control
session. Host normal unmount/UDisks eject restored Wi-Fi with cable retained.
Updated emulator includes observed sysinit0/PID1BusyBox/rootdetached/guardstate2
and deliberately stalled sysinit; both reset then coldboot back to stock.
bootstrap/dispatcher-guard-emulator/review.json records exact candidate hashes.

Physical64MiB Alpine image staged via temporary filename and hashverified on
FAT; bootstrap/native-physical-trial-inputs.json records inputs. Final staged
request is a single supervised native coldboot with90s guardian, preserved
original stock wrapper and one-use return session. Separate host observers
wait for exact g_serial fresh nonce and subsequent changed-bootID stockreturn.
No erased Amazon files or bootloader/kernel changes. Do not claim nativeboot
until the actual USB response arrives, or safety return until authenticated.

## First physical coldboot trial returned safely; USB visibility unresolved

Native trial1 did not enumerate external g_serial. Its90s guardian reset and
one-use return session yielded authenticated stock bootID04fe0928, original
Upstart/rootro; p1 and FAT requests consumed. bootstrap/native-coldboot-return-
proof.json and native-coldboot-request.log retained. Prior kernel ring recovered
read-only from /proc/printkbuf (stock printk.c backs previousboot memory):
bootstrap/.private-native-trial1-printkbuf.log. It shows loop1 ext3 root mount,
guardian, g_serial ready/bound, boot cable-detect event, then real deadline.
These narrow failure to early userspace/USB visibility but are not external
Alpine PID1 proof. No blind repeated image rewrite or OS erase.

Diagnostic image v4 adds fresh nonce, globalPID1/root/bootID/mount/process and
USB-state journaling on the already mounted FAT. Reviewer found EHCI fallback
could explicitly enter hostmode: removed entirely before physical arming;
unknown/unreadable USB state now stops mutations; pre-USB snapshot added.
Only optional documented device B-session helper5off/on remains after10s,
conditional on successfully read connected=0. Actual image was staged only,
no new boot request. Emulator revealed BusyBox readlink accepts one operand;
split exe/root reads before using this diagnostic. Disposable logs/artifacts
retained; current corrected image must pass emulation and hash restaging before
second physical request. Previous source-record labelled PREREVIEW, not final.

Diagnostic emulator exposed vendor-kernel/musl proc-write incompatibility:
writev -> do_loop_readv_writev forwards zero-length first iovec withNULL base
into proc_file_write -> asession_write -> sscanf(NULL). Oops atvsscanf+0x40;
trace retained native-rootfs-v4-diagnostic/proc-writev-oops-serial.log. Source
musl-1.2.5-source/src/stdio/__stdio_write.c constructs this empty first vector;
stock fs/read_write.c does not skip empty vectors and usb_dr.c parses buffer
without validating count/copy_from_user. This was not executed on hardware.
Ruling: no kernel patch or arbitrary register writes; bounded retry writes
fixed5-byte NUL-terminated commands via BusyBoxdd/write(2), with temporary
input onRAM. This specifically avoids the reproduced zero-vector path.
Updated diagnostic reruns on the stopped synthetic disk; current physical
image remains unarmed until corrected image passes and is restaged/hashread.

## PHYSICAL ALPINE NATIVE COLDBOOT VERIFIED — 2026-10-30 08:33+07

Authoritative native-rootfs-v4-diagnostic/physical-coldboot-proof.json and
physical-coldboot.log: pre-stockboot04fe0928 -> nativeboot1490f015 -> returned
stockboot706cae53. Fresh host trial nonce daa6482bb851cf5fc013e06adaaac0d9
matches the staged diagnostic. GlobalPID1 executable/bin/busybox, PID1root/,
Alpine3.24.3, root/dev/loop1 ext3readonly, old-root mount absent, sysinit0;
process snapshots show kernelthreads and our Alpine code, no Amazon init/java/
powerd. This is actual coldboot through retained bootloader/kernel, not chroot
or livePID1 takeover. The native kernelbootID differs from both neighboring
stock boots. Exact image/journal hashes recorded in proofJSON.

Hardware external USB serial remains false; guardian reset/stock Upstart
return authenticated via pinnedSSH. bootstrap/native-diagnostic-return-proof.json
records installedhashes/rootro and both requests consumed. Host observer's
PATH lostgdbus; this terminal FileNotFoundError was fixed to an absolute known
store path, safely ejected actual unmounted Kindle drive, then SSH returned.
No device retry or reset caused by that host-tool failure.

USB physical journal before/after documented Bsession5off/on: VBUSconnected1,
USBhost1, OTG0, clocks both0, USBCMD00080000(runbit0), USBMODE0000000a(device).
Bsession values change but no enumeration. Fixed write2 commands complete;
no reproduced procwritev NULL issue on hardware. Source-only theory is now
replaced by captured physical state. Next diagnosis: dynamicdebug in actual
arcotg_udc/fsl_otg/usb_dr paths; /sys/kernel/debug/dynamic_debug/control exists.
No EHCI/host-role fallback authorized by this proof. All Amazon files remain
for recovery, persistent default is still stock, no pendingtrialrequest.
Full goal remains active: nativeboot milestone achieved, independent cable
rescue, durable Alpine default and Amazonuserspace removal not achieved.

## Physical native Alpine AND external USB nonce verified — 2026-10-30 08:45+07

V5 adds dynamicdebug and exact stock empty removable g_file_storage, verifies
no backing file, waits4s, normally unloads it then binds g_serial. No hostmode
or storage export; emulator sysinit0/noOops/reset passed. Hardware
bootstrap/native-v5-usb-proof.json contains exact Alpine3.24.3/stockkernel/
BusyBoxPID1 response to fresh host nonce b63e77f23f6b359f1dde782b1026f4fc.
Expected full response was matched, not the echoed nonce prefix. Host sees
0525:a4a7 ttyACM0. Then physicalguardian reset and authenticated stockreturn
bootID63480630 in bootstrap/native-v5-return-proof.json. V5root journal/kernel
logs fetched over pinnedSSH to native-rootfs-v5-diagnostic/physical-*; reviewJSON
records physical acceptance and hashes. Current PID1 stock, rootro, no request.

Dynamicdebug explains bind timing: first legacy gadget resume reports stopped1
suspended1 and enterslowpower before boot cable simulation; subsequent serial
resume sees Bsession already present and immediately leaveslowpower. Thus use
the documented device-cable Bsession helper BEFORE binding first rescue gadget,
with full cable/USBhost checks and write(2), not muslstdio. Source updated in
bootstrap.c and compiledSHA04257c56...; not installed yet. Rescue prototype USB
ID0525:a4a5 distinguishes our helper from stock1949:0004, avoiding a false
rescueproof if an observer only catches the later stockreturn.

User explicitly chose permanent Alpine after USB/recovery qualification.
Next supervised gate: intentionally invalid64MiB image while retaining V5,
bootstrap rescue exports fully unmounted p4, host verifies unique rescueUSB,
writes nonce and repairs image, safeeject before90s, then verify stockreturn
and roundtrip data. Keep originalinit/fullp1 backup until this accepted.
AmazonOS erasure/permanent Alpine still incomplete; goal remains active.

## Independent rescue qualification gate — 2026-10-30

Reviewer accepted export ordering/Bsession helper but blocked proposed host
64MiB repair: enumeration-relative40s did not bound90s guardian, blockingIO,
and identity reuse. Ruling: do not run that repairtool. Renamed explicitly
rescue-host-repair-NOT-APPROVED.py. First physical rescue is enumeration only
using unique0525:a4a5, immediately unmount auto-mounted partition, no hostwrite.
Actual repair remains an unmetgate requiring its own adequate recoverywindow.

New bootstrapSHA04257c56... compiled, actualCPU --check passes, emulator corrupt
image->USBrescue->guardianreset. Installed atomically while stockPID1: prior
bootstrap preserved as bootstrap-v1-preserved, rootro/readback. Originalinit
andstockroot intact. Known64MiB invalidimage staged withhash; provenV5 image
preserved ondevice as system-v5-preserved.ext3. No hostmode drivers.
Single90s rescue trial requested with independenthost enumeration/stockreturn
observers; outcome pending. Do not infer USBrescue from stock1949:0004.

## Physical invalid-root rescue enumeration accepted; reset-safe repair design

Distinct0525:a4a5 physical rescue enumerated and host unmountedwithoutwrites.
Actualpreviouskernel confirms invalid loop1ext3, g_file_storage backingp4 and
highspeedconfig. Then normal90sguardian reset -> authenticated stockreturn
bootIDc1b4053e. native-rescue-return-proof.json and physical-usb-rescue-
enumeration.json retained; real recoverybranch works independently ofAlpine.

New repairwindow closes reviewer timer concern by ordering: empty0525:a4a6
removablegadget(no file) while90sdeadline active, requireconnected1 and
remaining>=15, cancelverifiedstate0, normalunloademptygadget, THEN exportp4 as
0525:a4a5 bcdDevice0200. Host accepts exact0200only. No automaticreset during
hostFATwrites. SCSIeject closesbacking -> bootstrapreboot, no simultaneousFAT
mounting/polling. NewbootstrapSHA d10b3d..., dispatcher117503... compiledhost;
notinstalled yet. Disposable compiled source-inclusion behavioralprobe verifies
cancel-before-export, exportblockedoncancelfailure and eject->reboot; mocked
IO is not physicalacceptance. Reviewer requested, no approvalinferred.

Repairwindow review found no critical cancellation/export sequencingdefect;
mutex makes CANCELrace failclosed if deadline alreadyfired. Identitychecks
added before finallycleanup and compareUSBdevnum to rejectreusedsdnames.
Add2s after backingclose before reboot to let SCSIstatusack reachhost.
FinalbootstrapSHA32ead45c..., dispatcher117503..., installedatomically with
priorbootstrap/dispatchercopies retained; originalinitSHAad624c... unchanged.
ActualCPU--check/stagedhashes/rootro checked, no trialarmed duringinstall.

Emulator nohosthandshake stays empty(no p4export) and reboots at82.84s when
remaining<15. Source-inclusion behavioralsimulation covers successfulcancel,
export and eject->reboot; cancellationfailure neverexportsp4. Physicaltrial
nowrequested with signed-by-known-inputs(notcryptosigned) exacthashes and
one-use specialmarker; hostrepair accepts0525:a4a5release0200only, verifies
knowninvalid64MiB hash, writestemporaryverifiedV5root thenatomicreplace,
nonce+fsync, hostunmount/eject. No automaticdeadline exists in finalexport
bycodeordering. Prior unsafe90srepairscript remainsNOT-APPROVED/unexecuted.

## PHYSICAL USB RECOVERY / REINSTALL ACCEPTED — 2026-10-30 09:06+07

Actual handshake appeared0525:a4a6empty then0525:a4a5bcd0200. Hostrepair
initialprocess stoppedterminally because UDisks hadnotregisteredblockyet;
nohostwrite before thaterror. RevalidatedexactUSB0200/partition andstarted
hostrepaironce more (no newdeviceboot). FullknowninvalidimageSHA3b6a07... read,
new64MiB V5imageSHA2e912f... written toexclusivepartial, hostfsync/readhash,
atomicreplace andfresh noncef223b2... file fsynced/readback; unmount/eject
completed. Bootstrapejectreboot -> authenticatedstockboot83ab733a.
SubsequentpinnedSSH independently verifies repairedimageexacthash andnoncefile,
originalstockPID1/rootro andno pendingrequests. physical-usb-rescue-repair-
proof.json nowstock_return_roundtrip_verified=true; device-roundtrip.log and
native-rescue-repair-return-proof.json. This establishes ordinaryUSB recovery
frombrokenAlpine independentofAmazonuserspace with actualimage reinstall.
It doesnotestablishrescuefromcorruptp1/kernel; bootloader/kernel untouched.

Userdecision permits permanentAlpine after these gates. Newhostcandidate
native-rootfs-persistent has physicalUSB adminconsole (STATUS/SHELL/REBOOT/
RESCUE), no normal90sreset, preservesexistingtrialguardianifoneexists, known
emptygadget->serialsequence. No networkdaemon. Sourcebootstrap adds exact
boot.enabled token forpersistentroot, opensimageRW and mountsrootRW onlyin
thatmode; one-shot remainsreadonly. Missing/badimage returnsownUSBrescue;
normal(no-trial) rescuehas noautoreset and rebootsoneject. Candidatebuilt/fsck/
shellpass, emulatorstartedwithstaticbootstrapdirect/sbin/init; no hardware
persistentenable orAmazoneraseyet. Fullgoalremainsactive.


## Persistent Alpine cutover in progress — 2026-10-30 09:28+07

Final bootstrap SHA b4aa162ffc2972a59a3c649d15b600c90e495e5025f6c29bada7798acb0b7175;
initial clean Alpine image SHA 8a12bf19f979facbd06e4c6db0fb8cfdc82cf26ac018408bcd18e8e6be4f61fc.
Reviewer startup-loop, failed-console recovery and ext3 journal concerns fixed:
keep boot.pending throughout uptime, remove only after clean ext3 unmount;
reject dirty/recovery-required ext3 before mount; arm startup guardian, cancel
before root RW. Existing readonly supervised trial never falls back to RW.
Shutdown initially failed EBUSY in emulator because inherited console file
references retained Alpine device inodes under later tmpfs; close/reopen all
console descriptors after entering shutdown RAM. Existing emulator normal,
exec-failure, early-failure and dirty-root cases verify the applicable paths.

Physical readonly trial boot9959ef9f returned fresh USB nonce, global Alpine
PID1 and readonly loop root. Orderly reboot requested and actual FAT journal
readback proves Alpine root cleanly unmounted; authenticated stock return
boot05fec872 verified. Original full-p1 backup hash rechecked.

User authorized permanent after recovery qualification; USB image reinstall
was physically accepted earlier. Atomically replaced /sbin/init with static
bootstrap, preserved stock wrapper/dispatcher/full-p1 snapshot. Exact boot.enabled
created. First actual default persistent boot0b196b96 enters Alpine PID1,
root RW, guardian state0, pending retained, physical USB administrator shell.
No Amazon init/services start in this default path. Amazon p1 files preserved;
no bootloader/kernel changes or p1 erasure. Uptime/reboot/reset-rescue acceptance
still being collected; see native-rootfs-persistent/README.md and evidence.


## PHYSICAL PERSISTENT ALPINE BOOT ACCEPTED — 2026-10-30 09:31+07

Default boot0b196b96 reached global Alpine3.24.3 BusyBoxPID1, loop1ext3RW,
guardianstate0 and uptime117.85s. Wrote fresh nonce into /etc on actual root.
Orderly RAMshutdown -> second default bootf62e0680 read back same root nonce,
RWroot and clean-unmount journal. Deliberate abnormalreset was bounded by
sync+verifiedreadonly root; boot.pending remained. Independent USB0525:a4a5
bcd0200 then exported unmountedp4. Host independently inspected ext3 clean
state/no journalrecovery and durable/etcnonce with readonlydebugfs, removed
pending, fsyncedFAT, unmounted/ejected. Final default native boot e1771f04-
eb0a-49e1-b3ed-88c03498f4a1 verified globalBusyBoxPID1, RWroot, originalnonce,
guardianstate0, enabled+pending and noAmazonuserspace executables.

review.json physical_verified and physical-persistent-reset-rescue-proof.json
record acceptance. NativeAlpine is now the persistent default, without launching
Amazoninit/JB. Bootloader/vendor kernel unchanged. Amazonp1files, exactstockinit,
dispatcher andfullp1backup are retained for rollback; this is OSruntime
replacement, not p1erasure. Dirtyimage repair is supported by recovery design,
while physical abnormalreset test deliberately used a cleanreadonly root.
Reader/EinkUI, WiFi/Tailscale, HTTPupload andlowpower policy remain separate
unimplemented appliance work. Native administrator console uses USB; orderly
reboot must use consoleREBOOT or BusyBoxkill-QUIT1 as documented.


## NATIVE ALPINE READER — 2026-10-30

The earlier reader/display limitation is now resolved for the minimal local
reader. Missing framebuffer/input nodes and unloaded cyttsp4 modules caused
the retained boot image. Native launcher creates nodes, loads the exact stock
touch modules without firmware loader, and starts KOReader v2026.07.1 directly
on framebuffer under Alpine PID1. Generic glibc compatibility libraries only;
no Amazon framework, LIPC integration, account or jailbreak runs in this path.

Final clean release image: native-display/release/alpine-reader-v2.ext3,
268435456 bytes, SHA256
5602b47427f34a9b04d6517bf22a390fee030e35da490242253a508f975fa2ef.
Physically installed with hash readback through the independent USB rescue;
previous images preserved. Vietnamese library and EPUB pages were inspected
from framebuffer; user explicitly confirmed “Mở sách và chuyển trang được”.
Automatic reader restart, clean reboot, and one-command document copy through
rescue/hash/eject/native-return were exercised on this PW3. Runtime inspection
is kept under native-display/release/. Image review records input hashes.

Native-kit/README.vi.md, REVERSE-ENGINEERING.vi.md and OPERATIONS.vi.md now
provide the executable capture/build/trial/install/maintenance path and full
reasoning/evidence limits. kindle-linux.py automates the qualified-device USB
maintenance path; initial stock migration still requires actual per-device
recovery acceptance. Source package excludes keys, device captures, books and
full p1 backups; optional image is the clean host release, never the live root.
Wi-Fi/Tailscale/web upload and suspend remain unfinished future work.


### 2026-10-30 — reader v4 cleanup and host-only research history

Removed legacy shared-storage apps, books, probes, guides, logs and redundant
images after host archival and hash checks. Final userstore free space is
2,996,682,752 bytes; documents is empty. Only system.ext3, boot.enabled and
boot.pending remain in the Alpine control directory. Boot/reader logs use
/run tmpfs. The proven shutdown helper retains one transient handoff record,
removed at the next Alpine boot; the safety markers remain required.

Amazon p3 was rebuilt from its firmware factory templates, preserving verified
eink waveforms and device guid. p1 firmware and its original init wrapper remain
intact for supervised rollback. No fresh Amazon fallback boot or reduced Amazon
firmware image has been validated. Original factory_reset must not be used: it
would erase the Alpine userstore as well.

The v3 shutdown change failed physically and required a button reset; cause is
unconfirmed. v4 restores the byte-identical previously proven shutdown helper.
Physical v4 load, independent USB mass-storage rescue, clean offline root check,
and return to Alpine PID1/KOReader succeeded. Final boot ID:
f0600849-2199-4d16-8797-7311ac08f040. Framebuffer shows the empty reader library.
User book/page-turn acceptance remains the earlier v2 result, not a new v4 test.
Evidence: native-kit/.private-cleanup/final-storage-cleanup.json,
v4-final-runtime.log, amazon-state-cleanup.log and final-device-history/.
Clean release SHA256: 927e9ab68fa9677e18d35691e226bd9ad519b44e666aa275d9ea6c3d3eb3d3eb.


### 2026-10-30 — first measured bloat reduction

Native reader RAM baseline: ~42 MiB excluding reclaimable buffers/cache,
KOReader RSS ~20 MiB. Removed 59 unused UI translations only after exact
live/host/archive SHA256 comparison. Retained Vietnamese, UK English, built-in
English and licenses. l10n usage: 18,335 -> 975 KiB (17,360 KiB reclaimed inside
root). No boot/kernel/recovery or font changes, no forced cache flushing.
Root image remains 256 MiB; FAT book capacity does not increase from this step.
Source image builder now applies the same locale selection. Existing v4 package
is historical and does not include these later live/source changes.


### 2026-10-30 — independent-session documentation handover

Added native-kit/CURRENT.vi.md as the current state/evidence/operations index,
root AGENTS.md as the session entry, and linked the historical appliance/stage
plans to that index. Reconciled README and ACCEPTANCE with live/source changes
after the frozen v4 package (GC16/page refresh and locale trimming). Preserved
all historical plans, captures, backups and archives. Documentation-only work;
no device write, reboot, new runtime qualification or release rebuild.


<a id="history-002"></a>

## Nguồn lịch sử: ours/APPLIANCE-PLAN.md

Current expansion: [EXPANSION-PLAN.md](BACKLOG.md#history-003) records the integrated
Calendar/Todos, Terminal and offline complete Update Apps implementation. Source
and disposable probes are complete; target acceptance is pending user-run USB
copy. Reader refresh is now 4/4. No domain update server has been deployed.

# Controlled reading appliance — 2026-10-29

## Agreed direction

Retain the stock Linux kernel, drivers, power management, and jailbreak. Replace
the previous third-party application layer with our own controlled tools. Keep
KOReader and official Tailscale as upstream dependencies. The user authorized
planning and building this direction; no further design questionnaire is needed.

## Architecture

- A small Lua HTTP service runs under KOReader's Kindle-compatible LuaJIT and
  bundled LuaSocket/LuaFileSystem. It replaces the incompatible Go prototype as
  the device target. Linux openat/O_NOFOLLOW confines leaf-file access; all
  names are single components. Native module loading and APIs must be checked
  on the actual device before migration.
- Books go to `/mnt/us/documents/GleWorks`; general files go to
  `/mnt/us/gleworks/files`. Only authenticated listing, download, and upload are
  supported. No remote shell, app installation, deletion, or arbitrary paths.
- Token and fresh Tailscale enrollment live in `/var/local/gleworks`, not the
  USB-exported partition. The service binds only to 127.0.0.1:8080.
- Transfer and VPN sessions are explicitly started/stopped. Default behavior
  does not disable sleep or create boot hooks. The usual reader is KOReader;
  stock reading remains a fallback. No framework/driver deletion.
- Host-side preparation downloads upstream releases from official HTTPS origins,
  validates published digests, and records source URLs/versions/hashes. The device
  does not download code. No domain/DNS/public deployment changes are included.
- `kindle.gleworks.io.vn` is intended for private tailnet access; optional HTTPS
  requires separately configured private hosting/certificates.

## Work and acceptance

Reader responsiveness target: **balanced speed and clean text**, selected by the
user. Use the reversible settings and device comparison in
[READER-PERFORMANCE.md](technical/READER-PERFORMANCE.md). No measured improvement is claimed
until that comparison is performed on the Kindle.

- [x] Implement Lua file server and preserve the self-contained UI; native probes
  pass with socket/LFS substitutes. ARM/runtime acceptance remains open.
- [x] Replace the device launcher with reader, transfer, VPN, and status controls.
- [x] Build host-side verified staging and a device preflight/install/rollback
  script. Never remove the previous apps before replacement preflight succeeds.
- [x] Provide library scriptlets, reversible quarantine, and upstream ownership records.
- [x] Validate syntax, compilation/loading where available, integrity packaging,
  and CLI behavior. No maintained tests are added, per user instructions.
- [x] Document installation, reader customization/backup, network boundaries,
  rollback, and exactly which device behaviors remain unverified.

## Deployment acceptance — still pending

- [x] Download and verify actual official release archives from a networked host.
- [x] Stage and read back the actual release bundle; install on the ejected device.
  User-run installation succeeded; reconnection verified all 1,048 files.
- [ ] Verify ARM module/API loading, VPN enrollment/connectivity, HTTP requests,
  reader lifecycle, balanced refresh behavior, sleep/wake and reboot on hardware.
- [ ] Restrict effective tailnet policy and verify unauthorized clients are blocked.
- [ ] Revoke old VPN identity and remove secret-bearing quarantine after acceptance.
- [ ] Optional: configure private DNS for the domain after verifying the new IP.

## Implementation rulings

- The original `.git` is inaccessible/empty, so work stays within the isolated
  `ours/` tree without worktree/commit operations. Original root scripts stay untouched.
- User instructions prohibit new maintained tests. Disposable native probes live
  only in `/tmp`; syntax checks and operational preflight remain part of setup.
- ARM32 has different no-follow/directory open flags from x86_64; select by
  `ffi.arch`, and reject unhandled platforms.
- Rollback restores only retired KPM rows from a consistent SQLite snapshot;
  a whole-database replacement would overwrite unrelated changes. A durable
  mutation marker covers interruption immediately after COMMIT.
- Fresh official KOReader is managed by us. Its old KPM installed record, package
  wrapper and launcher are quarantined to prevent stale ownership/auto-updates.
  Data preferences are sandbox-read and serialized; executable default overrides
  and old plugins are not imported.
- Official runtime startup on Linux 3.0 remains a device acceptance requirement,
  especially for Tailscale. Build provenance is not compatibility proof.

## Safety boundaries

No reset, OTA restoration, firmware/kernel flashing, bootloader work, automatic
updates, public ingress, or arbitrary command endpoint. Root execution is via a
user-run reviewed scriptlet because this session can only read the device.
The verified USB backup exists; it is not a system-partition recovery image.
The old Go source may remain as a host reference but is excluded from the device
bundle. A chroot is not needed and would not replace the kernel.

## Review focus

Authentication, malformed HTTP framing, symlinks and traversal, partial uploads,
disk exhaustion, resource limits, process identity before stopping, keeping
credentials private, safe upstream archive extraction, and rollback consistency.


<a id="history-003"></a>

## Nguồn lịch sử: ours/EXPANSION-PLAN.md

# Complete appliance application expansion

User authorized one complete rollout before hardware troubleshooting, with
one-click app updates, an on-device terminal, and local offline calendar/todos.
No ebook/update server exists on gleworks.io.vn yet. No hosting is inferred.

Design: extend verified KOReader with one lightweight GleWorks plugin. Reuse
its existing PTY terminal instead of shipping GTK/kterm. SQLite stores personal
calendar/todos outside application folders; draw only on user interaction.
Use manual, complete, host-verified USB bundles for Update Apps now. Online
release hosting remains unavailable until the user's planned server exists;
the button must clearly report missing staged updates rather than fake success.

Alternatives: standalone GTK apps add another runtime; a browser dashboard adds
a service for offline planning. KOReader widgets and its bundled terminal fit
the existing lightweight reading appliance. This is the implementation choice
following the user's instruction to deliver the whole set without more small
rollouts or design checkpoints.

- [x] Offline planner storage: validated dates/times/titles, parameterized SQL,
  CRUD for events/tasks, completion/due dates, bounded lists, no background timer.
- [x] Native KOReader menu/calendar/task editor, terminal shortcut and library
  launchers. Keep reading lifecycle and return-to-library behavior intact.
- [x] Update Apps applies one fully staged package of all managed apps only
  after the reader closes; verify all hashes before mutation, retain prior app
  files and user state, stop services, and retain interrupted-update records.
- [x] Extend the single host updater to deliver all own files/plugin/launchers,
  preserve personal preferences, and record one complete revision manifest.
- [x] Run disposable storage, UI-callback/lifecycle, health/export, update failure
  and integration probes. No maintained tests, per user instructions.
- [x] Freeze the complete rollout, update the guide and report limits. One host
  copy and one hardware acceptance cycle; no public deployment/DNS changes.

Personal data: /mnt/us/gleworks/data/planner.sqlite3 (back up with books; USB is
not private from the computer). No accounts, remote sync, alarms that wake the
device, public shell, calendar daemon or automatic network update checks.

## Delivery evidence and boundary

Implemented and locally probed all six items. See VERIFICATION.md for exact
adapters and hardware limits. Actual installed feedback patch has refresh 4/4.
Independent final review findings were repaired: preactivation recovery, missing
active-code restore path, extra empty directories and incomplete required files.
Decision: keep offline trusted USB bundles until actual hosting exists; do not
add a downloader, signing infrastructure or domain deployment speculatively.
User's no-new-tests instruction takes precedence; probes are disposable /tmp files.
Existing ours/ isolation retained; no git repository/commit or maintained tests.

- [ ] User-run integrated USB copy, then real device acceptance of all apps.
- [ ] Complete target bundle apply/restore and sleep/wake/reboot acceptance.
- [ ] Future online update hosting (unavailable; not part of this local rollout).


<a id="history-004"></a>

## Nguồn lịch sử: ours/PLAN.md

> Historical Go prototype plan; superseded for the device by [APPLIANCE-PLAN.md](BACKLOG.md#history-002). Go binaries are excluded from installation.

# GleWorks Kindle implementation

Build requested by the user: our own file server and launchers, official
Tailscale, configurable KOReader, and hosting under gleworks.io.vn.

## Constraints

- Local development only; preserve existing scripts and device contents.
- No third-party Go dependencies, CDN assets, telemetry, or automatic downloads.
- No maintained tests added, per the user's instructions. Use available compiler,
  vet, shell syntax, and disposable command-line checks.
- No firmware/reset/uninstall commands until exact model and recovery are known.
- No DNS changes or public deployment in this task.

## Implementation

- [x] Implement loopback-only authenticated HTTP file listing/upload/download.
  Restrict access to two explicit roots; never overwrite existing files or
  expose shell commands. Bound uploads and reserve disk space.
- [x] Add a self-contained browser page with an in-memory access token.
- [x] Add manual service/reader launchers and an offline ARM build command.
  Tailscale credentials stay outside USB storage. Nothing starts at boot.
- [x] Record baseline recovery evidence, kernel compatibility blockers, and
  private-domain hosting instructions.
- [x] Run formatting, vet, native and ARM builds, shell syntax checks, and CLI
  checks. Record unverified network/device behavior separately.

## Review focus

Unauthorized requests, cross-origin writes, symlink/path escapes, truncated and
oversized uploads, disk exhaustion, hostile filenames, duplicate uploads,
and process PID reuse. The supported API has no deletion or execution endpoint.

## Status

Local implementation and available checks complete; see VERIFICATION.md.
Model/firmware identified: Paperwhite 3 (7th generation), 5.16.2.1.1.
Device deployment and baseline rebuild remain blocked on root/recovery
inspection and kernel compatibility. There is no usable Git repository in this
workspace, so this directory provides isolation without modifying inherited files.


<a id="history-005"></a>

## Nguồn lịch sử: ours/VERIFICATION.md

# Fixed popup and centralized errors — 2026-10-29

Current revision: [NOTIFICATIONS.md](technical/NOTIFICATIONS.md). This supersedes the older
plain framebuffer notifications and dispersed operation-log locations described
below. The integrated updater now carries 41 managed files. Disposable probes
cover all 13 launchers, popup/layout/fallback, secret exclusion, persistent
journal rotation and Inspect collection, native callback/control/HTTP failures,
and update/restore retention. Device rendering and live connectivity remain
pending the user-run host update. The journal contains metadata, not raw errors.

# All-app lifecycle repair — 2026-10-29

See [APP-AUDIT.md](technical/APP-AUDIT.md) for the current complete audit, fixed findings,
executed disposable checks and physical-device boundaries. At audit start, the
connected revision matched 34/34 managed hashes; the pending host repair now
manages 39 files, including initial utility launchers and the inspector ownership
baseline. No device writes or real migrations were performed during this audit.
Prior statements below about ten-second holds and missing redraw describe older
revisions. The user must run update-appliance.py to install this source revision.

# VPN Login screen return repair — 2026-10-29

User reports a blank screen after VPN Login, requiring reboot. No USB mount
was present during this repair, so device runtime logs and enrollment state
could not be inspected. Source confirms direct framebuffer feedback had no
Amazon UI redraw/return path. Added an EXIT cleanup: xrefresh for ordinary
feedback, and Amazon Home activation followed by xrefresh for VPN Login.
The Home call matches the bundled upstream Kindle launcher API. Failures in
UI recovery do not mask the original action result. Login still holds its link
for up to three minutes; authorization on the phone does not shorten the timer.
Disposable command-adapter probes passed for normal/error exit, Home followed
by redraw, redraw after failed Home, and preserving error exit status. Shell
syntax checks passed. Physical return to Home remains unverified; this fixes
a demonstrated lifecycle omission, not a proven VPN/kernel crash. Updated
source only; user must run update-appliance.py for the Kindle rollout.

# Hardware feedback and VPN login display — 2026-10-29

User confirms KOReader, Terminal and Calendar open successfully. Latest connected
release has 34/34 recorded managed-file hashes matching. Reader crash.log shows
Terminal shell spawn and normal UI teardown with exit code 0. Planner SQLite
quick_check returns ok (zero tasks/events: editing/persistence not yet accepted).
No documents/GleWorks-status.txt exists yet. VPN runtime logs remain under /tmp
on the device, unavailable through this USB mass-storage mount.

Source confirms all feedback was held only 10 seconds. Changed VPN Login to hold
messages containing the enrollment URL for 180 seconds, while other messages
retain 10 seconds. Duration is bounded to 1–300 seconds. Link remains off USB
logs. This adjusts screen visibility, not the Tailscale server's link expiry or
the CLI's 15-second wait; successful enrollment/connectivity is still pending.
Disposable probes passed for link/nonlink display duration, default/invalid
arguments, feedback fallback and updater/repeat on a copy of current USB files.
Physical three-minute visibility still needs device confirmation after host copy.

# Integrated expansion verification — 2026-10-29

This section supersedes older pending-reader and six-page-refresh snapshots below.
Live USB inspection confirms both refresh counts are now **4** and the previous
feedback-patch marker exists. Reader runs well according to the user. No Status
export exists on this mount yet; visible feedback and VPN connectivity are not
established by USB inspection.

Implemented in one controlled app revision: native KOReader calendar/todos,
existing upstream PTY terminal launchers, offline complete Update Apps and Restore
Apps, shared Status export and feedback, retained manual VPN/Transfer sessions.
No update hosting exists; online updates are unavailable. No Kindle files were
written by this agent session during expansion (mount is session-read-only).

Disposable local probes, outside maintained source:

- Real SQLite using the installed KOReader wrapper with native host SQLite:
  task/event CRUD, bound SQL/injection-like titles, completion, reopen persistence,
  invalid dates/times/titles, 2,000 record and 500 list bounds. Fixed int64 cdata
  IDs to ordinary numeric IDs after reproducing failed task completion.
- 60 calendar weekday comparisons, including 2038 and 2099; leap-date checks.
- Widget adapters: month/day navigation, editors, completion/delete confirmation,
  terminal dispatch, absent/staged update, normal Exit broadcast and database close.
  Adapters do not prove physical layout or keyboard behavior.
- Copied actual connected patch revision -> integrated updater -> repeat updater:
  accepted originals/backup hashes, preferences retained, unexpected edits refused.
- Feedback renderer failure -> eips fallback; VPN export excludes enrollment URLs,
  keys and account fields. Target renderer behavior remains device acceptance.
- Full apply and restore in a disposable filesystem with runtime commands adapted:
  preferences/planner/files retained, corrupt hash abort before cutover,
  preactivation personal-copy failure clears READY and permits restaging;
  post-activation preflight failure restores originals. Restore at five simulated
  interrupted cutover phases; recovery library launcher finds retained code with
  active app directory and runtime directory absent (after reboot). These are
  shell/path probes, not physical power cuts.
- Installed Lua validator with filesystem/JSON adapters: complete tree/record,
  missing runtime, symlink, noncanonical path, unlisted and personal empty dirs.
- Official-download packaging with synthetic ZIP/release assets and path/link
  rejection cases; existing HTTP, token, reader-profile and staging probes rerun.
- Python, shell, JavaScript and Lua syntax checks. No maintained tests added.

Fresh independent review identified incomplete preactivation recovery, recovery
entrypoint dependence on an absent active folder, empty unmanifested directories,
and missing mandatory own-runtime files. All four were addressed and exercised
by disposable recovery/validator probes. No fresh online release download, target
planner/terminal GUI, real VPN/transfer or power-loss durability claim is made.

Hardware acceptance still required after user runs update-appliance.py:
reader/page turns, calendar CRUD and reopen, terminal/keyboard/exit, no-package
Update Apps feedback, real complete bundle apply/restore, VPN enrollment and
forwarding, authenticated transfer, Status visibility/export, sleep/wake/reboot.

---

# Device installation evidence — 2026-10-29

## Reader acceptance and feedback repair

User reports Reader runs well with good responsiveness, but light ghosting;
VPN and Status showed no visible feedback. Reader launch/exit and perceived
responsiveness now have user evidence; no numerical latency measurement exists.

Prepared `update-reader-vpn.py`: preserve originals, set refresh counts 6 to 4,
hold feedback ten seconds, fall back from failed FBInk to eips without requiring
fold, and save nonsecret Status output. Actual affected launchers/control script
match the original bundle; settings currently remain 6 until the host update runs.
Target rapidjson module exists. A running VPN daemon is reported separately from
its connection/authentication state. No conclusion about VPN connectivity yet.

Disposable probes passed for patch/repeat application, preserving unrelated
preferences and backups, rejecting unexpected installed edits, failed-renderer
fallback, wrapping, and limiting VPN JSON export to connection state/own IPv4.
Shell syntax, Python compilation and LuaJIT bytecode compilation passed. The
rendering cause is not proven from USB alone; actual updated screen behavior
and target JSON decoding remain pending. No maintained tests added.

After the user executed the installer and reconnected USB:

- Install log ends with successful installation/quarantine completion.
- 1,048 installed release files match the prepared SHA256 manifest; no missing
  or changed files. These are real official artifacts, not synthetic fixtures:
  KOReader v2026.07.1 kindlepw2 and Tailscale 1.104.4.
- ARM LuaJIT, LuaSocket/LuaFileSystem and SQLite loading/preflight succeeded.
  HTTP process startup/shutdown and VPN daemon startup/shutdown succeeded.
  Tailscale reports Go 1.27.3; short execution does not prove stable networking
  on this old kernel.
- Reader settings are full_refresh_count=6, night_full_refresh_count=6,
  swipe_animations=false and disable_double_tap=true. Original settings backup
  exists. Reader GUI and physical page-turn behavior have not been tested.
- KPM has only `kpm` installed; only the `kindlemodding` repository remains.
  Foreign-key check returned no issues. Replaced app directories/state are no
  longer active; original reader, package wrappers/state and SQLite snapshot
  exist in the recorded quarantine.

Pending: VPN enrollment and forwarding, authenticated real transfers, effective
access policy, actual reader launch/exit, sleep/wake, reboot and screen latency.
No live root process snapshot was taken during this USB-only verification.

## Earlier local verification and preparation evidence

# Current implementation verification — 2026-10-29

## Executed locally

- The actual official bundle now exists: 1,048 files verified with zero checksum
  mismatches, and appliance sources match the current workspace. It contains
  Tailscale 1.104.4 and KOReader v2026.07.1. An interrupted USB copy had 815 files
  and no READY marker/entrypoints. Staging now resumes known partial files.
  Disposable probes passed for partial/missing files, repeat resume, completed
  identical staging, refusal of completed mismatches, unrelated files and links.
  Actual USB resume/readback still requires the user's writable host terminal.
- Official release inspection confirmed `koreader-kindlepw2-v2026.07.1.zip`
  and upstream `make/kindle.mk` confirmed the ZIP includes `extensions/`.
  Fixed the earlier tar.gz-only selection. Full synthetic packaging now passes
  with both ZIP and tar.gz, including ZIP symlinks and skipped KUAL integration.
  The same seven hostile-path/link cases were rejected in ZIP fixtures too.
  Actual release download/extraction still requires the user's terminal.
- HTTPS trust-store repair: standalone Python loaded zero CAs because its
  default `/etc/ssl/cert.pem` was missing. The downloader's system-bundle fallback
  loaded 172 CAs. Certificate verification and hostname checking remain required;
  explicit SSL settings and `NIX_SSL_CERT_FILE` precedence were checked.
  Python compilation and the disposable packaging/archive probes passed again.
  A live download still requires the user's networked terminal.
- LuaJIT bytecode compilation of all three device Lua sources; shell syntax
  checks of all setup/control/library scripts; Python compilation and CLI help;
  browser JavaScript syntax check.
- **21 disposable HTTP probes** using native LuaJIT and real native file IO,
  with substituted socket and directory modules: public assets, missing-token
  rejection, listing/download/upload, no overwrite, partial-upload cleanup,
  symlink rejection, traversal/NUL/malformed encoding, cross-origin rejection,
  duplicate Content-Length, Transfer-Encoding, size limits, reserved filenames,
  invalid UTF-8, oversized headers, missing files and zero-byte uploads. Response
  Content-Length matched actual bytes in every case. This is not a real network
  or target-module test.
- **SQLite migration probes** used the actual copied KPM database, the bundled
  Lua SQLite wrapper and a native SQLite library, with temporary path adapters:
  ownership preflight, removal of two old app records plus reader ownership,
  selective restore, preservation of an unrelated intervening KPM record change,
  repeat record restore and foreign-key consistency all passed.
- **Packaging probe** with synthetic upstream metadata/ARM fixtures produced a
  complete bundle and verified every generated checksum. Seven hostile archive
  cases were rejected: traversal, absolute paths, escaping links, link parents,
  duplicate names, control characters and link cycles. Unexpected/non-HTTPS
  download origins were rejected. Synthetic artifacts are not official releases
  and exist only under `/tmp`; none are suitable for installation.
- **Five reader-profile probes**, using substitute IO/serializer helpers:
  ordinary settings and absent settings accepted; access to system APIs,
  infinite loops and function-bearing settings rejected. Reapplying refuses to
  overwrite the original backup. Actual KOReader serialization remains a
  device check. The final loader is restricted to textual Lua, not bytecode.
- Native Lua token initialization created a 64-character hexadecimal token at
  mode 0600; a second initialization failed and preserved it. A compiled LuaJIT
  bytecode settings file was rejected by the text-only loader. No token printed.
- A fresh socket attempt returned `Operation not permitted`. A direct official
  manifest download returned DNS resolution failure. No actual release archive
  was downloaded in this session.
- Fresh source review found and corrected ARM-specific open flags, interrupted
  SQL recovery, rollback destination preflight, inherited executable defaults,
  old keep-awake cleanup, bytecode loading and the reader executable entrypoint.

No maintained test files were added. Disposable probes live only in `/tmp`.
Source checks and simulated boundaries do not establish device compatibility,
security certification or measured screen speed.

## Still required on the real device

Actual official archive checksums; USB copy/readback; ARM Lua/native-module
loading; VPN daemon/start/enrollment and forwarding; HTTP authentication and
transfers over the tailnet; effective access restrictions; reader/profile launch
and recovery; sleep/wake; reboot persistence; physical response/ghosting
comparison. Tailscale's modern upstream runtime may not support Linux 3.0, even
if a short version/startup probe succeeds. No kernel or firmware workaround is
implemented or authorized by this plan.

## Earlier Go prototype evidence (historical; excluded from device bundle)

# Verification — 2026-10-29

## Executed successfully

- Go 1.26.8: `go vet -buildvcs=false ./...`.
- `sh scripts/build.sh`: native Linux amd64 and Linux ARMv5/ARMv7 builds,
  with CGO disabled and module network access disabled.
- ELF inspection: native machine 62, ARM machine 40; no dynamic interpreter
  in any binary. This does not establish old-kernel compatibility.
- `(cd dist && sha256sum -c SHA256SUMS)`: all three binaries verified.
- `gofmt -l server.go`: no output.
- `node --check web/app.js` and `sh -n scripts/build.sh scripts/kindle-services.sh`.
- Service launcher `help` prints usage without attempting device operations.
- Disposable native CLI runs: random token creation returned 0, token was
  64 lowercase hexadecimal characters plus newline, mode was 0600; repeated
  initialization failed and preserved the original token; a non-loopback
  `0.0.0.0:8080` listen address was rejected.

The first build failed because the workspace exposes an empty, unusable `.git`
directory. Explicitly disabling VCS stamping fixed the build. It does not
disable compiler/vet checks. Build toolchain version is in `dist/BUILDINFO`.

## Blocked or not executed

- Native HTTP startup attempted: exit 1 with `socket: operation not permitted`.
  The sandbox prohibits listening sockets. No HTTP upload/download,
  authentication enforcement, path/symlink rejection, cross-origin behavior,
  partial-upload cleanup, or browser journey has been verified at runtime.
  Source review is not equivalent to these checks.
- No ARM binaries were executed. The reported Kindle kernel (3.0.35) is below
  the current Go runtime's supported minimum. The launcher blocks this build
  on kernels older than 3.2. No compatibility or performance claim is made.
- No Tailscale/KOReader upstream binary was downloaded, compared, installed,
  launched, or configured. Their wrappers have syntax checks only.
- No device root inspection, baseline restore, firmware change, or old-package
  removal occurred; the device was disconnected.
- No DNS, domain, TLS certificate, tailnet policy, or hosting changes occurred.
- No maintained tests were added, per user instructions.

This is a locally built first version, not a completed security audit or a
production-ready replacement for the current Kindle installation.


<a id="history-006"></a>

## Nguồn lịch sử: ours/docs/superpowers/plans/2026-10-29-boot-gate-batch.md

# Consolidated boot gate batch

Goal: reduce USB round trips by reusing reviewed RAM staging and watchdog
launchers in one ordered run. User explicitly requested multiple approaches
per run, within existing ordinary-USB/no-flash constraints.

Design: a host-packaged launcher verifies all input hashes, captures readonly
state, runs the existing readiness batch (allocation faults, input/ownership,
five image sizes/checksums and normal cleanup), then the bounded runnable
watchdog trial last. Existing child launchers own their cleanup and guards.
A reset ends the process; a later invocation with pending evidence only
collects state. No startup hooks or automated post-reboot execution.

Existing nonviable branches are recorded, not retried: downloaded fastboot
RAM execution unsupported in captured BIST; bootloader/partition writes lack
ordinary-USB recovery proof. Native handoff remains absent, so no batch
outcome authorizes kernel execution or claims an Alpine boot.

- [x] Package existing child artifacts with a small orchestrator; add explicit
      collection-only mode to the watchdog shell to prevent a reviewed race.
- [x] Verify syntax, hashes and ordering/failure/recovery behavior using
      disposable host mocks; add no maintained tests.
- [x] Independent review; record exact limitations and user launch steps.

Expanded scope: user requested every viable path to improve the first-boot
decision. TEST-MATRIX.md maps31 checks/routes; audit-host.py executes10 host
structural groups. Device snapshot now includes IRQ/memory layout/CPU/thermal
information before staging and the bounded watchdog candidate. Hardware
execution, native handoff and product acceptance remain pending.

Acceptance: one copy/eject/launch runs all safe independent checks plus one
bounded reset candidate. A child leaving a module behind blocks subsequent
active operations. Staging failure can still be followed by watchdog testing
only after clean module state; its failure remains separately visible.
Signals terminate the wrapper; child launchers retain their existing cleanup.
Every returned result is recorded. Pending markers select collection only.
No new module interface, timeout change, RAM guard reduction, clock/IRQ
modification, persistent boot mutation, flashing or new-kernel execution.


<a id="history-007"></a>

## Nguồn lịch sử: ours/docs/superpowers/plans/2026-10-29-independent-linux.md

# Independent PW3 Linux feasibility and build plan

> Historical implementation plan. For the current native reader and remaining work,
> read [CURRENT.vi.md](guides/CURRENT.vi.md); do not rerun earlier
> stock migration or boot trials just because an old checkbox is incomplete.

> Execute inline with superpowers:executing-plans. No maintained tests, per user instruction.

**Goal:** Replace Amazon OS with an independent, low-power e-ink edge device: offline reading first, with an on-device SSH terminal for operating the user's server.
**Architecture:** Retain the installed Amazon U-Boot and replace the Amazon OS with an independently booted Linux kernel/userspace compatible with it. Jailbreak is an initial installation tool, not a runtime dependency of the target system. Prepare artifacts on the host before device boot operations; a chroot is not completion.
**Tech Stack:** ARMv7, i.MX6SL, installed U-Boot 2009.08-lab126; Alpine Linux (release pending compatibility validation), KOReader, SSH client/server, Wi-Fi, Tailscale and authenticated HTTP transfer Web UI. Kernel/driver and runtime compatibility remain pending hardware support audit. Barebox is retained only as a future alternative.
**Spec:** Latest accepted scope in this plan and user conversation. ours/investigation/FINDINGS-2026-10-29.md and BOOT-ROUTES.md provide evidence/history; superseded candidates there are not dependencies.

## Source access decision

Use official public Alpine distribution sources/mirrors and publicly accessible
kernel/driver sources. No Solarcene account, OpenID registration or paid
subscription is required by this plan. Ponder installers/images and inaccessible
smallsolar kernel patches are excluded, not merely waiting for credentials.
No complete public PW3 kernel port has yet been validated; this remains real
engineering work, not a ready-to-flash Alpine image.

This source-access decision does not remove the separately accepted Tailscale
network enrollment requirement; no Solarcene identity is involved in it.

## Constraints

- Ordinary USB data cables remain allowed for installation/recovery. Routine administration must also work over Wi-Fi plus Tailscale without a data cable, including while using a charge-only cable. No UART cable, opening enclosure, soldering or eMMC pin manipulation is authorized.
- Existing user data may be discarded; preserve hardware identity/calibration needed for operation.
- User requires avoiding a bricked device. No persistent boot mutation without a demonstrated USB-only recovery route.
- No new maintained tests; use source inspection, syntax checks and actual build/runtime evidence.
- Workspace has no usable git metadata. Keep changes under ours/linux; preserve appliance implementation.

## Review focus

- Root access does not establish recoverability after loss of kernel/userspace.
- Keeping the installed bootloader does not establish compatibility of a new kernel or a complete PW3 display/power port.
- No bootloader replacement in the active track; preserve its research for a separately revisited need.
- Source tag/commit and SHA256 record are provenance, not hardware acceptance or trusted signature proof.
- Simulation, compile success and on-device boot must be reported separately.

## Execution

- [x] Match device evidence to public bootloader support and document USB-only limitation.
- [x] Extract exact running-kernel configuration locally for comparison.
- [x] Prepare host-only source acquisition script selecting Barebox v2026.10.0; check syntax.
- [x] Obtain Barebox source and record resolved commit: 5c46c205c8d1a7c57b9dbaa5e516758a6c0a4d80; user ran host fetch.
- [x] Inspect Barebox board code, boot targets, environment writes and documented recovery paths; see ours/linux/BAREBOX-REVIEW.md. Stock-family U-Boot source reviewed at 4b6c52358630564562224b780bd75a5659fbc84b; see ours/linux/UBOOT-REVIEW.md. Actual production/BIST strings and BIST Fastboot boot-handler disassembly now corroborate key source behavior; see ours/linux/BOOT-CAPTURE-REVIEW.md. No on-device Fastboot trial.
- [ ] Establish installation and recovery via the installed bootloader using ordinary USB on this exact firmware. A temporary boot is preferred if supported; actual Fastboot boot behavior does not provide the assumed RAM-image handoff.
- [ ] Select a kernel with verified PW3 display/touch/power/Wi-Fi support; audit patches and device tree.
- [ ] Obtain ARM toolchain and build outside the source tree, retaining configuration/logs/hashes.
- [ ] Only after a suitable installation/recovery route exists: conduct a logged boot of the independent OS and verify recovery to a working system.
- [ ] Add reader and validate refresh, touch, storage, reboot, sleep/wake and battery behavior.

No flash commands or persistent device changes are included in the acquisition step.

## Deferred bootloader replacement — preserve for later

User explicitly requested retaining this research in case replacement becomes
necessary. It is not part of current implementation. See
[bootloader research and revisit conditions](technical/investigation/BOOT-ROUTES.md#current-decision-and-deferred-bootloader-work).
Earlier completed Barebox tasks above are historical investigation evidence,
not a requirement to build or deploy Barebox now. Keep source checkouts, pinned
commits, reviews and private captures; do not delete them as abandoned work.

## Accepted product scope — reader and SSH edge device

User confirmed: reading is the primary function; remote access means opening
an SSH terminal on Kindle to operate the user's server. The Kindle is a client
within the user's system, with heavy work performed on the server. Inbound SSH on Kindle is now explicitly authorized over Tailscale only, for
remote package installation and administration. This does not authorize public
SSH exposure or changes to the remote server.

- Offline reading with KOReader, responsive opening/page turns, working touch,
  suitable e-ink refresh and persistent reading state.
- On-device terminal and keyboard usable for SSH sessions; validate server host
  identity and keep private credentials out of public reports and USB exports
  where feasible. Server address and authentication details are not yet supplied.
- Wi-Fi on demand for server use or deliberate transfers/synchronization; no
  periodic background polling by default. No sync service is selected yet.
- Reliable suspend/resume, reconnect after wake, and measured idle/reading/SSH
  power behavior. Low power is an acceptance requirement, not a proven result.
- Retain installed bootloader, use jailbreak only for initial transition, and
  retain the USB-only constraint. Independent OS boot remains unverified.

Acceptance must cover offline reading, page refresh/touch, reading-state
persistence after reboot, an authenticated SSH session to the intended server,
Wi-Fi shutdown after use, repeated sleep/wake, and battery measurements.
Existing appliance acceptance is not acceptance of the replacement OS.

## Accepted networking and remote administration

- Alpine is the selected distribution; compatible packages may be installed
  remotely with apk. No particular Alpine release/image is yet validated for PW3.
- Wi-Fi and Tailscale operate simultaneously: Wi-Fi supplies connectivity and
  Tailscale provides private access, without requiring a USB data connection.
- Outbound SSH to the user's server and inbound key-authenticated SSH to Kindle
  are both in scope. Inbound management is restricted to the tailnet, not exposed
  directly on the Wi-Fi LAN or public Internet; verify actual interface binding
  or firewall behavior on the selected kernel and Tailscale mode.
- A simple authenticated HTTP Web UI permits multiple-file selection and
  drag-and-drop uploads, progress/results, explicit errors and no silent
  overwrites. Uploaded documents appear in KOReader's book directory. Access is
  restricted to Tailscale. Evaluate the existing appliance implementation for
  reuse without treating old runtime results as Alpine acceptance.
- Reading mode permits normal low-power sleep; management/transfer mode keeps
  networking available for the active session. Do not stop Tailscale while an
  SSH session needs it. No assumption of wake-on-Wi-Fi or access during deep sleep.
- A charge-only cable supplies power, not USB data or recovery access. Network
  administration requires the running OS, working Wi-Fi and Tailscale; it cannot
  substitute for recovery when the kernel/network stack does not boot.

Additional acceptance: administer Kindle from an authorized tailnet client
without USB data, install a compatible package with apk, transfer books via Web
UI, reject unauthorized/non-tailnet access, and measure management-mode battery
behavior. Capture network/suspend recovery behavior separately from cold-boot
recovery. User intent is recorded; none of these Alpine behaviors is yet proven.

## First Alpine feasibility research results

See [Alpine/PW3 feasibility](technical/linux/ALPINE-FEASIBILITY.md). Candidate
userspace branch is Alpine v3.24 ARMv7, not a prebuilt Kindle image. Current
kernel lacks TUN and is below Go 1.24's supported Linux minimum. Investigate
a modern kernel with appended DTB/ATAG compatibility to retain installed U-Boot;
this is not yet a verified boot recipe. Complete modern PW3 driver support and
USB-only recovery remain unresolved. Existing Web UI needs drag/drop and byte
progress; prior uploads do not satisfy those requirements.

## Continuous boot-readiness preparation

Native diagnostic image now contains embedded PID1, with a host USB nonce
observer; build/CPIO/ELF checks passed. See ../../../linux/native-boot/README.md
and HANDOFF-AUDIT.md for source/review evidence and the one-run readiness
bundle. The prior real-image staging batch passed on device; the new image
and its USB/PID1 remain unbooted. Watchdog query side effects and warm-reset
boot-source override require targeted current readback and reset/return
qualification. No handoff enabled and no boot/recovery checkbox closed.


<a id="history-008"></a>

## Nguồn lịch sử: ours/docs/superpowers/plans/2026-10-29-popup-errors.md

# Fixed notification popup and inspectable failures

User specifies one notification area, popup styling and one discoverable error
location. Prior instruction authorizes integrated implementation without further
design checkpoints. Retain shell/Lua runtime; no service, GUI framework or polling.

Design: fixed 48-column panel centered on screen with black border, GleWorks heading,
14 wrapped content rows and a footer. FBInk draws only that panel without clear;
eips fallback uses a bounded fixed x=2,y=6 area when FBInk is unavailable. The native reader's existing framed
InfoMessage gets fixed centered size, title and disabled dragging. Preserve
Unicode native text. Standalone shell messages use the existing ASCII content;
wrap safely at UTF-8 character boundaries in the formatter. Long content gets
paged within the same panel so status/link text is not silently discarded.

Failure journal: /mnt/us/gleworks-logs/errors.log, with one rotated predecessor,
256 KiB per file. Record UTC, controlled action, numeric exit code, safe category;
never raw message, token, login URL, account/peer identifiers or book titles.
Root-private runtime logs retain detailed network diagnostics. This persistent
folder is outside app release directories and survives updates/restores/reboot.
Inspect validates schema and collects the last 100 safe events from both files.

- [x] Shared panel rendering and UTF-8-safe wrapping, one bounded area, renderer
  failure and Home/redraw events included in the error journal.
- [x] Shared failure recorder and daily/initial launchers, control/update failure
  traps, native planner callback errors; log failures without changing exit codes.
- [x] Inspect collection, release/updater/validator integration and retention.
- [x] Disposable rendering/layout, secret-exclusion, rotation, app failures,
  inspect collection, packaging/update probes; syntax and source snapshot.

No maintained tests, per user. Physical e-ink appearance and Home return still
need device acceptance. Disk failure or abrupt power loss can prevent logging.

Delivery: implemented and locally verified; see NOTIFICATIONS.md for exact probe
boundaries. User-run host copy and physical e-ink acceptance remain pending.
Rescue eips rendering uses its own fixed bounded grid when centered FBInk is
unavailable. Log records intentionally use constant metadata rather than trying
to redact arbitrary raw text. Detailed private network logs remain separate.


<a id="history-009"></a>

## Nguồn lịch sử: ours/docs/superpowers/plans/2026-10-30-alpine-safe-native-boot.md

# Safe native Alpine boot implementation plan

> Historical implementation plan. For the current native reader and remaining work,
> read [CURRENT.vi.md](guides/CURRENT.vi.md); do not rerun earlier
> stock migration or boot trials just because an old checkbox is incomplete.

> **For agentic workers:** Execute inline with superpowers:executing-plans.
> No new maintained tests, per user instruction. Preserve proven artifacts.

**Goal:** Boot independent Alpine on PW3 with installed bootloader retained,
then replace Amazon OS only after ordinary-USB recovery is qualified.
**Architecture:** First validate returning RAM preparation, then a nonpersistent
minimal native kernel trial, then Alpine userspace in RAM, and finally persistent
independent boot. Jailbreak is a transition tool only; chroot is not completion.
**Tech Stack:** PW3 i.MX6SL ARMv7 512MiB; installed U-Boot; prepared Linux6.18
candidate; pinned official Alpine ARMv7; USB serial plus nonce observer.
**Spec:** 2026-10-29-independent-linux.md and accepted user constraints in session.

**User-selected reference:** [schuhumi/alpine_kindle source review](technical/linux/ALPINE-KINDLE-REFERENCE.md).
Use its PW3 userspace/UI experience in Tasks4–5; its chroot/Upstart launcher
does not replace Tasks2–3 or the independent-boot acceptance requirement.

## Global constraints

- Keep installed bootloader. Ordinary USB only; no UART/opening required.
- Amazon OS and documents are disposable, but current recovery assets remain
  until an independent working/recovery path is demonstrated.
- No guarantee of zero brick risk. A reproducible safe-enough route has to be
  supported by evidence; approval cannot substitute for missing hardware facts.
- Before persistent cutover, do not alter primary/diagnostics kernel, bootmode,
  partition layout or bootloader. Retain initial trials entirely in RAM.
- Normal module load/removal only; no forced load/unload, ABI bypass or guessed
  MMIO writes. Hashes identify artifacts, not proof of hardware support.
- Do not repeat the already observed stock reset simply to reconfirm it.

## Review focus

- DMA writes after old kernel memory is repurposed: require bounded stop and
  producer exclusion for every active bus master, not shutdown callback presence.
- USB detach after gadget stop: collect durable pre-jump logs and host events;
  require a fresh challenge from new PID1, not an old USB device or stale text.
- Watchdog/clock changes and warm boot selectors: stock return is not proof
  recovery survives handoff; derive from exact code and capture actual behavior.
- Malformed or larger Alpine image: reject before shutdown if size, ownership,
  relocation/workspace or fixed-image identity checks fail.
- Persistent boot failure: snapshots and diagnostics CRC alone are not a tested
  cable-only rescue path. Do not write just to discover whether rescue works.

## Task 1: Qualify returning preparation on the actual Kindle

**Files:** existing ours/linux/native-stage/{bundle,stage.c,stock-normal-core.inc}.
**Consumes:** copied gleworks-native-stage.sh and exact hashed inputs.
**Produces:** reviewed runtime capture; no native jump.

- [x] User safely ejects USB storage, launches GleWorks Native Stage once,
  reconnects after completion. Preserve12MiB guard; no blind process killing.
- [x] Verify capture hashes/input identity, helper results, fixed SHA256/control
  preparation, repeated preparation, bad-image rejection and final zero pages.
- [x] Confirm normal module removal and absence of new kernel faults. If guard
  declines or a check fails, diagnose from the same capture before another run.

## Task 2: Complete bounded transition and recovery engineering

**Files:** ours/linux/native-handoff/{transition.c,chain.c,memory-review.json};
new shutdown audit/implementation only where exact hardware behavior is known.
**Consumes:** allocator registry, captured callbacks, device driver associations.
**Produces:** reviewable no-return implementation and explicit GO/NO-GO rationale.

- [ ] Map all active DMA producers: USB gadget/EHCI/OTG, both MMC hosts,
  SDMA, EPDC/PXP and any other bus master identified in the device capture.
- [ ] For each, derive stop, bounded idle/readback and prevention of new work
  from exact driver/register semantics. PXP software terminate_all is insufficient;
  its unbounded suspend wait cannot be copied as a bounded handoff operation.
- [ ] Order userspace/producers, outstanding storage writes, filesystems and
  controller stops while interrupts/scheduler are still usable. Keep required
  code, parameters and logs in RAM before stopping storage/USB. Do not treat
  removing a mounted storage driver as a harmless stop primitive.
- [ ] Validate callback identities/lifetimes and outer-cache functions. Check
  decompressor envelope through0x805ad6e0 against reserved RAM and all allocations.
- [ ] Derive watchdog deadline/pretimeout, clock continuity and warm-return
  selectors through shutdown and candidate kernel initialization. A failed jump
  must have a credible bounded reset/return route; record remaining uncertainty.
- [ ] Build/inspect and run disposable host probes of actual source/assembly.
  Independent whole-change review before an execute-capable device package.

## Task 3: First nonpersistent native kernel trial

**Files:** ours/linux/native-boot/{init.c,observe-usb.py,probe.config}; separately
versioned transition package, preserving all prepare-only/proven packages.
**Consumes:** Task2 GO and exact fixed diagnostic image; not Alpine yet.
**Produces:** native Linux/PID1 proof or a bounded, diagnosed failed trial.

- [ ] Keep storage writes disabled in candidate kernel/PID1. Log exact hashes,
  recovery assumptions and host USB observation before one explicit handoff.
- [ ] Require fresh32hex challenge response from new PID1 identifying Linux6.18.
  Native USB is an outcome of this trial, not a circular precondition for it.
- [ ] Observe bounded recovery/working return after trial and preserve evidence.
  On failure, stop automatic retries and fix the evidenced cause on the host.

## Task 4: Boot actual Alpine userspace in RAM

**Files:** isolated ours/linux/alpine-boot build inputs/artifacts; new image
identity metadata and bounded staging changes if needed; retain old fixed image.
**Consumes:** native kernel/USB/recovery evidence and official Alpine ARMv7 files.
**Produces:** Alpine PID1/session under a new independently running kernel.

- [x] Fetch pinned Alpine3.24.3 ARMv7 release and published digests via verified
  TLS; retain provenance and verify signature when publisher signature is provided.
  Archive and published SHA256/SHA512 acquired and matched over verified TLS;
  no publisher signature verified. See alpine-boot/rootfs-review.json.
- [x] Build minimum Alpine initramfs with BusyBox/musl loader/libraries, devices
  and controlled USB console. Check ELF ABI/dependencies on host; no auto apk
  install, storage mount/write or package scripts during first hardware trial.
- [ ] Recalculate actual image/RAM/decompressor bounds. Existing module caps
  image2MiB and allocator1024 pages; do not assume full Alpine rootfs fits or
  increase limits without ownership, fault-cleanup and memory qualification.
- [ ] Run returning preparation for the new fixed Alpine image, then one RAM
  boot using the validated transition/recovery route. Verify Alpine release,
  /proc/version, PID1 and nonce over USB. This proves Alpine boot only.

## Task 5: Qualify PW3 hardware for independent use

**Files:** separate PW3 DT/kernel configuration and Alpine board services.
**Consumes:** working Alpine RAM boot; exact board/firmware/waveform captures.
**Produces:** reader/device acceptance evidence before destructive cutover.

- [ ] Enable/test eMMC read first, then deliberate writes; preserve panel data.
- [ ] Integrate EPDC/waveforms, touch, frontlight, PMIC/charging and shutdown.
- [ ] Validate Wi-Fi, TUN/Tailscale, SSH over tailnet; later reader/Web UI.
- [ ] Measure suspend/wake, reconnect, charging and power behavior on hardware.
  A generic upstream build or successful USB boot does not prove PW3 usability.

## Task 6: Persistent boot and Amazon OS removal

**Files:** a separately reviewed install/recovery recipe and exact-sized images.
**Consumes:** independent working system plus demonstrated cable-only recovery.
**Produces:** installed bootloader directly launches independent Linux/Alpine.

- [ ] Demonstrate a usable rescue path independent of the Amazon userspace
  being replaced. Recheck selector/image validation and recovery prerequisites
  against actual bootloader; Fastboot boot does not execute downloaded RAM data.
- [ ] If no cable-only rescue is established, remain at RAM trials and report
  this boundary; never equate bootloader preservation with recoverability.
- [ ] Review exact target regions/image sizes and interruption behavior before
  persistent OS writes; retain bootloader and necessary board-specific data.
- [ ] Verify repeated cold boot directly into Alpine, recovery after failed OS
  boot, and operation without Amazon services, registration or jailbreak runtime.

## Immediate handoff

Task1 completed on device:78 PASS,25 manifest hashes verified, normal module
removal with zero owned pages. No repeat run needed. Agent continues Task2
source/host work and Alpine acquisition
without waiting on unrelated product features. No new user design approval
is needed for those already authorized reversible preparations.


## Alternative userspace route and control prerequisite — 2026-10-30

Investigate retaining the exact stock kernel/drivers while independently
starting Alpine as PID1; this can satisfy native userspace but does not solve
modern Tailscale's kernel requirement. No decision to delete Amazon rootfs.
MULTI/external raw initramfs is source-supported, not hardware-qualified.
Vendor Upstart0.6.6 has no SIGTERM reexec handler; do not use modern Upstart
`telinit u` recipes. A running Alpine chroot only measures ABI compatibility.

Before another on-device iteration, install the reviewed temporary
[control session](technical/linux/control-session/README.md). One physical
launcher activation is required with the current mass-storage-only connection;
thereafter use authenticated Wi-Fi SSH when available. Its diagnostics do not
signal PID1, stop DMA, change boot selectors or execute a new kernel.
Alpine-image prepare-only package is host-verified; returning hardware
qualification, larger decompressor/RAM derivation and all Task2 GO criteria
remain open. Do not mark Task4 complete from these host preparations.


Host alternatives prepared: stock-alpine-boot/build-host.py produces an exact
stock-kernel+rawCPIO MULTI hypothesis, private host-only5,209,408-byte candidate.
Actual CRC/payload/CPIO checks pass; captured decompressor arithmetic avoids
initrd. Production bootloader behavior, USB, ABI and recovery still open; do not
install it or claim Task6 acceptance. Modern Alpine memory-review now derives
heap end0x807b6b10 and reproduces the prior diagnostic's layout; hardware/DMA
qualification remains open. Temporary-control and returning-stage bundles are
copied/readback-verified17files; awaiting physical activation, not approval.


## Returning Alpine-image trial qualified — 2026-10-30 01:05

New stage capture verifies78PASS, exact2,722,513-byte image/668owned pages,
SHA/chain/control prepare, rejection/fault/close cleanup, normal removal and
zeroowned pages. No new unexpected kernel lines in exercised interval.
Separate Alpine3.24.3 BusyBox/musl ABI-only chroot runs on actual stock kernel.
This satisfies the returning-preparation portion of Task4, not the RAM-boot
portion. Upstart is stillPID1. Do not rerun these passed checks unchanged.

Temporary SSH daemon starts, but captured endpoint192.168.1.200 is unreachable
from host (neighborFAILED), so authenticated control remains unqualified.
Try existing session with USB unplugged and Kindle awake/on Wi-Fi before any
new device launcher. No boot/flash action follows from these successful probes.


## Authenticated Wi-Fi and RAM PID1 preparation — 2026-10-30 01:25–01:34

After USB unplug, pinned-key root SSH to the captured endpoint succeeds with
fresh challenge; PID1 remains /sbin/init.exe, kernel3.0.35. Device control no
longer requires a physical launcher for each returning experiment. USB mass
storage attachment correlated with failed neighbor resolution; cause unproven.
Prior unreachable observations remain historical, not current status.

Ruling: investigate live RAM-only Alpine PID1 while retaining the stock kernel,
IRQs and device drivers. This avoids the unqualified DMA/kernel transition for
this experiment; it does not qualify cold boot, modern Tailscale or Amazon OS
removal. Old Amazon processes temporarily retained for Wi-Fi/control are an
explicit limitation, not independent-system completion.

pid1-handoff builds freestanding ARM trace/bootstrap and a separate exact-ABI
kernel deadline module. All24 imports have captured exported symbols. Latest
child qualification passes four TRACEME/ATTACH valid-exec and ENOENT rollback
cases, including nanosleep/restartblock continuation. Absent and idle guardian
reject before PID1 attachment. Returning2s kernel deadline fires(state4), cancel
returns idle/unpins, next deadline cancelled remains idle, normal rmmod succeeds;
taint unchanged. Dmesg499 before-tail lines match, followed only by two expected
undefined-instruction child traps and guardian idle/test/removal messages.
Harness output assertions required correcting grep state= formatting and
allowing automatic udev node removal; these were not device module failures.

Review fixes: distinct injectedPC avoids signal.c restartblock rewrite;
marker-only authorization replaced by live compiled-token ioctl(state2 and
>=60seconds remaining); bootstrap binds root trialnonce; userspace sync/reboot
replaced with process-context kernel emergency_restart on exact stock PMIC path.
No PID1 attachment or actual guardian reset yet. RAM root and bind mounts are
prepared in confirmed /dev/shm tmpfs; /tmp is not assumed RAM-backed.


## Additional qualified milestone: stock-kernel Alpine global PID1

- [x] Actual Alpine3.24.3 BusyBox/musl PID1 and tmpfs root;44 fresh-nonce
  observations through90.33s, pinned live ELF hashes, TracerPid0.
- [x] Real unchanged-kernel deadline reset and stock return: fresh bootID,
  restored Upstart, trial RAMroot/module absent, authenticated Wi-FiSSH.
- [ ] Clean independent services/cold boot and recovery after replacement-rootfs
  failure. Existing Amazon processes remained during this trial.

This milestone does not close Task3(newkernel), Task4(newkernel+Alpine) or
Task6(persistent cutover). It removes uncertainty about live Alpine PID1
initialization and the exercised stock-kernel recovery path. Preserve the
no-write boundary for stored boot/rootfs assets while USB rescue is unproven.


<a id="history-010"></a>

## Nguồn lịch sử: ours/docs/superpowers/plans/2026-10-30-stock-kernel-alpine.md

# Stock-kernel Alpine native implementation plan

> Historical implementation plan. For the current native reader and remaining work,
> read [CURRENT.vi.md](guides/CURRENT.vi.md); do not rerun earlier
> stock migration or boot trials just because an old checkbox is incomplete.

> Execute inline with superpowers:executing-plans; user opted out of repeated
> approvals. No new maintained tests. Preserve all proven artifacts.

**Goal:** Alpine userspace as nativePID1 with existing bootloader/kernel.
**Architecture:** Host-built readonly ext3 Alpine root, exact stock modules,
RAM-only mutable directories and90s reset guardian for the first cold-start.
**Spec:** ../specs/2026-10-30-stock-kernel-alpine-design.md

## Constraints and review focus

Ordinary USB only; no enclosure/UART. No stored boot/kernel/selector/p1 changes
until independent cable recovery qualified. No forced module operations.
Image ownership, symlinks and full fs metadata must survive unprivileged build.
Partial boot or serial initialization failure must not strand PID1 without the
previously qualified stock-kernel recovery deadline. Coldboot behavior remains
unknown even when source paths and host image checks pass.

- [x] Recheck pinned rootSSH/bootID/root entrypoint and exact cmdline.
- [x] Inspect p2 contents/mountrole, kernel special-reset config and bootloader
  routing: no current independentUSBrescue; no bootmode changes authorized yet.
- [x] Build separate native-rootfs/build-host.py output: full pinned Alpine,
  exact stock modules, sysinit/USBproof/guardian,64MiB ext3 image. Host only.
- [x] Verify image e2fsck, root ownership, actualimage file/symlink contents,
  shell syntax, ELF inputhashes and unchanged prior artifacts.
- [x] Run full rootfs/apk ABI check from tmpfs on actual stockkernel; noPID1 change.
- [x] Coldstart exact captured kernel in PW3/Muscat emulation with stock bootargs:
  relative init, static console/null, AlpinePID1, guardian and USBdriver readiness.
- [x] Exercise missing init/corrupt p1 in emulation: panic, no automatic USBrescue.
- [ ] Qualify official signed recovery on synthetic broken roots and establish
  ordinary USB entry/return on hardware before relying on that recovery route.
- [ ] Demonstrate independent cable rescue and return without relying on p1.
- [ ] Only after rescueGO: exact reviewed installation and coldboot of candidate;
  require newPID1/freshUSBnonce/noAmazonprocess proof and resetreturn.
- [ ] Production init/services and repeated coldboot, then AmazonOSremoval.


<a id="history-011"></a>

## Nguồn lịch sử: ours/linux/boot-gate-batch/TEST-MATRIX.md

# Expanded PW3 boot qualification

Purpose: maximize evidence before any kernel execution, using ordinary USB.
This is not a measurable boot-success percentage or a no-brick guarantee.
Passing host checks, staging, reset recovery and an actual native boot are
separate milestones. A failure may be fixable; UNKNOWN is never PASS.

| ID | Check / route | Evidence and acceptance |
| --- | --- | --- |
| H01 | Source/image inventory | Pinned Linux6.18 HEAD, clean tracked source, current bundled image hash; not independent proof of build provenance |
| H02 | ARM static binaries | PID1/helper ELF machine40, no interpreter/dynamic segment |
| H03 | Embedded root | Verify CPIO bytes inside decompressed actual zImage and exact /init, root ownership/mode, console and mountpoints |
| H04 | Kernel envelope | zImage magic/length, appended exact DTB, image under module2MiB limit |
| H05 | Board memory | DTB declares512MiB at0x80000000; compare with captured iomem before execution |
| H06 | Minimal side effects | Generated config disables MMC/MTD/display/network/PM; no stock root command line. Config sidecar is not independently extracted from packaged kernel |
| H07 | USB candidate | Config enables gadget/ChipIdea/MXS PHY; DTB USB peripheral enabled; physical PW3 path still UNKNOWN |
| H08 | Image data readback | Expected padded FNV values independently recomputed for all five image sizes |
| H09 | Module linkage | Both guard/load imports are captured exports; vermagic/descriptor/init-exit match stock ABI |
| H10 | Stock images | Captured main/diags uImage header and payload CRCs valid; not restoration proof |
| D01 | Current identity and health | Existing child guards: exact kernel/config/taint, rootro, battery>=40%, minimum RAM |
| D02 | Current hardware baseline | Capture iomem, IRQs, buddyinfo, vmstat, mounts, CPU governor/frequency, available thermal readings |
| D03 | Allocation failures | Existing load helper exercises budgets0..7; no leak or emergency allocation |
| D04 | Five staging sizes | 4KiB,64KiB,256KiB,1MiB,whole new image; exact device checksums |
| D05 | Invalid requests/ownership | Existing helper: malformed args, exclusive open, privilege drop, final-close cleanup |
| D06 | Normal module cleanup | No remaining GleWorks module before watchdog; no forced unload |
| D07 | Abort callback | Immediate guard restoration returns; independent periodic feeding remains unproven |
| D08 | Sleeping watchdog wait | Prior device run: NO RESET; deadline callback returned at150.487s. Do not rerun unchanged |
| D09 | Runnable watchdog wait | Current candidate stays runnable, normal scheduling/IRQs, monotonic client limit160s, guard150s |
| D10 | Reset cause/return | If reset: collect boot ID/WRSR/stock return. Changed boot ID alone insufficient |
| D11 | Post-run state | Review module/taint/kernel logs, selectors and cleanup; errors must not be hidden by batch status |
| R01 | Native3.0.35 handoff | NOT IMPLEMENTED: adapt/review cache, IRQ/FIQ, MMU, relocation and DTB argument before execution |
| R02 | Generic3.8 soft_restart module | NO-GO as-is: stock soft_restart absent; do not force-load or patch vermagic |
| R03 | Fastboot RAM boot | NO-GO on captured BIST: handler invokes stored boot command; no downloaded RAM execution |
| R04 | Persistent main/diags image replacement | NO-GO until ordinary-USB restore demonstrated; no partition/selector writes in suite |
| R05 | Vendor-kernel Alpine alternative | Research-only: hardware reference useful, but3.0.35 insufficient for current Go/Tailscale; new init/root still needs safe boot path |
| R06 | Chroot/container fallback | Can investigate userspace compatibility later; does not replace Amazon OS or prove native boot |
| P01 | Real native boot observation | Fresh USB challenge from running6.18 PID1; absence of USB response is ambiguous failure |
| P02 | Diagnostic return to stock | Observe actual return after first native kernel, with preserved boot selection |
| P03 | Alpine userspace | Pinned ARMv7 rootfs/signatures/package closure and runtime; no minirootfs currently certified bootable |
| P04 | Product acceptance | EPDC/waveforms/touch/frontlight, Wi-Fi/Tailscale, SSH/upload UI and measured idle/suspend power |

## Run sequence and decision

1. Host audit validates packaged inputs and writes `host-audit.json`. Its
   host-check exit0 never means boot GO.
2. Copy the one bundle, eject, launch **GleWorks Boot Gate Batch**. Readonly
   hardware capture, staging/fault/ownership suite, then watchdog candidate.
   Insufficient RAM stops staging, not its guard. A leftover module blocks
   watchdog. No random process kill, forced unload, clocks/IRQ changes or flash.
3. After a reset, run the same launcher once more. Pending evidence selects
   collection-only; even if a marker disappears, the child collection mode
   cannot arm. Reconnect USB. Without reset, reconnect after three minutes.
4. Review child results individually. Keep source/config/image/ABI failures,
   watchdog recovery, missing handoff, and physical USB support distinct.

Current overall decision: **NO-GO for kernel execution**, because R01 is absent
and D10 is unproven. A successful batch can clear prerequisites, not silently
enable execution. Before first handoff, audit decompressed kernel/control-page/
DTB memory placement against actual reserved RAM, all required nonexported
callbacks and device shutdown/watchdog clock behavior. Do not infer these
from successful compressed-image staging.

Once the mandatory gates pass, first boot is the minimal initramfs diagnostic,
not full Alpine. Only P01 is positive native boot evidence. P03/P04 are later
product gates, not reasons to enable storage/display during the first trial.

## Limits

A reset terminates subsequent tests. The suite makes no persistent startup
hook; one additional manual launcher action is needed for post-reset evidence.
Wrapper-only signals need not instantly stop a child; child lease cleanup and
the existing watchdog deadline remain responsible for restoration while stock
scheduling works. A runnable process tests one feeding hypothesis; busfreq or
other feeds can still prevent reset. Hardware unknowns cannot all be settled
by source inspection, host simulation, or testing with the old kernel alive.


<a id="history-012"></a>

## Nguồn lịch sử: ours/linux/bootstrap/PLAN.md

# Minimal rescue bootstrap spike — host/emulator only

Ruling: evaluate an independent small p1 bootstrap with Alpine in a p4 file,
instead of erasing p1 to discover recovery. The user authorizes alternative
safe routes and no repeated questions. Bootloader/kernel remain unchanged.
This is a spike, not permission to install its p1 entrypoint.

p1 contains our static init, exact captured USB modules and mount directories;
no Amazon init/services are required by the proposed runtime. A fixed one-shot
file on FAT requests a readonly Alpine ext3 root. Consume/sync the request
before pivot_root; the trial's existing90s guardian then resets to rescue.
Absent/bad root or absent request -> unmount control/image mappings before
exporting p4 over USB. Never export backing storage used by a mounted root.
Recovery after a corrupted p1/bootstrap still requires the separate stock
signed-recovery route; this does not magically repair arbitrary eMMC failure.

- Build static ARM bootstrap; validate ELF and execute --check on ARM emulator.
- Coldboot synthetic p1, absent request: USBstorage driver bound to p4.
- Bad/missing Alpine image: request consumed, safe USBfallback, no stock init.
- Valid Alpine: actualglobalPID1/newroot, guard and reset back to rescue.
- Host evidence/review only; physical RAM/USB acceptance before persistent entry.

Reference: PaperOS native/bootstrap.c (GPL3+, pinned in PAPEROS-REFERENCE.md)
for the one-shot/pivot-root pattern. PW3 differences: no devtmpfs, exact stock
3.0.35, nested FAT offset8192 and existing first-boot guardian.

## Staged first coldboot evaluation

Physical USB serial challenge/response now passed, and normal host eject restores
Wi-Fi with the cable retained. Before any replacement, evaluate a temporary
p1 shell dispatcher while retaining the complete original Amazon rootfs and
exact original init wrapper as fallback. A p1 request is renamed/synced before
executing the experimental bootstrap; after any reset the dispatcher selects
stock init without requiring the Alpine image or bootstrap to work. This is
additional protection for an initial coldboot, not the final Amazon-free OS.
Full p1 erasure remains prohibited until our own cable rescue is proven.

No p1 modification yet: the following host/emulator checks and exact entry
procedure must precede a separate physical cutover decision.
- Normal/default boot selects original stock init.
- Trial consumes request before bootstrap, enters Alpine; reset -> stock init.
- Missing bootstrap / corrupt Alpine: power reset -> original stock init.
- Backup original init exacthash, atomic dispatcher installation and readback;
  precreated directories/modules/image; original bootloader/kernel untouched.
- Manual power-button reset may be needed before Alpine's guardian is armed.
  A p1 filesystem failure is outside this fallback; do not erase p1 in this step.

## Reviewed stock-default entry procedure — 2026-10-30

Ruling: the first persistent entry is a small atomic wrapper replacement, not
p1 erasure. Keep the full, fsck-checked actual p1 backup and original init on
p1. Independent arbitrary-p1 rescue remains unproven; production erase is
still NO-GO. The user authorizes safe native-boot trials without repeated
approval. This bounded first entry keeps kernel, bootloader and all original
Amazon userspace files untouched except the init wrapper pathname.

1. Stage the hash manifest and install-default-trial.sh in device tmpfs;
   validate on the actual CPU, exact original init hash and battery.
2. Save/sync exact original init as init.gleworks-stock; stage exact modules,
   bootstrap, mount directories and hash-pinned one-use return-control job.
3. Atomically rename verified dispatcher over init, with try.once ABSENT.
   Sync/remount root readonly and read back every installed artifact.
4. Verify default coldboot enters original Upstart before arming Alpine.
   A one-use /var/local token starts the existing 1h restricted control session
   after framework readiness; host safe-eject restores Wi-Fi with cable kept.
   The job waits at most180s for the pinned launcher; it adds no permanent SSH.
5. Only then stage/hash Alpine system.ext3 and its FAT request, create p1
   try.once as the final arming action, and perform supervised reboot.
6. Bootstrap --trial arms the previously physically qualified90s guardian
   before FAT/image/USB work. Alpine adopts that deadline without rearming.
   Explicit earlier failures request normal reboot. A stall in the short
   pre-guardian setup interval still has no autonomous deadline.
7. Require fresh serial nonce/global AlpinePID1/root proof, automatic reset and
   authenticated return to original Upstart before further deployment.

Rollback restore-stock-init.sh verifies the exact preserved original, creates
and syncs a replacement inode, atomically restores /sbin/init, removes the
one-use return job/token and remounts root readonly. It does not erase files
or alter bootloader/kernel. A failed transaction before wrapper rename may
leave staged own files; it must never arm a trial. Sync/remount with barrier=0
is normal-reset evidence, not a guarantee against abrupt power loss.


<a id="history-013"></a>

## Nguồn lịch sử: ours/linux/native-display/PLAN.md

# Basic Alpine display and reader

> Current runtime/release status: [native-kit/CURRENT.vi.md](guides/CURRENT.vi.md).
> Stage-specific descriptions and pending gates below may describe earlier trials.

Scope: replace the stale boot image with an interactive, local e-ink reader.
Keep the working native bootstrap, kernel and independent USB recovery.
Prior session instruction authorizes implementation without repeated questions.

Approach: reuse installed KOReader/FBInk and their direct framebuffer/input
support; add a small native launcher and disable Amazon service integration.
A custom desktop adds unrelated dependencies. A status-only screen does not
meet the request to use Kindle for reading. Public generic glibc libraries may
be copied from the existing backup strictly for binary compatibility; no Amazon
daemon, framework, account or jailbreak is started. Alpine remains PID1.

- [x] Inventory actual display/input nodes, driver modules and root capacity.
- [x] Prepare a clean expanded root candidate offline using independent USB
  rescue; retain the exact running root before changing it. Add KOReader,
  compatibility libraries, exact touch modules, and small native launch scripts.
- [x] Validate binary loading on ARM/emulator and framebuffer/init script paths.
  Use cheap existing checks and disposable probes; no new maintained tests.
- [x] Boot candidate with USB monitoring. Verify full e-ink update, reader file
  browser, touch events and a sample document; inspect screenshot and logs.
- [x] Enable automatic reader startup only after the live smoke check; prove
  clean reboot/shutdown with reader running and retain functioning USB console.

UI failure must leave USB administration working and a visible diagnostic when
possible; it must never export user storage while the root image is mounted.
No partition/bootloader changes, Wi-Fi or network service work in this change.

## Packaging extension requested by user

- [x] Maintain one USB maintenance CLI with identity checks, backups, hash
  verification, safe eject, evidence capture and a source archive command.
- [x] Preserve the original investigation, exact source acquisition, stock
  capture/backup, build and staged-install steps with truthful phase gates.
- [x] Document the full reasoning/evidence trail, current boot architecture,
  reader integration, known limits and recovery in Vietnamese.
- [x] Generate and inspect a versioned local source/docs + runnable-image bundle;
  exclude account data, keys, device backups, library contents and screenshots.
- [x] Verify the packaged commands and manifests in a disposable extraction.

Live UI evidence: touchscreen cyttsp4_mt/event1 is present, framebuffer1072x1448
renders Vietnamese file browser, and user confirmed opening/turning book pages.
Initial old-Kindle launcher hash dialog corrected in native.lua by explicitly
bypassing the obsolete stock launcher check for native mode.

Initial accepted release: alpine-reader-v2.ext3 (256 MiB), SHA256
5602b47427f34a9b04d6517bf22a390fee030e35da490242253a508f975fa2ef.
Clean CLI reboot verified fresh boot031ed477 with Alpine PID1, guardian0,
reader PID704 and library framebuffer. Package preflight:171 manifest members,
full SHA verification/extraction, extracted CLI help, offline e2fsck clean.

## Cleanup and RAM logs, 2026-10-30

User requested removal of old Amazon user content and keeping research notes
and history only in the host repo. Kept p1/p2 and the native bootstrap intact.
Re-seeded inactive Amazon p3 from firmware defaults; retained identical e-ink
waveforms. Old KMC payload had immutable flags, removed only within deletion
targets. Shared storage cleanup uses independent USB rescue and archives
remaining guides/logs/snapshots on the host.

Reader v4 routes early and reader logs to /run; the byte-identical proven
shutdown binary is retained, and its handoff record is cleared next boot.
V3 RAM-journal hardware trial did not pass and required button reset to the
independent rescue. Original failed image is preserved on host.
boot.enabled/pending remain persistent safety state.


<a id="history-014"></a>

## Nguồn lịch sử: ours/linux/native-handoff/PLAN.md

# Stock ARM handoff adaptation

Authorized scope: continue native independent Linux boot preparation, retaining
bootloader; Amazon OS disposable, ordinary USB only and avoid bricking.

1. Audit actual captured CPU/cache/MMU reset code and measured RAM layout.
2. Build a standalone position-independent physical relocation stub and a
   preparation/transition C object against exact prepared stock headers.
3. Check blob relocations/size/parameter offsets and allocation overlap guards;
   independently review before integrating any execute-capable module.
4. Remaining pre-device gates: actual kernel staging, device/DMA shutdown,
   watchdog recovery after shutdown/new kernel and native USB observation.

No execute-capable .ko, launcher, flash or device write in this host phase.
Preserve proven load-only module and all prior candidate images unchanged.
Stock reset0xc021cfc0 is only MOV pc,r0 (captured flash confirms source);
cache finish0xc021cfa4 clears I/C/A, not MMU M. Use a dedicated identity-mapped
stub to clear M/I/C and issue barriers before relocation. Existing stock
setup_mm_for_reboot installs 1:1 mappings below TASK_SIZE0xc0000000.

Compressed target0x80008000 with appended DTB. Control page must be owned,
lowmem/identity mapped, outside every image destination and list/source page.
DTB address and relocation chain must be validated before destructive shutdown.
Boot params r0=0,r1=~0,r2=appended DTB (8-byte aligned). New decompressor must
be entered with MMU and D cache disabled. This is not validated by normal
load/readback. Do not call the stub as an ordinary returning function.


<a id="history-015"></a>

## Nguồn lịch sử: ours/linux/native-stage/VERIFICATION.md

# Host evidence — 2026-10-30

- Built new prepare-only ARM32 module/helper against prepared exact stock
  headers using GCC4.9.4. Build log has no warnings/errors.
- package.py completed: five exact input hashes, fixed image SHA256/size,
  ARM ELF, static helper, module imports in captured stock exports, absence
  of transition/restart symbols, and generated shell syntax checked.
- sha256sum -c SHA256SUMS: 5/5 bundle artifacts match.
- Disposable /tmp/native-stage-sha-check.py included the actual sha256.inc
  source with host type/endian adapters. ASan/UBSan passed 33 comparisons
  with Python hashlib: lengths0,1,55,56,63,64,65,4095,4096,4097,1832609 and
  aligned chunk sizes4,64,4096. LSan disabled for host ptrace compatibility.
  This is host algorithm evidence, not ARM runtime or device qualification.
- Reviewed chain.c/h and relocate.S remain byte-identical to native-handoff.
  Proven kexec-load-probe module/helper and native image hashes unchanged.
- Independent integration review found no must-fix defect in allocation role
  tracking, chain-before-dereference checks, fixed SHA256 binding or helper.
- Independent final bundle review found no must-fix defect in guards, hash
  binding, normal cleanup or read-only reader. Minor deferred: output/log
  creation can fail after lock acquisition but before cleanup trap setup,
  leaving /tmp/gleworks-native-stage.lock; no module loaded at that point.
  A reboot clears the temporary lock; do not retry a failed trial blindly.

Current USB mount is readable. No new native-stage runtime capture exists;
this package has not been copied/launched by these host checks. No native
boot attempted, no watchdog armed, no flash or bootloader mutation.

Decompressor layout source derivation is recorded separately in
../native-handoff/memory-review.json; its envelope fits declared RAM but
hardware-reserved memory and peripheral DMA still prevent boot GO.


## 2026-10-30 — gom tài liệu về một nơi

Đã chuyển toàn bộ Markdown do dự án quản lý vào ours/docs; nhập execution,
verification và các plan cũ vào backlog này. Giữ chi tiết theo chủ đề ở
technical/, hướng dẫn ở guides/, dữ liệu kiểm chứng ở evidence/. Không giữ
trang chuyển hướng, bản sao hay symlink tài liệu ở vị trí cũ. Không sửa các
release archive đã phát hành hoặc tài liệu upstream đi kèm mã bên thứ ba.
Không thay đổi thiết bị. Những thay đổi runtime gần nhất vẫn là full refresh
và dọn locale đã ghi trong phần lịch sử trước đó.


Kết quả kiểm tra lượt gom tài liệu: 61 tài liệu chuyển, 15 nguồn lịch sử nhập
vào backlog, 73 liên kết local hợp lệ, các vị trí cũ không còn file/stub/symlink.
Đã sửa đường dẫn đầu vào của 10 công cụ dùng capture/backup và bộ đóng gói để
đọc tài liệu trực tiếp từ docs/. Gói source thử ở /tmp có 181 file: manifest,
SHA256 và CLI --help sau giải nén đều qua. Không đưa evidence/private vào gói.
Đây là kiểm tra host/cấu trúc tài liệu; không build lại OS, không chứng nhận
runtime mới, không sửa gói v4 lịch sử.

## 2026-10-30 — thiết kế bộ cài PW3 cho cộng đồng

Người dùng thống nhất chuyển bộ công cụ thành bộ cài Alpine native; bản đầu
chỉ PW3 đã jailbreak/có root, sau đó chuẩn bị công khai để cộng đồng đóng góp
model/firmware khác. Giữ kernel/bootloader và USB rescue đã kiểm chứng.
Thiết kế chờ duyệt tại
[PW3 community installer](technical/docs/superpowers/specs/2026-10-30-pw3-community-installer-design.md).
Chưa triển khai code mới, chưa ghi thiết bị hoặc publish. Bộ cài lần đầu trên
stock cần bằng chứng riêng; không lấy cập nhật root trên máy native làm thay thế.


## 2026-10-30 — bộ cài PW3 cộng đồng: triển khai source candidate

Người dùng chốt PW3 đã jailbreak/có root và yêu cầu hoàn thiện trước model khác.
Đã triển khai pw3-control.py, pw3-build.py, pw3-install.py, pw3_common.py và
public-source.py; thêm template return launcher riêng, giữ diagnostic worker
cũ mặc định, chỉ bỏ diagnostics khi chọn chế độ control-only. Không ghi thiết bị,
không thay image hoặc paired identity đang chạy, không public/upload.

Luồng mới gồm profile/model/layout/init/battery preflight, backup về host,
bind candidate với backup, dispatcher/default stock return, readonly native,
ảnh lỗi được bảo toàn bản tốt, USB repair/nonce/stock return, promote và final
native/rescue roundtrip. Pending operation ngăn retry mù; reconcile chỉ nhận
postcondition đã hoàn tất. Profile hiện chỉ 0KB/G090KB, không mọi biến thể PW3.

Build riêng đã qua từ gói source giải nén mới, dùng prepared dependencies
được chỉ định rõ; không claim whole-toolchain clean-room. Bootstrap/guardian/
control/shutdown ARM build, QEMU checks, 256 MiB ext3 và 6 inode root-owned qua.
Shutdown mới byte-identical với helper đã chứng minh trước đó. 35 disposable
rejection checks, 2 bound-rescue checks, luồng mô phỏng với 16 shell scripts và
6 CLI help cùng syntax/docs/package checks qua. Không thêm maintained tests.
Review độc lập tìm một lỗi: final rescue mở lại console không gắn đúng máy;
đã sửa bằng giữ connection đã xác thực, lặp identity/session check trước ghi,
kiểm tra session sau rescue; disposable probe đã tái hiện lỗi rồi qua sau sửa.

Hướng dẫn cộng đồng tiếng Anh tại community/README.md, BUILD.md, CONTRIBUTING.md,
LICENSES.md; STATUS.json phân biệt host checks với hardware qualification.
Gói source dùng allowlist 37 file + manifest, không binaries/image/capture/
keys/token header. Private-data scan và source-hash đối chiếu candidate qua.
Không giả định private image có quyền phân phối; giữ giấy phép/nguồn riêng.

Artifact: ours/linux/native-kit/dist/kindle-native-pw3-installer-source-v1-candidate-2026-10-30.tar.gz
SHA256: 5f576d627907ad1aa55a8073823d9d6a82ae2b0f04d2971bcd2aef2a0d16e6be
Bằng chứng: evidence/community-installer-host-checks.json.

Còn chờ nghiệm thu: bộ coordinator mới từ stock PW3 phù hợp qua toàn bộ gate;
reader/touch/đọc nhiều trang sau refresh trên candidate mới. Máy tham chiếu
hiện đã native, không lấy cài lại root làm chứng minh first-install. Full Amazon
restore sau cleanup vẫn chưa được chứng nhận. CURRENT/ACCEPTANCE đã cập nhật.
Không có Git metadata dùng được: giữ input cũ, comparison copies và probes ở
/tmp; không tự khởi tạo Git hay chỉnh release v4 lịch sử.


## 2026-10-30 — Repository layout executed

Separated Alpine/device source, optional KOReader, host tools and ignored local work/dist. Preserved all 332,206 original file/link entries; five backup/release hashes unchanged. Base and reader built from extracted source with external work-dir and explicit prepared dependencies. Passed 35 disposable behavior checks, 27 syntax checks, current-document links and independent review. Evidence: `evidence/repository-layout-checks.json`; guide: `guides/STRUCTURE.md`. No device writes or public upload; fresh first-install and clean-room toolchain remain unqualified. Whole-tree file audit requested subsequently and recorded separately.


## 2026-10-30 — Whole-tree file audit

Inventoried all hidden/vendor/work/archive entries: 331,560 regular files (205,093,673,644 bytes fully read and SHA-256 hashed), 1,696 symlinks, 24,705 directories and one archived emulator socket. Final independent enumeration: zero missing/extra entries; zero file read errors. Private inventory: `work/private/audits/2026-10-30-all-files/`. Public summary: `evidence/all-files-audit-2026-10-30.json`. Finding: 142 legacy work/build symlinks require rebuilding/adaptation before reuse; archived links/rootfs and socket retained. No new device qualification; hashing is not an all-source semantic/security audit. Git metadata appeared during final enumeration and was also fully scanned; no Git mutation performed.


## 2026-10-30 — PW3 USB/autoboot RCA ongoing

See `technical/PW3-USB-AUTOBOOT-RCA-2026-10-30.vi.md`. Preserved original dirty image and markers; recovered journal on copy, verified deployment/readback/eject, native boot and clean reboot, clean rescue/pending removal/rootfs check. Earlier transient USB disconnect remains unexplained. Pending/dirty policy deliberately prevents auto Alpine after hard reset; autonomous recovery and visible rescue state remain design gaps. Not brick, no new first-install qualification, no p1/kernel/bootloader writes. Private continuous monitor/journal under `work/private/hardware-trials/continuous-usb-20261030T064223Z/`.


## 2026-10-30 — Hardware RCA: USB hotplug wake FAIL, clean reboot PASS

Repeated USB unplug/replug loses enumeration while KOReader UI remains responsive. Durable telemetry proves PMIC host/cable=1 with USB clocks0/0 and device uptime advancing. Guarded vendor force-detect pulse ran (two full4-byte writes) but10 follow-up samples remained0/0: workaround rejected. Menu reboot restores native/USB. Exact missed driver wake callback unresolved; no permanent code/kernel/p1 workaround applied. Source kit first-install still not qualified. See `technical/PW3-USB-AUTOBOOT-RCA-2026-10-30.vi.md` and private append-only journal.


## 2026-10-30 — USB fix hardware PASS; bounded crash recovery under qualification

Root UDC module patch passed three same-boot physical unplug/replug cycles with nonce console verification. Console upload now uses bounded chunks and SHA256 before rename; stale-shell reconnect handled. Builder rebuilds the pinned vendor UDC source with strict patch application; public source allowlist includes patch. Fresh55-file extracted source base build passed with prepared dependencies; reader build passed before final pending prevalidation edit. Controller awake power cost unmeasured.

Paired bootstrap now attempts offline preen repair once with battery>=50%, persistent interrupted-repair marker and no gadget export/reset deadline during repair; unsafe cases select rescue. Paired p1 init atomically replaced with host/device original backup and hash readback, early acknowledgment updated. New-bootstrap clean reboot and clean rescue/rootfs verification passed; user confirmed rescue message over old panel background. Physical hard-reset repair still pending. No first-install acceptance, no bootloader/kernel image changes, no publication.


## 2026-10-30 — PW3 runtime repair qualification completed within measured scope

Physical40s reset recovered journal automatically and returned Alpine/KOReader, new boot e640d68f-9bd1-430d-ae0a-96d92a10dbff, guardian0, repairing cleared. Patched UDC synchronized to rescue p1 with backup/readback; clean rescue after crash verified image clean offline. Injected interrupted-repair marker on clean image correctly returned rescue and host resume rejected it; acknowledged only that test marker after clean checks. Fresh extracted base and reader builds passed. All tools Python and tools/PW3 shell syntax plus diff whitespace passed. Remaining: first-install qualification, power measurement, broader hardware/power-loss cases.


## 2026-10-30 — Clean reader candidate deployed; physical qualification in progress

Explicit erase/reinstall authorization received. Verified new reader candidate manifest/OS-app sources/fsck, backed up old p1 pair and clean root, disabled persistent boot before paired p1 replacement, verified new independent rescue before root replacement. Clean root deployment/readback/eject and new native boot passed; runtime guard-control/shutdown/UDC hashes match. Clean rescue/resume and temporary Vietnamese EPUB transfer passed. Reader stable beyond guardian deadline; library framebuffer recorded. Physical book/pages, repeated hotplug and hard-reset recovery remain pending on this exact new candidate. Reinstall from native is not stock-first-install coordinator acceptance. Private trial: `work/private/hardware-trials/candidate-reader-20261030T074953Z/`.


## 2026-10-30 — Clean reader candidate hardware qualification PASS (native reinstall scope)

User confirmed book reading/page turns. Two hotplug cycles re-enumerated with unchanged candidate boot. Physical40s reset replayed AlpinePW3 journal and returned native, guardian0 and repairing cleared. Offline rootfs clean check passed. Invalid placeholder image selected independent USB rescue; preserved clean image hash/restore/fsck and native resume passed. Final reader process verified. Candidate source/pair/root retained under work/build with all payload hashes and symlinks preserved. Public evidence: `evidence/pw3-clean-reinstall-2026-10-30.json`. Stock-first-install remains false; no bootloader/kernel-image change or publication; USB power impact unmeasured.


## 2026-10-30 — Product scope clarified: minimal complete Alpine reader OS

User requires unified Home/library/settings, app/software management, hardware/power, connectivity and updates; chose minimal e-ink UI prioritizing books/apps. KOReader becomes an app. Concrete design: `technical/superpowers/specs/2026-10-30-minimal-alpine-reader-os-design.md`, independently builder-reviewed; pending approval, implementation not started. Existing installer/runtime qualification remains bounded and does not certify this product.

Power investigation: PM devices dry-run and actual mem/RTC20s same-boot passed; disposable adapter checks passed. App-owned power spike was not physically qualified and is not promoted; preserved privately and restored source/live adapter to verified candidate hash. Final clean reboot740f3f93-ad22-41c0-b4af-92f98187cad0, guardian0, KOReader process verified. Trial under work/private/hardware-trials/power-*. No new maintained tests or bootloader/kernel changes.

## 2026-10-30 — Reader OS foundation in progress

Read-only PW3 baseline and disposable host socket probes are recorded in
`evidence/ACCEPTANCE.json`. The service is not deployed: supervised shell PID
authorization, event delivery, hardware ownership handoff, physical button and
LVGL/EPDC/touch qualification remain open. PM/RTC evidence above is kernel
investigation, not acceptance of the OS sleep/wake lifecycle. Preserve the
reader candidate, guardian identity and independent rescue throughout.

The planned separate data image introduces a shutdown dependency: the current
PID1 helper unmounts the root and backing FAT but does not unmount a nested
`/data` filesystem. An opt-in, unbuilt `GW_SEPARATE_DATA` shutdown path has
been drafted; it must not replace the qualified helper until its unmount,
USB rescue and power-loss behavior have been verified on the reference device.

The isolated `work/build/reader-os-trial-20261030/` image contains the first
experimental Home/lock/managed-reader and Wi-Fi path, using the same paired
guardian and private captured firmware. It passed its offline image build and
combined source checks, but has not been installed. Back up current reader
data and establish independent rescue before a trial; Wi-Fi, display/touch,
power, app lifecycle and update paths remain unaccepted.

Subsequently entered independent USB rescue, backed up the live reader root
and verified its hash and offline filesystem, copied reader settings/history
into a separate trial, and installed that trial with the original retained on
USB storage. Boot `55f05ba1-28ff-4d2d-a67c-c010dc8d92da` has BusyBox PID1,
guardian0, USB console, OpenRC, service and shell processes. Device display,
touch, reader handoff and Wi-Fi still need direct acceptance; the previous
reader image remains the recovery path.

The user confirmed the trial Home is visible and launches KOReader. The service
reported `foreground=koreader` on the same boot. Return-to-Home was not found;
the first Exit interaction did not return to Home and the captured framebuffer
shows `Thoát ▸` as a submenu. Return, lock and Wi-Fi remain unverified.

## 2026-10-31 — PW3 E-ink touch/rendering performance scaffolding

- Added monotonic-microsecond instrumentation to `os/ui/lv_port_epdc.h`,
  `os/ui/lv_port_input.h` and `os/ui/gw-shell.c`.
- Added dirty-rect coalescing, LUT-based RGB332->Y8 conversion, 32-row draw
  buffer, and guarded fast-mode state machine (default off).
- Added `tools/maintenance/parse-eink-perf.py` and `PW3-EINK-PERF.md`.
- No hardware waveforms, overclocking or boot-critical kernel changes.

## 2026-10-31 — Ghi rõ định nghĩa sản phẩm "lite OS cho E-ink"

- Thêm định nghĩa sản phẩm vào đầu `docs/README.md` và mục "Mục tiêu và phạm vi"
  của `guides/CURRENT.vi.md`: Linux native tối giản cho E-ink, Alpine nền,
  kernel/bootloader gốc, UI LVGL vẽ trực tiếp framebuffer/EPDC, không
  X11/Wayland/compositor; KOReader là app, không phải OS shell.
- Nêu rõ "tối giản" là không có lớp desktop trung gian, không phải ít tính năng;
  V1 giới hạn PW3 `G090KB` / `5.16.2.1.1`, chưa phải OS E-ink tổng quát.
- Chỉ sửa tài liệu; không thay đổi mã, thiết bị hay boot.

## 2026-10-31 — Power thật, Wi-Fi bền, System UI và signed app manager

- Power: thêm `gw-power-policy` (thuần, có host test), RTC alarm, wakeup_count
  handshake, drain input sau resume, gate cáp/sạc; service tự suspend sau khóa
  và tự hẹn ngủ lại sau RTC wake. Đã nghiệm thu mem suspend + RTC wake thật và
  đường evdev nút nguồn bằng uinput; nút vật lý còn chờ bấm tay.
- Wi-Fi: thêm giám sát link mỗi 5 giây với backoff 5→60 giây; Disconnect tắt
  auto-connect nhưng giữ credentials. Clean reboot đã tự kết nối lại.
- App manager/updater: `gw-appctl`, `gw-signature` (RSA-2048/SHA-256 qua
  libcrypto), bundle `.gwapp` + `.gwapp.sig`, manifest/path/entrypoint checks,
  staging + active symlink, giữ version cũ và appdata; trust key
  `os/system/app-keys/release.pem`, private key chỉ ở host.
- UI: thêm Applications và System update; Settings có lối vào Software;
  host geometry 765 check, 0 fail; đã cài/update/remove bundle qua tap thật.
- Tài liệu: thêm ba evidence 2026-10-31, cập nhật ACCEPTANCE, CURRENT và
  `os/system/APP-PACKAGE.md`; `pw3-os-trial.py` cài appctl/trust key và yêu cầu
  accepted power flags.
- Chưa làm: bấm nút membrane thật, OS A/B update/catalog online, sandbox UID
  riêng cho app, Tailscale/SSH và web upload sau xác thực.

## 2026-10-31 — Hợp đồng APP-SESSION và checklist acceptance

- Thêm [hợp đồng APP-SESSION](technical/superpowers/specs/2026-10-31-app-session-contract.md):
  một foreground app, OS sở hữu phần cứng, bàn giao fb/input qua fd, refresh
  policy tập trung, tín hiệu power, giới hạn tài nguyên trên kernel không có
  cgroup/namespace/seccomp, data/payload tách biệt, ngoại lệ KOReader v1.
- Thêm checklist `AS-01`…`AS-48` (baseline, launch/exit, crash/OOM/fd, tài
  nguyên, hiển thị/input/power, đóng gói, hồi quy/phục hồi, vận hành/bảo mật)
  và quy tắc bằng chứng/điều kiện dừng.
- Trạng thái: DRAFT để duyệt; chưa sửa mã, chưa chạy trên thiết bị. Thêm B07
  vào công việc còn mở.

### 2026-10-31 — Sửa lỗi nút nguồn chỉ tắt đèn, không khóa

- Nguyên nhân: service đặt `lock=locked` và tắt frontlight nhưng không báo cho
  shell; shell chỉ hiện lock screen qua callback nội bộ.
- Đã thêm event `POWER LOCK/WAKE/UNLOCK` trên `/run/gw-system.evt`; shell kết nối
  và xử lý trong render loop, đồng thời đọc `lock=` từ STATUS lúc khởi động.
- Đã nghiệm thu trên máy: bấm nút → `screen=lock`, đèn tắt; bấm lại → đèn sáng,
  vẫn `screen=lock`; Unlock → `screen=home`. `gwctl ui-screen` dùng để xác nhận.
- Khi KOReader foreground: plugin `gw-home` thấy `lock=locked`, quit hẳn reader
  (không restart UI), service khởi động shell mới và shell sync STATUS để hiện
  lock screen. Đã nghiệm thu `foreground=koreader` → `foreground=none`,
  `screen=lock`, đèn tắt; Unlock → `screen=home`, đèn khôi phục.
- Lần bấm nút vật lý đầu tiên (người dùng): xác nhận **lock screen + đèn tắt**
  và wake/Unlock, nhưng chưa vào mem suspend vì `max77696-charger/online` còn
  báo `charging=1` sau khi rút cáp. Đã sửa gate: `usb=none` là căn cứ, bỏ qua
  charger trễ; STATUS báo `battery` khi không cáp. Đã kiểm chứng lại đúng điều
  kiện cũ `usb=0 charging=1` → `PM: Entering mem sleep` bằng uinput.
- Capture `/usr/sbin/gw-power-capture` đã chạy lại; cần một lần bấm vật lý nữa
  để ghi `physical_button_suspend_confirmed=true`.

## 2026-10-31 — Review cập nhật lớn và điều chỉnh kế hoạch app runtime

- Review codebase/evidence sau đợt cập nhật: signed `.gwapp` installer,
  Applications/App updates UI, mem suspend + RTC wake, Wi-Fi auto-reconnect,
  idle/log optimization và reader performance đã có bằng chứng thiết bị.
- Xác định khoảng trống mới: `APP LAUNCH` chỉ nhận `koreader`; app đã cài chưa
  có đường chạy; app/Reader còn chạy root; chưa UID riêng, `RLIMIT_*`,
  fd handoff, crash/OOM, data image.
- Cập nhật [APP-SESSION](technical/superpowers/specs/2026-10-31-app-session-contract.md):
  trạng thái triển khai, đường dẫn `/var/lib/gleworks/...`, định dạng `.gwapp`,
  ngân sách RAM theo số đo, bảng trạng thái `AS-*`, quyết định D4 đã chốt.
- Thêm [app runtime plan](technical/superpowers/plans/2026-10-31-app-runtime.md)
  với phase 0–7, cổng thoát, `gwtest`, rủi ro và định nghĩa hoàn thành.
- Thêm status pointer vào kế hoạch OS 2026-10-30; không sửa lịch sử checkbox.
- Chỉ sửa tài liệu; không chạy phase nào, không thay đổi mã/thiết bị/boot.

### 2026-10-31 — Paint/transition policy và xác nhận suspend bằng nút vật lý

- Bấm nút vật lý thật (sau khi sửa gate `usb=none`): lock screen → mem suspend
  ~76 giây → bấm lại wake, đèn về mức cũ, vẫn khóa → Unlock. Capture log giữ
  `PM: Finishing wakeup.`; user reset để phục hồi USB enumeration.
- Paint: screen transition dùng DU4 (4 mức xám) ngay trong event cycle, cleanup
  full-screen bằng REAGL sau 600 ms yên tĩnh thay vì GC16 flash.
- Section isolation: chỉ vùng ≥5% màn hình mới tính vào budget partial → full;
  feedback nút nhỏ không còn kéo theo full-screen cleanup; dirty-rect vẫn tách
  rời từng section. Áp dụng chung cho Home, Settings, Calendar, Notes, Network,
  Applications, App updates, Lock.
- Đo trên máy bằng uinput evdev: release 80 ms (trước ~692 ms), transition
  `input_to_submit_us=75890`, cleanup `waveform=8` (REAGL) lúc 719 ms.
- Tài liệu: `docs/technical/PW3-PAINT-POLICY.md`; shell production SHA256
  `434b090f3199ee7193486d126c1acd81912a55fe5ab446c32590b4abaeedc2b1`.

## 2026-10-31 — Review đợt cập nhật paint policy và nút vật lý

- Xác nhận thêm bằng chứng thiết bị: nút nguồn vật lý thật đi đủ
  lock → mem suspend → wake → Unlock; paint/transition policy DU4 + section
  isolation đo ~76 ms và đã áp dụng cho mọi màn hình.
- Thêm `uinput` touch/power vào kế hoạch như công cụ test lặp, không thay thế
  xác nhận trải nghiệm vật lý cuối.
- Cập nhật APP-SESSION: trạng thái AS-28 đã đạt; AS-26/27/29 vẫn một phần;
  tham chiếu paint policy cho `libgwdisplay`.
- Cập nhật app runtime plan: bỏ mốc chờ nút membrane, thêm tái sử dụng policy
  và injection tooling.
- Cập nhật CURRENT: bỏ câu “chờ bấm nút thật” đã lỗi thời; ghi paint policy.
- Chưa thay đổi mã/thiết bị; generic app launch vẫn là khoảng trống chính.

## 2026-10-31 — Ghosting/flicker sau paint policy; khảo sát cơ chế waveform

- Người dùng báo ghosting rõ và flickering rõ dù chuyển màn nhanh hơn.
- Xác định nguyên nhân trong code: mỗi scene chạy DU4 rồi deferred full REAGL
  sau ~600 ms → hai lần vẽ toàn màn; DU4 4 mức xám để lại residue.
- Thêm policy runtime trong `os/ui/gw-shell.c`:
  `quality` (mặc định, một GC16 mỗi scene), `balanced` (DU4 + GC16 mỗi 4
  scene), `fast` (DU4 + REAGL như cũ). Chọn bằng
  `/etc/gleworks/paint-policy` hoặc `GW_UI_PAINT_POLICY`.
- Thêm [EINK-UI-RENDERING.vi.md](technical/EINK-UI-RENDERING.vi.md): cơ chế
  A2/DU/DU4/GC16/GL16/REAGL, trade-off ghost/flash, quy tắc một update mỗi
  scene, pre-render, on-release scroll, UI đơn sắc, test protocol.
- Cập nhật paint policy, APP-SESSION, app runtime plan và CURRENT.
- Chưa deploy lên máy; cần build/armed và người dùng test quality vs balanced.

## 2026-10-31 — Kế hoạch paint quality đầy đủ

- Thêm [paint quality plan](technical/superpowers/plans/2026-10-31-eink-paint-quality.md):
  mục tiêu, acceptance PQ-01…PQ-12, 8 phase, ma trận test PT-01…PT-12,
  rollback, rủi ro và thứ tự thực hiện.
- Gắn plan vào CURRENT và app runtime plan.
- Trạng thái source: policy modes + coalesce transition đã có; ARM build
  `2803d57…`; chưa deploy, chưa đạt PQ nào.

## 2026-10-31 — Paint quality đo trên PW3; quality là default

- Đo trên máy boot `186d6571…`: 20 scene `quality` = 20 GC16 submit;
  `balanced` = 20 DU4 + 5 GC16; `fast` = 20 DU4 + 20 REAGL (tái hiện flash thứ
  hai).
- Sửa feedback bàn phím: vùng bàn phím dùng A2, phím phản hồi ~68 ms; 20 ký tự
  không full GC16; đóng bàn phím mới cleanup. DU không dùng cho vùng >5%.
- Handoff reader→lock chỉ một GC16; lock/unlock 5 vòng và suspend-test 5 lần
  không loop; reboot vào lại build/policy.
- Soak 50 scene: 51 submit, fd 7→7, RSS 656→680 kB; idle 5 phút 5 submit.
- Build cuối `0163000e…`, policy `quality`, perf off/diag on; evidence
  `docs/evidence/paint-quality/`.
- Còn chờ người dùng chấm ghost/flash trên panel và xác nhận quality vs
  balanced; rescue chưa chạy lại sau build cuối.

## 2026-10-31 — Rescue/resume sau build paint cuối

- Chạy `kindle-linux.py rescue` → USB rescue độc lập, rồi `resume` → Alpine.
- Boot mới `ae4fa08e-4d3c-468e-b339-387773498842`; `gw-shell`
  `0163000e…` và policy `quality` vẫn còn; Home/status bình thường.
- Còn 3 mục cần người dùng chấm trên panel (ghost/flash, quality vs balanced)
  và quyết định công bố default.

## 2026-10-31 — Audit và cập nhật PW3 UI polish plan

- Audit source thật: host preview 760 check/0 fail, heap 158912/262144 (61%),
  peak 63374; ARM `gw-shell` 2,626,560 byte; tokens hiện body28/caption24.
- Ghi nhận: nav chỉ 3 callback và còn trên secondary screens; Home còn hero/
  Quick Access/nút lồng; Settings còn slider; Calendar tiếng Việt cell 70;
  To-dos TSV 5 field; Apps có 2 primary; font thiếu 5 codepoint; `os/ui` có
  binary/core chưa ignore.
- Sửa plan: audit baseline, ASCII copy, primary map, Light/Lock one-tap,
  Lock không Back, modal một GC16/lifecycle, baseline commit sạch, test 760,
  NET plain/hex, font verify, paint-quality regression gate, definition of done.
- Chưa sửa UI; các mục cần người dùng xác nhận: English-only.

## 2026-10-31 — Balanced comparison và host debounce/long-list evidence

- Balanced trên máy: 20 scene = 24 EPD update (quality = 20); balanced dùng
  20 DU4 + khoảng 5 GC16, ít full refresh hơn nhưng chưa có điểm ghost/flash
  từ người dùng. Vì vậy checkbox so sánh trực tiếp vẫn mở.
- Host harness: 789 check/0 fail; thêm 8-app fixture (last row scroll) và
  rapid-status check (10 update không đổi text → 0 label write).
- Evidence: `docs/evidence/pw3-ui-audit-2026-10-31/`,
  `docs/evidence/paint-quality/README.md`.

## 2026-10-31 — Balanced bị loại; quality là default cuối

- Người dùng thử balanced: màn bị bể/không hiển thị đủ thông tin, tệ hơn quality.
- Đã chuyển máy về `quality`; `GW_SHELL: paint policy=quality`, shell mới chạy,
  Home/status bình thường.
- Chốt: `quality` là default, `balanced` bị loại khỏi đường phát hành.
- Paint-quality plan: 0 task mở.

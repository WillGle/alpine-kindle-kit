# PaperOS reference audit — 2026-10-30

Public reference https://github.com/ChrisPei/PaperOS at
8dc24d6278c120eb9c69ceddc470f62eb15cbb79 (GPL-3.0-or-later), host source only.
No downloaded project scripts/installers have been run. README claims real KT3
native boot with stock3.10.53 and independent ext3 image; source confirms a
concrete implementation, not proof for our PW3 with stock3.0.35.

`native/bootstrap.c` is a staticPID1 bootstrap: opens p4 via an8192-byte loop
mapping, consumes a one-shot enable file, mounts an ext3 image, moves proc/sys/dev
and control mount, pivots root, detaches old root, closes oldfds and execs newinit.
Missing enable/image returns to retained stockinit. A newOS guardian re-arms the
next boot only after success. Installation atomically replaces /sbin/init after
backing it up; it does mutate p1 and is not an untouched-stock-root solution.
This fallback still depends on stock p1 and is not independent USBROM rescue.

PW3 differences requiring adaptation: DEVTMPFS disabled, exact3.0.35 moduleABI,
boardIDMuscat and panel/PMIC details, early /dev nodes, loop offset from actual
userstore, and pre-guardian failures. Do not copy KT3 drivers/images. Existing
64MiB Alpine ext3 candidate is host-verified/actualBusyBox+apkABI only; not a
bootstrap/coldboot qualification.

Reference lock also identifies public katadelos/qemu commit
b1a5347b628f13afe88454a096b5c3c78d84da5c and eink-emulator commit
6599bb52da300a893dbf0967a2f6e0bba0969fd7. Source/docs confirm imx6sl-wario,
512MiB, machineID4091, MuscatID detection, exactlegacy3.0.35 and extraLab126ATAGs.
Build host emulator then test exact capturedstock kernel against syntheticdisks;
never boot private full-device captures or publish identities. Emulator results
can qualify code/error paths, not realUSB/eMMC power-loss/hardware rescue.

Candidate progression: emulator direct Alpinecoldstart; earlybootstrap +
one-shot image/fallback; simulated failed/missingAlpine; RAM-only hardware
bootstrap/USBrecovery qualification; separately reviewed persistent entry.
Keep bootloader/kernel/IDME untouched throughout current investigation. Amazon
fallback is temporary, not final achievement; goal still requires Alpine
native operation and removal of AmazonOS after independent rescue is usable.

Subsequent execution: emulator coldstarts Alpine using actualstockkernel and
exactstockbootargs, no init override. Stockkinit's pre-chroot find_init rejects
absoluteinit symlinks; relativeinit works. Correctedcandidate's diagnosticcopy
proves BusyBoxPID1, Alpine3.24.3, sysinitRC0, armedguardian and USBserialdriver
ready. This advances defaultcoldboot code qualification, not physicalboot.

Missinginit/corruptroot negativecontrols panic instead of exportingUSB. A
malformedupdate bundle is attempted before kinit even with corruptp1, rejected
and deleted then rebooted. Embeddedrescue cannotyetbe claimed independently
usable over ordinarycable; officialsignedFB03restore is nextconcretequestion.

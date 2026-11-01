# Minimal Alpine reader OS implementation plan

> **For agentic workers:** Use superpowers:executing-plans, directly in this session. User authorized scope; no new maintained tests. Preserve all existing qualified candidates and paired device identity; no unrelated cleanup or publication.

**Goal:** Deliver a minimal Alpine-based OS for PW3 with unified hardware ownership, one application-management Home view, one lock screen, app lifecycle, power management, Wi-Fi, and A/B OS update — replacing direct KOReader boot while keeping all existing USB recovery and rescue paths intact. The library belongs in an application; its further product design is deferred.

**Architecture:** C system service owns hardware (power button, display, suspend, Wi-Fi); LVGL UI shell drawn directly to framebuffer/EPDC via evdev; KOReader launched as a managed app with an adapter for return-to-Home. BusyBox PID1 and OpenRC for service ordering; apk for Alpine dependencies; app packages use `.apk` + manifest with separate data roots. No cloud accounts, no remote lock, no Amazon userspace.

**Tech stack:** C (LVGL, evdev, Unix sockets), POSIX shell, Python host tools, OpenRC, wpa_supplicant/udhcpc, apk, e2fsprogs; existing kernel/bootloader/modules retained.

**Spec:** ../specs/2026-10-30-minimal-alpine-reader-os-design.md (builder review passed; no open questions before planning).

**Note:** The native power spike (2026-10-30-pw3-native-power plan) is superseded by this design; Lua power adapter retained only as a private spike until this OS layer accepts the button.

**Status pointer — 2026-10-31:** phần lớn Milestone 1–3 đã có bằng chứng thiết
bị: `gw-system` sở hữu power/Wi-Fi/app install, signed `.gwapp` installer +
Applications/App updates UI, mem suspend + RTC wake, **nút vật lý thật
lock → suspend → wake**, Wi-Fi auto-reconnect, paint/transition policy DU4, và
idle/log đã tối ưu. Xem
[app runtime plan](2026-10-31-app-runtime.md) cho phần còn lại. Checkbox bên
dưới giữ nguyên lịch sử; trạng thái hiện hành đọc từ plan mới,
[APP-SESSION](../specs/2026-10-31-app-session-contract.md) và
[CURRENT](../../../guides/CURRENT.vi.md), không suy từ checkbox cũ.

## Global constraints

- PW3 G090KB / kernel 3.0.35-lab126 / firmware reference only; do not claim support for other models.
- Preserve bootstrap/guardian identity, BusyBox PID1, independent p1 USB rescue and all qualified candidates throughout.
- No repartitioning; no bootloader/kernel changes; no replacement of vendor modules outside a separate port project.
- No new maintained test suite; disposable probes and syntax checks only.
- No A/B update manager, auto-suspend default, or Tailscale until each underlying layer is accepted.
- Do not publish binary/keys/backups; do not mark any OS function accepted without real-device evidence.
- Do not rename or restructure the repository again.

## Review focus

- Hardware ownership must transfer cleanly from app-direct to OS service before UI is built.
- LVGL/EPDC/evdev spike must run on real 8 bpp / pitch-1088 hardware before driver is locked in.
- App crash must return to shell with readable error, never boot-loop.
- Power/network commands must route through the service socket; UI and apps must not own hardware directly.
- Package install must check signature/hash/manifest/space before touching the active version.
- A/B slot must not be marked good until service, shell and USB paths verify; startup failure must roll back.

## Milestone 1: OS foundation and hardware contract

- [ ] Move hardware ownership (power button, frontlight, suspend, USB state) from KOReader/app layer to a stub system service; verify existing USB rescue and BusyBox PID1 shutdown remain intact.
- [x] Audit OpenRC service ordering against current mount/shutdown sequence; identify any ordering conflicts before enabling OpenRC as service manager. Host audit and remaining device gate: [ordering audit](../specs/2026-10-30-openrc-ordering-audit.md).
- [x] Run PM devices dry-run, RTC-bounded suspend/wake, button-event capture and USB reconnect probes; record baseline measurements with sysfs/dmesg evidence. PM logs: `work/private/hardware-trials/power-20261030T081722Z/`; reconnect evidence: `docs/evidence/pw3-clean-reinstall-2026-10-30.json`; button event: execution record below. These baseline probes do not accept the new power lifecycle.
- [ ] Define Unix socket IPC contract (fixed command set, size-bounded, PID-checked) between service and future UI; document in os/system/.
- [ ] Update CURRENT and ACCEPTANCE with hardware-contract evidence; append BACKLOG with any remaining qualification limits.

## Milestone 2: Shell and reader integration

- [ ] Build a disposable LVGL/EPDC/evdev spike on real PW3 hardware; confirm 8 bpp framebuffer and pitch-1088 rendering before locking the driver. If it fails, re-evaluate renderer choice rather than masking the failure.
- [ ] Implement one application-management Home view with installed apps, available management actions, Wi-Fi controls and a shared status bar (battery/charge/network); wire hardware controls to the system service API, not direct sysfs.
- [ ] Implement one lock screen; the library is supplied by an app, with further library-app design deferred. Preserve access to the existing KOReader library.
- [ ] Implement KOReader app adapter: launch via OS app lifecycle, expose return-to-Home action, save state before exit; adapter must not manage Wi-Fi, power, or OS updates.
- [ ] Verify: open book → reader → exit → Home without KOReader respawning; app crash → Home with readable error.
- [ ] Do not build simulated screens disconnected from backend and call them complete.

## Milestone 3: App manager

- [x] Design app package format: `.apk` + manifest (ID, version, entrypoint, supported formats, required hardware permissions, compatibility declaration); document in os/system/. See `os/system/APP-PACKAGE.md`; signing envelope, installer and device acceptance remain open.
- [ ] Implement install pipeline: download/read to staging → verify signature/hash/manifest/space → extract to new version root → check entrypoint → swap active symlink; failure keeps old version, never deletes user data.
- [ ] Implement update and uninstall: uninstall retains data by default; data deletion is a separate confirmed action.
- [ ] Enforce: only foreground app gets display/touch; UID isolation and read-only payload confirmed via DAC on kernel 3.0; no namespace/seccomp promises.
- [ ] Validate rejection paths: wrong arch/profile/signature refused before touching active version; power-loss and out-of-space handled gracefully.
- [ ] UI shows only features with accepted backend; do not surface install/update UI before pipeline passes real-device probe.

## Milestone 4: Power and connectivity

- [ ] Validate full suspend/wake cycle (short press → save state → lock input → finish EPDC refresh → mem suspend → wake → restore) across multiple iterations in reader and Home states, with and without cable.
- [ ] Implement USB plug/unplug handling: admin cable locks screen, keeps kernel awake for console and file transfer; cable removal from sleep transitions to suspend after buffer.
- [ ] Validate Wi-Fi: wpa_supplicant/udhcpc on held kernel; credentials stored 0600, never logged. Settings UI owns connection management; KOReader must not run Amazon Wi-Fi tools.
- [ ] Implement book import over authenticated LAN: size limit, safe filename validation, completion check before library entry; no shell/delete/install API exposed over LAN.
- [ ] Record power current/charge measurements in key states (sleep, locked screen, reading, charging); note UDC fix impact.
- [ ] Enable auto-suspend timeout only after button, RTC, charge, touch, reader and USB reconnect pass multi-cycle validation; leave configurable, not default-on before acceptance.

## Milestone 5: OS update and recovery

- [ ] Design A/B slot layout within existing userstore partition; measure and fix image sizes before committing; document in os/system/.
- [ ] Implement update pipeline: fetch unified signed release → verify signature/version-compat → write inactive slot → hash/fsck → trial boot → mark good only after service, shell and USB verify; startup failure triggers automatic rollback to last good slot.
- [ ] Implement config/data migration: migration must preserve rollback path or config snapshot; never delete books or user data as part of OS rollback.
- [ ] Validate power-cut safety: metadata selects active slot; sync ordering and power-cut behaviour must be designed and probed, not assumed.
- [ ] Verify: both slots corrupt → drops to independent p1 rescue with visible message.
- [ ] Validate USB rescue and fresh install from Amazon OS using updated installer; confirm bootstrap/guardian identity is preserved through normal OS update.
- [ ] Do not describe any of these as safe/atomic before power-cut probe and real-device evidence.

## Milestone 6: Public v1

- [ ] Conduct extended real-use and battery-life testing; record results with device/build identity.
- [ ] Write English contributor documentation: hardware support matrix with evidence levels; reject install outside qualified config.
- [ ] Produce clean source package: allowlist-based, excluding backups, private evidence, keys, tokens, serials and user data. Verify manifest/extraction from empty directory.
- [ ] Audit license/provenance for every component; note any non-redistributable captured components.
- [ ] Publish only functions with PW3 evidence; do not release vendor binaries, keys or default backups.
- [ ] Update CURRENT, ACCEPTANCE and BACKLOG to reflect v1 boundary; do not overwrite historical acceptance records.

## Execution record

Plan created from accepted design spec; no implementation performed yet. Native power spike superseded; qualified candidate on reference device unchanged. All milestones pending first real-device evidence.

Latest user clarification: the machine needs Wi-Fi; Home is a single view for
application management and the device has one lock screen. The library is an
app, with further app planning deferred. This supersedes the earlier Home
recent-books strip and separate OS library screen; it does not remove the
remaining hardware, recovery or update requirements.
The Wi-Fi control must scan and display networks for selection; manual SSID
entry is not the requested primary flow. Credentials are entered on the device.

2026-10-30: Read-only USB console inspection of the running reference candidate
confirmed BusyBox PID 1, kernel 3.0.35-lab126, persistent boot, KOReader,
USB-ready, framebuffer mode 1072x1448, 8 bpp and 1088-byte stride; power
button and touch appear as max77696-onkey and cyttsp4_mt input devices. Boot
ID was 740f3f93-ad22-41c0-b4af-92f98187cad0. This does not validate LVGL,
event capture, hardware ownership transfer or suspend. The unintegrated
service skeleton's host syntax check initially failed because GNU socket
declarations were unavailable; enabling the GNU feature macro corrected that
host check. No device software was changed. The existing source-only installer
package passed its 55-file manifest and empty-directory extraction check; it
does not include or qualify the new OS skeleton.

The skeleton's frontlight path was corrected to the existing PW3 backlight
interface. A read-only device check reported brightness 512 of 4095, battery
99%, status Full and charger online. Host syntax checks passed with the light
write disabled and enabled. A static ARM build passed using musl and Linux
6.18 UAPI headers; the resulting binary has not been run on PW3, and those
headers are not evidence of compatibility with the retained 3.0 kernel.

A disposable host socket probe exposed and then verified fixes for fragmented
requests, idle-client blocking and duplicate service startup. It did not
establish supervised-shell authorization or event delivery. The first physical
button capture timed out at the host; its orphaned reader was stopped. A second
read-only capture recorded EV_KEY/KEY_POWER down and up, 2.391 seconds apart,
on the same boot. This establishes event delivery only, not short-press policy
or suspend/wake. The unintegrated EPDC stub was corrected against the retained
kernel ioctl layout and RGB332-to-Y8 conversion; neither change has been
tested on the physical display.

The rebuilt static ARM service was uploaded with a disposable socket client,
hash verified, and run temporarily on the same PW3 boot while KOReader kept
running. It opened the button device, returned live battery/light/charging
status, and refused SUSPEND and APP EXIT. It was stopped and all probe files
and sockets were removed; the reader remained running. This is a stub probe,
not transfer of hardware ownership or shutdown/rescue acceptance. LVGL 8.3.11
commit `74d0a816a440eea53e030c4f1af842a94f7ce3d3` was obtained for the spike;
host and static ARM builds succeeded, but the spike has not run on PW3.

Read-only storage observation on that boot: the existing userstore loop has
3,188,640 KiB usable and 2,139,576 KiB available; the active ext3 image is
256 MiB, with 85,857 KiB used inside it. Three other 256 MiB images remain
preserved on userstore. These are current measurements, not a slot sizing or
space reservation; app data, update staging and power-loss metadata still
need measurement and design before A/B implementation.

An isolated copy of the reader root now has signed Alpine OpenRC packages
installed with package scripts disabled; its runlevels are empty and it has
not been booted. The current image is untouched. Two concurrent touchscreen
captures returned no events while the user confirmed that the reader responded.
A temporary EVIOCGRAB probe returned EBUSY on event1, showing an existing
exclusive grab. The reader-to-shell transition must release that grab; the
shell's touch handling remains unverified.
The service's gated light-set operation passed a disposable host file probe
covering authorization, range rejection and readback. A temporary PW3 spike
returned OK and changed raw brightness 512 to 1024 for LIGHT SET 25. The
probe script exited nonzero at shutdown; its cleanup restored 512, verified
in a separate console session along with removal of the probe and sockets.
This is neither visual acceptance nor hardware ownership transfer; writes
remain disabled in normal builds. The proposed app
package contract is recorded in `os/system/APP-PACKAGE.md`; signing envelope,
installer and isolation remain unfinished.

The service now reports the PMIC cable classification from `/proc/asession`.
Disposable parser probes covered missing, inconsistent and malformed values;
a temporary PW3 run reported `usb=host-class` with the cable attached, then
exited cleanly. That PMIC class is not proof of USB enumeration and is not yet
used to control sleep. The reader was still running and probe files were gone.

Work in progress: the managed-reader adapter and fixed-command `gwctl` client
have been drafted so a future supervised reader can obtain power and light
state through the service. They are not integrated into the image or tested;
the current reader continues to use the qualified legacy path. Shell PID
authorization, foreground lifecycle and device handoff are still required.

An experimental, disabled-by-default lifecycle path now starts the shell as a
service child, accepts the fixed reader launch only from that shell PID, waits
for shell exit before launching the reader, and starts the shell after reader
exit. The reader adapter adds a Home action and routes available status/light
requests through the service. These changes have not yet been integrated or
verified and do not establish UID isolation, safe power handling, a complete
library, or real display/touch handoff.

An offline trial-image builder has been drafted to copy a prepared reader root,
retain the paired guardian/shutdown/USB modules, replace reader respawn with a
separately started OpenRC service, and install the experimental shell and
managed reader. It has not run. In particular, a copy of the live reader data
and its migration/rollback proof are required before any such image is used.

The app manifest parser, empty data-image creator, guarded data mount helper,
and opt-in nested-data shutdown path are drafted but have not been checked or
enabled. Data image sizing, migration, independent recovery and app UID/DAC
enforcement remain open; none of these drafts is an accepted feature.

The private trial `work/build/reader-os-trial-20261030/` was built from the
paired reader candidate and includes the experimental Home/lock lifecycle,
Wi-Fi backend, captured private Wi-Fi modules and firmware. ARM builds and a
single combined source/syntax check passed; the image builder's offline ext3
check also passed. This is not device acceptance. The live reader remains on
boot `740f3f93-ad22-41c0-b4af-92f98187cad0`; the trial has not been installed.
An independent rescue and a backup of the current reader's data are needed
before replacing its root. The Wi-Fi trial exposes a capability marked
`trial`, so its setup is deliberately hidden from the ordinary Home until it
has been validated on the reference PW3.

Subsequently, independent USB rescue was reached and the live reader image was
backed up and checked clean offline. A new trial containing copied reader
settings/history was installed while preserving the prior image on userstore.
The trial booted as `55f05ba1-28ff-4d2d-a67c-c010dc8d92da` with BusyBox PID1,
guardian0, USB console, OpenRC, system service and shell processes. Display,
touch and reader handoff still need visual/interactive evidence; Wi-Fi and
other milestones remain unaccepted. The userstore backup and private host copy
preserve the prior reader image.

User confirmed the Home is visible and opens KOReader; the service reports
foreground `koreader` on that boot. User did not find Return to Home and the
first Exit interaction did not return; the captured screen shows `Thoát ▸`,
a submenu. The return path, lock and Wi-Fi are still pending device evidence.

The user subsequently pressed the power button: KOReader exited with status 0,
released its input, and the shell displayed the lock screen on the same boot.
The user confirmed Unlock returns to a responsive Home. This does not qualify
suspend, the explicit reader Exit action or crash recovery. A new shell with
Wi-Fi trial controls was installed for the next Home launch; its previous
binary is retained on the device. Wi-Fi connection remains unverified.

The user confirmed the Wi-Fi button is visible and requested a scanned network
list rather than manual SSID entry. The existing wpa_supplicant scan interface
and LVGL dropdown have been wired in source; a combined syntax check passed.
These changes are not yet on the device and the scan/connection remains
unverified. No network password was requested or recorded in the conversation.

Subsequently the scan UI and service were installed on the experimental device
with previous binaries retained. Initial scans failed because the kernel
hotplug path was empty and the old firmware loader required direct writes.
After enabling the helper and using BusyBox `dd` for firmware transfer, ath6kl
created `wlan0`, wpa_supplicant returned scan data and the user confirmed a
network list. The shell was updated to handle selecting the first network.
Connection, credential persistence and automatic reconnect are unverified.

User deferred Wi-Fi and requested a high-contrast e-ink Home resembling the
provided three-column application grid, with larger text and better response.
The trial shell now uses a black/white grid, larger font and keyboard, and
queues display updates instead of waiting for every refresh. It was built and
installed on the trial; the prior shell remains at `/usr/sbin/gw-shell.before-grid`.
Framebuffer capture shows the grid at 1072x1448; perceived sharpness, touch
latency and display handoff with queued updates still require device feedback.

2026-10-31: Source tracing showed KOReader's built-in Exit event routes through
`DeviceListener:onExit()` and closes the reader to KOReader's own file manager;
it does not quit the managed process for the OS shell. The Home plugin now
replaces the submenu's Exit action with KOReader's `exitOrRestart()` save/close
path followed by `UIManager:quit()`. Host Lua compiler is unavailable; device
LuaJIT accepted the plugin via `assert(loadfile(...))`. The update is installed
with SHA256 `73cd0f11caf65e666bbadfa58f4747e4d8666040d8084690d7a3b12dced67c17`;
the previous file is preserved as
`/opt/koreader/plugins/gw-home.koplugin/main.lua.before-home-return-20261031`.
The reader has not been reopened since deployment, so behavior remains pending
one user-facing check.

After the user reported “Checking storage — keep power connected” following
two resets, USB enumerated as native Alpine console `0525:a4a7` on `/dev/ttyACM0`.
Nonce-verified read-only inspection confirmed Alpine 3.24.3, BusyBox PID 1,
boot ID `03319c70-5a61-4fd8-b8c1-6095030127ee`, read/write ext3 root, and running
`gw-system --supervise` and `gw-shell`; status was `foreground=none`,
`lock=unlocked`, `display=on`. A framebuffer capture showed the Home grid while
the user still saw the older Kindle storage-check image. FBInk then reported a
completed full-screen GC16 update. The resulting panel image is not yet
confirmed by the user; this verifies Alpine access and an update request, not
visual or lifecycle acceptance.

After the user started `tailscaled` in userspace mode and attempted
`tailscale up --ssh`, an initial host check saw `alpine-reader` at
`100.100.178.75` offline and USB nonce handshake timed out. On recheck, USB
nonce inspect succeeded on the same boot ID; PID 4720 `tailscaled` was running,
device `tailscale ip -4` returned `100.100.178.75`, device `tailscale status`
listed the host, and `gwctl status` reported `net=connected`. Host status now
reports the peer online. SSH reached the Tailscale SSH endpoint but requires
an interactive browser auth check; no shell has been established. The
one-time check URL was sent to the user and is not stored here.

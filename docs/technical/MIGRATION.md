# Installation verified after reconnection — 2026-10-29

The user ran GleWorks Install and reconnected USB. The installation log
`gleworks-install-20261029-022009-6321.log` reports successful completion.
Readback verified all 1,048 installed release files against the prepared bundle,
with zero missing/changed files. Four balanced profile values and its original
settings backup are present. The active KPM database retains only `kpm` as an
installed package and `kindlemodding` as a repository; foreign-key check is clean.

Original reader, package wrappers, launchers, private old app state and consistent
KPM snapshot remain at `/mnt/us/gleworks-quarantine-20261029022009-6327`.
The old `unlocked` state and replaced package directories are absent from their
active paths. No reset, firmware or kernel replacement occurred.

The log confirms native Lua modules loaded, HTTP service started/stopped, and
Tailscale 1.104.4 executed and its daemon started/stopped. It does not establish
VPN enrollment, real HTTP requests, reader launch/lifecycle, sleep/wake, reboot
or measured screen performance. Complete the acceptance steps in README before
revoking the old VPN identity or deleting the secret-bearing quarantine.

## Previous preparation status (historical)

# Current migration status — 2026-10-29

**Source implementation complete; actual release preparation, installation and
hardware acceptance pending. No old device app has been removed by this session.**

Implemented in `scripts/prepare.py`, `scripts/stage.py`, `scripts/install.sh`,
`scripts/rollback.sh`, and `device/package-records.lua`:

- Official-release selection, published-digest verification, ARM ELF/layout
  checks, bounded archive extraction, bundle checksums and USB readback.
- Root scriptlets and preflight for the verified reader runtime, transfer
  server and VPN. The Go prototype is excluded from the device bundle.
- Whole-reader quarantine, data-only preference import, reversible balanced
  profile, old-service identity checks and owned keep-awake cleanup.
- Old app/repository retirement without executing inherited uninstall hooks.
  KOReader's installed KPM record/wrapper is quarantined too, to avoid stale
  ownership replacing the official runtime. KMC/KPM stay intact.
- Consistent SQLite recovery snapshot and a durable mutation marker; selective
  restoration preserves unrelated later package records. Destination conflicts
  stop rollback for review. Old network services remain stopped after rollback.

The verified local USB backup remains at
`backups/20261029-005217/kindle-usb.tar`. Device-side root inspection previously
ran successfully; it is inspection, not proof of uncompromised firmware.

Run the preparation and staging commands in [README.md](README.md), safely eject
USB, then open GleWorks Install. If installation stops, use the already staged
GleWorks Rollback entrypoint and inspect the log; do not rerun over partial state.
The last quarantine path is recorded in `/mnt/us/gleworks-last-migration.txt`.

Current environment limits: normal network downloads fail DNS here, socket
creation is prohibited, and this session cannot write to the device mount.
The user's host-terminal mount is writable; no remount or filesystem repair is
needed. Official Tailscale compatibility, ARM module loading, HTTP over tailnet,
reader lifecycle, sleep/wake and reboot require actual device checks. Optional
DNS, tailnet-policy changes and revoking the old VPN node remain external steps.

The quarantine holds old credentials on USB. Protect it; revoke the old VPN node
and remove the quarantine only after acceptance. No system reset, root remount,
firmware operation, boot hook or DNS change is included.

## Earlier prototype notes (historical; superseded above)

# Migration status — 2026-10-29

User authorized safe app removal and replacement. **No removal has occurred.**

## Completed

- Re-detected the Paperwhite USB drive and re-read firmware 5.16.2.1.1.
- Verified the existing custom HTTP executable is unchanged from the audit.
- Backed up all USB-storage files locally, with per-file SHA-256 verification:
  1,526 files, zero mismatches. See BASELINE.md for the private backup location.
- Kept the existing services, package database, reader, and books untouched.
- Device-side reports returned successfully: root execution through the existing
  KMC helper is now demonstrated. The reports confirm ARMv7/Linux 3.0.35 and
  renamed OTA executables. Neither old application was running in those snapshots.

## Hard blockers

1. `/run/media/will/Kindle` is read-only **inside this session**. The user checked
   from the normal host terminal and confirmed the host mount is read-write.
   This identifies a session access restriction, not evidence of disk failure.
   No remount, filesystem repair, deletion, or installation was attempted.
2. Our current server is compiled with Go 1.26 and requires a supported Linux
   kernel of at least 3.2. The device reports 3.0.35. The source/build is not a
   validated replacement for this Kindle. Resolve compatibility before cutover.
3. USB storage does not provide a live root shell, but the user-run KMC scriptlet
   execution path is demonstrated. A reviewed device-side migration can use that
   path once a compatible replacement is staged. Startup contents and listener
   ownership still require verification; a process snapshot does not prove the
   underlying system clean.

## Device-side inspection prepared

`scripts/gleworks-inspect.sh` reads kernel/CPU information, process executable
paths without arguments, local listening TCP ports, startup file hashes, OTA
executable metadata, and selected SSH configuration choices. It never reads
private keys, tokens, VPN state, or account databases. It writes only a new
`/mnt/us/gleworks-inspection-*.txt` report. This necessarily uses the currently
installed KMC root helper if the launcher does not already run as root; that
helper has not been independently verified. The report cannot prove a compromised
OS clean. The script has been shell-syntax checked, not run on the Kindle.

From the normal host terminal, copy the script (the workspace path is absolute):

```sh
cp /home/will/dev/kindle/ours/scripts/gleworks-inspect.sh /run/media/will/Kindle/documents/
```

Safely eject using the desktop's normal eject action. Open **GleWorks System
Inspection** / `gleworks-inspect.sh` from the Kindle library. After it finishes,
reconnect USB so the report can be read. This script does not remove anything;
it gathers the missing evidence for a reviewed removal/migration.

## Cutover requirements

Establish a compatible and behaviorally verified server build, verified official
Tailscale binaries, and a device-side root execution path. Stage and verify the
replacement before stopping old services. Restrict inbound access and keep
credentials outside USB storage. Remove the old apps/launchers/repository through
a reviewed migration that also updates package records. Keep the private backup
until transfers, reader launch, VPN access, sleep, and restart behavior pass on
the device. Firmware reset is a separate operation; this app migration does not
prove the underlying firmware clean.

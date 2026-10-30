> Application implementation status: the Go prototype is superseded by the Lua appliance. See [APPLIANCE-PLAN.md](../BACKLOG.md#history-002) and [MIGRATION.md](MIGRATION.md). Baseline evidence and recovery limitations below remain applicable.

# Baseline investigation — 2026-10-29

## Evidence available

- User confirmed **Kindle Paperwhite (7th generation), PW3**, firmware
  **5.16.2.1.1**. Serial model prefix `G090KB` matches the white Wi-Fi PW3:
  [KindleTool's device definitions](https://github.com/NiLuJe/KindleTool/blob/master/KindleTool/kindle_tool.h)
  identify device code `0KB` as `KindlePaperWhite3WhiteWiFi`. No full serial is
  needed or recorded.
- Earlier USB inspection: firmware `5.16.2.1.1 (409747 002)` and a 3 GiB
  user-storage volume. These do not identify the exact Kindle model.
- User screenshot: Tailscale 1.104.4, Linux `3.0.35-lab126`.
- Earlier app inventory: ku-http 0.3.5 and ku-tailscale 0.3.3 from the previous
  third-party repository; KOReader 2026.7.2 and KPM 0.2.4 recorded against the
  community repository. Repository metadata alone does not verify binaries.
- USB reconnected successfully after model identification: `/dev/sda1`, VFAT,
  mounted at `/run/media/will/Kindle`. This session reports the mount as **ro**;
  no device write is possible here. The reason for the read-only mount is not
  established; `errors=remount-ro` alone does not prove a filesystem error.
- The user's subsequent normal-terminal `findmnt` output shows the same host
  mount as **rw**. Read-only access is confined to this session; no host remount
  or filesystem repair is indicated by this evidence.
- Archived boot logs report ARMv7, a Freescale i.MX 6SoloLite Wario board, and
  Linux `3.0.35-lab126`. These are historical logs, not a live root-shell probe.
  The root filesystem, current processes, system modifications, installed
  recovery components, and upstream binary integrity remain unverified.
- A full USB-storage backup was created at
  `../backups/20261029-005217/kindle-usb.tar`. All **1,526 archived files** were
  individually SHA-256 compared with the mounted originals: **zero mismatches**.
  File contents total 541,376,611 bytes; archive size is 544,604,160 bytes.
  The private backup directory is mode 0700 and backup files are mode 0600.
  Checksums and a verification summary are alongside the archive. This is not
  a backup of the system partitions and cannot restore the full firmware.

## Findings from primary sources

The [KindleModding removal guide](https://kindlemodding.org/jailbreaking/unjailbreaking.html),
retrieved on the date above, prescribes restoring OTA functionality, then a
factory reset, then installation of a model-matched firmware update at the same
or a higher version. Its OTA restoration instructions differ for modern and
legacy jailbreaks. A reset alone is not the documented full removal process.
This is a community procedure, not forensic proof that every persistent byte
on a device is trustworthy. Do not execute it until the model, jailbreak type,
OTA modifications, and recovery route have been identified.

[Amazon's update listing](https://digprjsurvey.amazon.com/csad/help/node/GKMQC26VQQMM8XSW?theme=light)
currently lists **5.16.2.1.1 for Paperwhite (7th generation)** and provides an
update download link. This establishes a same-version stock restore candidate;
the firmware bytes, signature, and installation behavior have not been verified.
There is no reason here to choose firmware intended for a newer Kindle model.

The [upstream Winterbreak2 repository](https://github.com/KindleModding/Winterbreak2)
states its firmware range is below 5.16.4, which includes this device's version.
That establishes a candidate re-jailbreak route, not successful reproduction or
audit of its downloaded payload. Verify the exact release and payload before
executing it. The current device's OTA modifications still need inspection.
Do not use third-party firmware images merely because they have a checksum.

[Go's release notes](https://go.dev/doc/go1.24#ports) establish a minimum Linux
kernel of 3.2 starting with Go 1.24. The current Go build here is therefore not
a supported deployment on the reported 3.0.35 kernel. ARM compilation only
establishes that source compiles for that architecture. A stock firmware rebuild
does not imply a kernel upgrade. Do not flash a custom kernel to fit this app.
Resolve this with actual device/runtime evidence or a reviewed implementation
using a maintained toolchain that supports the device; don't silently downgrade
to an unsupported old Go release.

## Required before reset or replacement

1. Model/firmware identified by the user and cross-checked against primary
   sources. Reconnect USB for device inspection; keep the full serial and
   account credentials private.
2. Back up books/settings and record the current install, including root-level
   startup hooks, keys, listening services, OTA changes, and jailbreak hotfix.
   Keep any suspected binary quarantined rather than executing it on the host.
3. Verify the model-specific official image, its update constraints, recovery
   instructions, and an independently sourced jailbreak route for that image.
   If those cannot be established, record that a verified rebuild is blocked.
4. Before any destructive action, present the exact firmware, procedure, backup
   location, rollback limits, and book/account impact for explicit approval.
5. After restoration, inspect the fresh baseline before introducing apps;
   use fresh VPN enrollment and HTTP credentials, and verify all inbound ports.
6. Only then deploy the reviewed replacement and validate upload/download,
   rejected unauthorized/path requests, reading, restart behavior, and sleep.

No reset, OTA restoration, firmware download, jailbreak, device write, existing
package removal, DNS mutation, or public deployment has been performed.

## Authorized replacement status

Two device-side inspection runs completed successfully on 2026-10-29, via the
existing KMC helper. Reports are preserved with the private backup. They confirm
ARMv7, 514,724 KiB RAM, and Linux 3.0.35. Neither captured tailscaled, ku-httpd,
an SSH daemon executable, or a TCP listener on 22/8080. Upstart's `sshd`
start/running label alone does not establish an SSH listener. Root-owned
loopback listeners include port 55555; socket ownership and purpose were not
collected, so they remain unattributed. No backdoor attribution follows from
these observations. OTA executables exist as `/usr/bin/otaupd.bck` and
`/usr/bin/otav3.bck`; the root ext3 filesystem is mounted read-only during the
inspection. This resolves live kernel/CPU inspection, but not stock integrity,
full startup-content review, or the replacement server's compatibility.

The user explicitly authorized safe removal of the previous third-party apps
and replacement with ours. Backup preparation is complete. Execution remains
blocked by the read-only mount and the incompatible/unvalidated server runtime.
Do not remove the working VPN or file server merely to make deletion progress.
The current HTTP configuration still has remote control and notifications off;
the installed executable hash matches the earlier inspection. A stored config
does not prove which processes are currently running.

The removal scope is ku-http, ku-tailscale, their document launchers and setup
launcher, and the previous third-party repository registration. Preserve KPM,
KOReader, books, and firmware. Clean service shutdown and package database
updates require a device-side execution path; moving folders from USB alone
would not establish that running services or system hooks have been removed.

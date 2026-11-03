---
status: current
updated: 2026-11-03
verified_build: 4b48e1ced2a9e7586e10de12599f2f293e2a6992fe85d379214a3bd9769b2b3f
supersedes: none
audience: maintainer
---

# Full system audit remediation (2026-11-03)

Status: **fixes complete on the host; device acceptance pending.** This pass
closed the fatal and critical findings from the read-only audit of every
component that runs on the PW3. The device still runs the previous build until
the new binaries are deployed over USB.

## Why this pass exists

The device will travel as an on-demand e-ink exit node. A boot, suspend or
Tailscale failure away from the home network is not a cosmetic defect, so the
bar for this pass was: no known fatal or critical defect may remain.

## Fixed

| Severity | Area | Fix |
| --- | --- | --- |
| Fatal | Bootstrap | `/sbin/init` is a BusyBox symlink; the old `O_NOFOLLOW` check rejected every built image and fell into USB rescue. Added `checked_init_path_at()`: resolve the link, require a regular executable inside the mounted root. |
| Fatal guard | Build | `pw3-build.py` now runs `bootstrap --check-root <tree>` on every image, so a layout regression fails the build instead of the boot. |
| Critical | Installer | `list_line()` counted invalid members before validating, then freed/OOB-read uninitialised slots. Entries are now appended only after every check passes. |
| Critical | Shutdown | `EINVAL`/`ENOENT` and a busy mount no longer pause PID1 forever. `unmount_bounded()` retries `EBUSY` and falls back to `MNT_DETACH`; the journal is best-effort. |
| Critical | Early init | The guardian recovery trap is installed before mounts, node creation or the shutdown copy. |
| Critical | Early init | A busy `g_file_storage` is retried, then the boot continues without the USB console instead of entering a reset loop. |
| Critical | Rescue | Any rescue with the guardian armed now cancels the reset deadline before exporting p4 read-write. |
| Critical | Reader | `gleworks-managed-reader.sh` now starts the single `gwctl watch` lock watcher, so a physical power lock exits KOReader to the OS lock screen. |
| Critical | Tailscale | `gw-tailscale` validates the PID against `/proc/<pid>/comm`, serialises calls, adopts an existing daemon, waits for the SOCKS listener and bounds stop with `SIGKILL`. |
| Critical | UI | Raw Y8 frames and lock images are pre-encoded through the inverse RGB332 table; a mid-gray no longer renders as dark red. |
| Critical | UI | The Hub no longer blocks the render loop on `fork/exec/waitpid`; the child is reaped in the main loop. |
| Critical | UI | App rows read identity from the row object; the remove sheet owns a copy of the id, removing the NULL-callback and stale-pointer paths. |
| Critical | UI | `APP LIST` reports "too large" instead of silently dropping rows. |
| High | Power | A failed suspend re-arms the 5 s retry instead of leaving the device awake; `errno` is captured before the RTC clear. |
| High | System | `NET ENABLE` no longer starts a second Wi-Fi instance while one is running. |
| High | Agent | UI receive keeps an unterminated tail across `recv()` calls; `recv()` retries `EINTR`. |
| High | Apps | USB package names are restricted to the protocol alphabet (no newline/space/control bytes). |
| High | Apps | Free space is rechecked against the declared decompressed size before extraction. |

## Verified on the host

```text
make check CC=cc                      PASS
system host tests                     PASS
root layout tests                     5 checks, 0 failed
Y8/RGB332 round-trip                  0 failures
ARM cross-build                       PASS
bootstrap --check-root (ARM + qemu)   good rc=0, escaping-symlink rc=1
```

Build hashes:

| Binary | SHA-256 |
| --- | --- |
| `gw-shell` | `7a7baedeb719209ca1cb288157645b8de8609b62eeca33734889c985673a2fee` |
| `gw-system` | `4b48e1ced2a9e7586e10de12599f2f293e2a6992fe85d379214a3bd9769b2b3f` |
| `gw-agent` | `8bc02bc595c74f0ac121b8e63ebe44cd33015e390923b6c95a5e9517f031e970` |
| `gw-appctl` | `059a9d1c33cdbd049644c8b1ada73e6791e8c8caef68c7fd5c303823df58746f` |

## Pending device acceptance

- Deploy `gw-shell`, `gw-system`, `gw-agent`, `gw-appctl`, `gw-tailscale`,
  `gleworks-early.sh` and `gleworks-managed-reader.sh` with backups.
- Boot once and record the new boot ID; confirm Alpine still boots (the fatal
  fix only changes the *next built image*, but the deployed early script and
  binaries need a boot).
- Hub screen: `gw-tailscale status` gains a PID on open and is empty on exit.
- Suspend: lock, unplug, confirm `suspend scheduled in 5000 ms` and `slept=`.
- Lock screen: confirm the KOReader export renders at the correct brightness.
- Exit node: decide whether advertisement is on-demand only or enabled while
  Wi-Fi is up; the current script advertises only when
  `/etc/gleworks/tailscale-exit-node` exists.

## Rollback

Restore the `.before-audit-20261103` copies of the binaries and scripts.

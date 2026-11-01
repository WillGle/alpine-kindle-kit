# OpenRC ordering audit — PW3 Alpine OS

Date: 2026-10-30  
Status: audit complete (host); hardware probe pending before activation.

## Additional host finding

The retained reader candidate does not contain `/sbin/openrc`, an OpenRC
service directory, or an installed OpenRC package. The proposed inittab is
therefore not deployable as written. Supplying and verifying OpenRC and its
dependencies in a separate candidate is required first. Because BusyBox runs
the proposed sysinit commands before starting the USB console respawn, a hung
or failed runlevel can delay that console; the existing independent p1 rescue
must remain available. The `rc_shutdown` ordering suggestion below is not an
implemented or verified shutdown path and must not be treated as one. No
OpenRC activation or shutdown ordering acceptance has occurred.

An isolated copy of the reader root was subsequently populated with Alpine
3.24 signed packages `openrc` 0.63.2-r0 and its resolved dependencies, with
package scripts disabled. Its OpenRC binary reports 0.63.2 under host QEMU,
but all runlevel directories are empty. In particular, the statement below
that `localmount` runs in `boot` is not true of this copied candidate unless
it is explicitly added; `after localmount` alone does not add that service.
No runlevel, mount, startup or shutdown was exercised by this package probe.

## Objective

Determine whether OpenRC can be introduced as the service manager after the
existing BusyBox init sysinit sequence completes, without conflicting with:
- the proven BusyBox `::restart:` shutdown helper
- the `gleworks-early.sh` sysinit sequence (mounts, USB gadget, USB console)
- the `gleworks-reader-service.sh` respawn entry
- the guardian/rescue deadline cancel sequence

## Current init sequence (BusyBox inittab)

```
::sysinit:/bin/busybox sh /etc/gleworks-early.sh
ttyGS0::respawn:/bin/busybox sh /etc/gleworks-console.sh
::restart:/run/gleworks-shutdown          ← proven shutdown helper
::respawn:/bin/busybox sh /etc/gleworks-reader-service.sh  (from koreader/inittab.fragment)
```

`gleworks-early.sh` performs, in order:
1. Mount proc, sysfs, tmpfs /dev, devpts, /dev/shm, /run, /tmp
2. Copy and chmod the proven shutdown binary to /run/gleworks-shutdown
3. Check/arm guardian (gleworks_pid1_reset module)
4. Load USB OTG / UDC / g_serial modules and create /dev/ttyGS0
5. Write /run/persistent-boot.log and /run/usb-ready
6. If persistent boot: cancel guardian deadline, remount root rw,
   clear shutdown/repair markers, echo persistent > /run/persistent-boot

The USB console and reader respawn entries start only after sysinit completes.

## OpenRC integration point

BusyBox init does not natively call `openrc` or any runlevel manager.  
The standard pattern for Alpine with BusyBox init is:

```
::sysinit:/sbin/openrc sysinit
::sysinit:/sbin/openrc boot
::sysinit:/sbin/openrc default
```

replacing or augmenting the existing `::sysinit:` entry.

### Conflict analysis

| Item | Risk | Mitigation |
|------|------|-----------|
| Early mounts (proc/sysfs/dev) | OpenRC also mounts these | Keep `gleworks-early.sh` as the first sysinit; OpenRC's `sysinit` runlevel starts after |
| Guardian deadline cancel | Must happen before any write in persistent boot | `gleworks-early.sh` already cancels before `remount rw`; OpenRC starts after |
| `::restart:` proven helper | OpenRC `rc-shutdown` would normally handle restart | Retain `::restart:/run/gleworks-shutdown` in inittab; do NOT let OpenRC override it |
| `gleworks-reader-service.sh` respawn | Currently a `::respawn:` entry | Replace with an OpenRC service (`gw-shell` / `gw-system`) in the `default` runlevel |
| USB gadget modules | Loaded in `gleworks-early.sh` | No conflict; modules loaded before OpenRC runlevels |

### Proposed inittab after OpenRC introduction

```
# Early hardware init (USB, guardian, mounts) — unchanged
::sysinit:/bin/busybox sh /etc/gleworks-early.sh
# OpenRC runlevels — start after gleworks-early.sh
::sysinit:/sbin/openrc sysinit
::sysinit:/sbin/openrc boot
::sysinit:/sbin/openrc default
# USB console — unchanged
ttyGS0::respawn:/bin/busybox sh /etc/gleworks-console.sh
# Proven shutdown helper — unchanged; OpenRC must NOT override this
::restart:/run/gleworks-shutdown
```

The reader respawn entry (`gleworks-reader-service.sh`) is removed from inittab;
the system service and shell become OpenRC services in the `default` runlevel.

### Service ordering (`gw-system.openrc`)

```
depend() {
    after   localmount
    before  gw-shell
    use     logger
}
```

`localmount` runs in the `boot` runlevel and waits for fstab mounts.  
Since `/etc/fstab` is empty (mounts are owned by `gleworks-early.sh`),
`localmount` completes immediately, which is correct.

### Shutdown ordering risk

The main risk is OpenRC's `rc-shutdown` sequence racing with or replacing
`/run/gleworks-shutdown`. Mitigation:
- Retain `::restart:/run/gleworks-shutdown` in inittab as the final authority.
- Add `rc_shutdown` → `rc-svcdir` ordering to ensure services stop before
  the BusyBox init `::restart:` action fires.
- Do not add `shutdown` or `reboot` commands to OpenRC services; let only the
  system service (via gw-ipc REBOOT) trigger the proven helper.

## Required hardware probe (before activation)

On the real PW3 device, with the proposed inittab change applied:

1. Boot into persistent mode; confirm guardian deadline is cancelled before
   OpenRC services start.
2. Confirm `gw-system` starts and the IPC socket appears in /run/.
3. Issue REBOOT via IPC; confirm `/run/gleworks-shutdown` runs (not openrc shutdown).
4. Confirm USB rescue (RESCUE command or boot.enabled removal) still works.
5. Confirm clean reboot with same-mechanism boot ID change.

Do not activate OpenRC service ordering in the production image until all
five probes pass and results are recorded in `docs/evidence/ACCEPTANCE.json`.

## Decision record

- OpenRC **will not** own `/run/gleworks-shutdown` or the restart action.
- OpenRC **will** manage `gw-system` and `gw-shell` in the `default` runlevel.
- `gleworks-early.sh` **remains** as the first `::sysinit:` entry, unchanged.
- `gleworks-console.sh` **remains** as a BusyBox `ttyGS0::respawn:` entry.
- The reader service respawn **moves** to OpenRC (`gw-shell` service).
- Hardware probe required before deployment; this document records host audit only.

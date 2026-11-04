---
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# Alpine core, optional apps and ARM QEMU

Scope: separate the userspace core from personal apps, compose a clean Alpine
VM root and validate app lifecycle/recovery on the host. **No Kindle was
accessed or written during this work.** PW3 p1 recovery remains a separate UART
step. The procedure is [reproducible here](../../how-to/alpine-core-qemu.md).
Machine-readable results: [results.json](results.json).

## Inventory and separation

Read-only source image SHA256:
`1937813d3fb09be4772552141cae2898521e08cc6d33ab592f863a49f8f4840c`.
The source contains 135874303 bytes (129.58 MiB) of regular files, excluding
symlink targets and omitted special nodes. Categories do not imply dependency
closure or equivalence of source/device/VM functionality.

| Source category | Actual regular-file MiB | Treatment |
| --- | ---: | --- |
| KOReader and compatibility libraries | 69.776 | Separate app code/resources; no app dependency in the core |
| Tailscale and named helpers/config | 34.861 | Separate optional app; no VPN state in the core |
| Base executables/config/services | 6.560 | Rebuild the core subset from source and pinned Alpine |
| Shared runtime libraries | 14.065 | Keep only dependencies supplied by the clean Alpine baseline and core |
| Hardware/network profile | 2.587 | Separate from the VM root; PW3 reintegration pending |
| Development/diagnostic files | 0.782 | Excluded except the small musl-utils/scanelf maintenance closure in the clean baseline |
| Other resources, retained backups and private state/config | 0.948 | Review or exclude; not copied wholesale |

The installed apk registry overstates present Python/GDB/debug files because
prior trimming removed their executables/bulk payloads without removing every
registry entry. For example Python lists 20898561 bytes but only 66622 registered
regular-file bytes remain; GDB lists 8064476 bytes but only 699310 bytes remain.
The inventory records missing registered paths separately. These are not
additional measured megabytes available to remove.

The new core contains BusyBox init, musl, system supervisor/client/appctl, a
fresh static graphical shell, signature crypto/public key and clean Alpine
maintenance utilities. Core builds disable built-in-reader special launch and
removal protection; the reader app ID behaves like other optional apps. Legacy
build defaults are preserved. Python/GDB/Tailscale/KOReader, settings/history,
VPN identities and development backups are absent from the new image.

The core remains an Alpine **VM profile**, not a hardware-ready PW3 root.
Its explicit console-recovery flag bypasses supervisor/UI. PW3 USB rescue,
module loading, guardian pairing, display/input/energy management and Wi-Fi
remain owned by the device profile, not replaced by VM code.

## Images and optional packages

The final core's exact hash/bytes and separate package hashes/sizes are in
[results.json](results.json). The ext3 file capacity is 64 MiB; logical core
payload is **8707628 bytes (8.30 MiB)**. Source and VM capacities are not an
apples-to-apples storage comparison: the VM profile excludes the PW3 stack and
personal apps, but includes a full shell. No image was deployed.

Separate development app sources contain about 68.56 MiB reader/compatibility
payload and 34.82 MiB Tailscale payload before gzip. They use the existing
`.gwapp` + RSA/SHA-256 format and explicit VM test key. Personal reader settings,
custom defaults, history, caches, screenshots, clipboard and news are excluded;
Tailscale state and auth material are not copied. Production app source/signing
and distribution review remain separate work.

Independent review found two more central reader-metadata directories,
`docsettings` and `hashdocsettings`, which the exporter now excludes. Neither
exists in the captured source, so this correction leaves the measured app
payloads unchanged.

The real reader package exposed a 512-member legacy installer ceiling. The
core-only build now sets a bounded 4096-member ceiling, retaining duplicate,
path/type, signature, total-name-byte and expanded-size checks. The legacy
ceiling stays unchanged. This was found with a real package, not inferred from
a tiny app fixture.

## QEMU acceptance and evidence boundaries

ARM QEMU `virt`, cortex-a15, 512 MiB, no networking, Debian ARM kernel
`6.12.107+deb13-armmp`, matching virtio/ext4/evdev/uinput modules. The core's
BusyBox PID1 and actual service tools run after an initramfs mount/switch_root.
The host uses a disposable image clone; core hashes before/after are equal.
Real app checks expand that clone to 256 MiB only.

Passed checks cover:

- Empty core app list and absence of personal apps from core.
- Signed app installation, generic launch and externally requested stop.
- Update with old version retained, appdata preserved and nonzero-exit handling.
- SIGSEGV-terminated app returns control to the shell; the service survives.
- A tampered package fails verification while the active version is unchanged.
- Manual active-version rollback, rerun, uninstall and retained appdata.
- `koreader` installed/launched/removed through the generic app path.
- Freshly compiled real shell enters its event/render loop after the fixture.
- Actual KOReader and Tailscale packages install, execute `--version` and remove.
- Explicit recovery mode runs a shell command with no system service/UI active.

App fixture requests originate from the supervised shell PID, preserving the
service's IPC credential checks. A real synthetic power button and touch
capability are supplied using uinput, rather than disabling the checks. The
real shell runs with display/touch acceptance off and a VM framebuffer stub.
This proves shell runtime, not pixel quality or real gestures.

The app SIGSEGV result proves foreground process recovery, not memory/OOM
isolation or a security sandbox. Apps still run as root under the current
contract. The reader smoke executes LuaJIT's version path; it does not open a
book. Tailscale's smoke does not authenticate or carry VPN traffic, and its
foreground entrypoint is not accepted as a concurrent background app service.
App rollback was manual; OS A/B, automatic whole-system rollback and data-schema
rollback were not implemented or proven by this check.

Earlier VM failures are retained privately: missing virtio/ext4 adapter modules,
missing power input, a fixture's incorrect handoff exit code, missing touch
capability for the real UI and the real reader's member-count limit. Final
acceptance uses a clean rerun with the final artifacts, not those failed logs.

## Measurements and next decision

The machine-readable report keeps the raw meminfo and CPU tick delta from a
10-second interval after removing optional apps, with the real shell alive.
RAM is `MemTotal - MemFree - Buffers - Cached`; CPU busy excludes idle and
I/O wait. Filesystem df reports the disposable guest, including VM probes and
filesystem effects; it is not the core payload measurement.

| Final ARM VM measurement | Value |
| --- | ---: |
| Core regular-file payload | 8.304 MiB |
| Core ext3 capacity | 64 MiB |
| RAM excluding buffers/cache | 31.797 MiB |
| MemAvailable | 448.055 MiB |
| CPU busy, 10-second sample | 2.068% |
| Disposable expanded guest df used after app removal | 8.558 MiB |
| Lifecycle/package/shell checks | 40 passed |
| Explicit recovery-shell check | 1 passed |

These are VM baseline measurements, not PW3 benchmarks. Kernel version,
synthetic devices, caches/slab left by app install/removal, lack of Wi-Fi,
EPDC and power work all affect the result. Do not compare the measured RAM/CPU
directly with the earlier physical PW3 43.9 MiB/3.8% figures or claim battery,
smoothness or speed gains from this run.

**Buildroot decision: defer.** The dominant core content is the existing shell,
crypto, BusyBox and musl, which remain needed in either implementation. Alpine
maintenance overhead is small relative to the removed optional apps. First
consider removing optional maintenance utilities if a product budget requires
it. Buildroot would need the same app/signature/recovery/UI functionality for
an honest comparison. No equivalent Buildroot image has been built or timed.

**Hardware gate:** recover/inspect p1 using UART; verify the actual bootstrap,
mount targets, matched guardian and pivot with the intended root. Qualify
independent USB rescue and fallback, then reintegrate/test EPDC, touch, power,
Wi-Fi, suspend/wake and app handoff. Keep the Amazon kernel/U-Boot and follow
the [safe reinstall procedure](../../how-to/safe-reinstall.md). The VM root is
rejected by that PW3 installer and must not be substituted for a native image.

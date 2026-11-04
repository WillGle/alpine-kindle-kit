# Kindle workspace — device installation rules

Read `docs/status/CURRENT.md` and `docs/how-to/safe-reinstall.md` before any
installation, reinstallation, recovery or boot change. These are maintained
instructions; archived/private trial scripts are evidence, never installation
entry points.

- Current reference PW3 is blocked before Alpine init. USB rescue exposes p4,
  not p1. Replacing p4 again does not establish that p1 is fixed.
- Default to host/QEMU preparation. Do not access/write a device unless the user
  authorizes that session's device action. Preparing a kit is not deploying it.
- Reinstall a healthy native system only through
  `tools/install/pw3-safe-reinstall.py` and its documented identity/recovery gates.
  Keep p1, kernel, bootloader, IDME, diags and partition layout unchanged.
- First installation from rooted Amazon is a separate, hardware-unqualified
  coordinator: `tools/install/pw3-install.py`. Never run it on native/rescue.
- Never replace `/sbin/init` with a standalone `cp`, `dd`, ad hoc script or old
  opt-a launcher. Required p1 directories include real `/control`, `/newroot`,
  `/modules`; modules/bootstrap/root guardian/shutdown must be a paired build.
- `bootstrap --check`, a clean ext3, a visible rescue disk or passing QEMU checks
  do not prove p1 mount/pivot compatibility or a working stock fallback.
- Do not edit session state, hashes, evidence or boot markers to bypass a gate;
  do not add `--force`/ignore-errors or retry an interrupted write automatically.
- Preserve the old image, failure markers and logs before repair. Never export
  FAT while Alpine is using the root image inside it. Never guess `/dev/sdX`.
- A p4 image cannot repair p1. Do not promise USB-only Amazon restoration or
  automatic fallback for p1/kernel/bootloader failures. UART restore requires
  model/partition verification and its own reviewed procedure.
- KOReader and Tailscale are optional apps. The new QEMU core is not a qualified
  PW3 installation image. Retained historical images may still contain apps.
- Select established community apps with evidence of broad real use and
  practical usefulness (for example KOReader). Agents port, package and
  integrate those apps; do not invent replacement product apps. Record upstream
  source/version, license and usage evidence before selecting a dependency.
- New/rebuilt app packages must follow `docs/how-to/agent-apps.md`, include
  `compatibility.json`, and pass `gwapp.py check` plus `build --rootfs` against
  the intended target. Never remove the declaration to bypass a failure or
  replace core libraries to satisfy an app. Static acceptance is not QEMU/PW3
  acceptance; keep source and target inputs stable during checking/signing.
- Preserve unrelated dirty work. Make minimal changes, add no tests unless
  requested, run existing appropriate checks, and distinguish host proof from
  physical boot, panel, touch, suspend and power acceptance. Do not commit/push
  or publish private kits, backup images, serials or keys without instruction.

---
diataxis: how-to
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: user
---

# Install and update

**2026-11-05 safety update:** read [safe reinstall and fallback](safe-reinstall.md) first. The reference device currently fails before Alpine init; p4 rescue cannot repair p1. Do not repeat first-install or replace init to work around that failure.


This page is the short operational path. Build instructions are in
[`build-and-install.md`](build-and-install.md); recovery is in
[`recover-usb.md`](recover-usb.md).

## Update an installed native system

1. Build and verify a clean root image on the host. Keep the image and manifest
   together.
2. Inspect the device and confirm the boot ID:

   ```sh
   python3 tools/maintenance/kindle-linux.py inspect --output /tmp/pw3-inspect.log
   ```

3. Deploy with an explicit SHA256 and a new backup name:

   ```sh
   python3 tools/maintenance/kindle-linux.py deploy-root \
     <clean-system.ext3> --sha256 <manifest-hash> \
     --backup-name system-before-<change>.ext3 \
     --evidence /tmp/pw3-deployment.json
   ```

4. Reboot through the supported path, then confirm `gwctl status`,
   `gw-system --supervise`, the shell, touch and reader handoff.

## Update a signed application

Place the `.gwapp` and `.gwapp.sig` pair in `/mnt/us/gleworks/apps/`, then use
**Settings → App manager → Scan USB**, select the package, and choose
**Install selected**; or run `gw-appctl install <file>` from a root console.
Unsigned, modified, wrong-architecture or path-traversing bundles are rejected
before extraction.

## Guardrails

- Do not export FAT while Alpine is using its root image.
- Do not use `reboot -f` as a normal update step.
- Do not reuse an image or component from a different candidate build.
- Keep the previous image until the new boot and reader path are confirmed.

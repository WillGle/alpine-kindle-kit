---
diataxis: tutorial
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: user
---

# Tutorial 3 — first boot

This is the guided first-success path after a supervised install. The installer
must already have finished promotion and returned a clean native boot. If the
device is still on stock Amazon firmware, use
[`02-first-install-community.md`](02-first-install-community.md) instead.

## Steps

1. Keep the USB cable attached and confirm the serial console is the native
   gadget `0525:a4a7`.

   ```sh
   python3 tools/maintenance/kindle-linux.py inspect --output /tmp/pw3-first-boot.log
   ```

2. Confirm the boot identity and process tree:

   ```sh
   python3 tools/lib/usb_console.py <<'SH'
   cat /proc/sys/kernel/random/boot_id
   readlink /proc/1/exe
   gwctl status
   free -m
   df -h
   SH
   ```

3. Wait for Home to appear. Open KOReader, open a book, turn a page, and return
   to Home with **Return to Home**.

4. Check the power path: a short power-button press must show the lock screen
   and suspend when no cable or charger is attached. Press again, then Unlock.

5. Copy one small book and confirm its size and SHA256 on both host and device.

6. Record the boot ID, deployed binary hashes and any screenshots in
   [`../evidence/`](../evidence/README.md), then update the facts in
   [`../status/STATUS.json`](../status/STATUS.json).

## If something fails

- No console: check the USB mode before assuming a boot failure. `a4a5`/`0200`
  is rescue; an empty `a4a6` or stock `1949:0004` is not writable rescue.
- Dirty root or a pending boot marker: follow
  [`../how-to/recover-usb.md`](../how-to/recover-usb.md).
- Reader or UI failure with a working console: keep the console, collect
  `/run/reader.log` and `/run/gw-system.log`, and do not force a reboot loop.

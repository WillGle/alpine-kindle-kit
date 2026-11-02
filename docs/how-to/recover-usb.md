---
diataxis: how-to
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: user
---

# Recover over USB

Use these steps when a boot marker is pending, the root image is dirty, or the
reader/UI does not start. The independent rescue gadget is the recovery path;
it does not depend on the installed root image.

## Identify the USB mode

| ID | Meaning |
| --- | --- |
| `0525:a4a7` | Alpine native serial console (not a disk) |
| `0525:a4a5`, bcdDevice `0200` | Independent USB rescue, writable |
| empty `a4a6` | Not a writable rescue handshake |
| `1949:0004` | Stock Amazon mode |

Do not guess a block device. Confirm the serial gadget and nonce handshake with
`tools/lib/usb_console.py` or `tools/maintenance/kindle-linux.py inspect`.

## Enter rescue and inspect

```sh
python3 tools/maintenance/kindle-linux.py rescue
```

In rescue the host may mount the FAT partition, back up `system.ext3`, and run
`e2fsck` offline on a copy. Preserve the image and repair log before changing
anything. A pending `boot.pending` or `boot.repairing` is not cleared just to
make a gate pass.

## Return to native

```sh
python3 tools/maintenance/kindle-linux.py resume
```

If the image is damaged, deploy a known-good image with an explicit hash and a
new backup name, then reboot. See [`install-update.md`](install-update.md) and
[`operate-reader.md`](operate-reader.md) for the full operating context.

## Hard limits

- Never mount/export FAT while Alpine is using its root image.
- Never edit p1/IDME or the bootloader as a UI repair shortcut.
- Amazon Factory Reset removes the Alpine data on p4; it is not a repair path.
- Rescue on p4 does not recover a damaged kernel, p1 or eMMC.

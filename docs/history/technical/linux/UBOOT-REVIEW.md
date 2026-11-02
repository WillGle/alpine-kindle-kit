---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# Stock-family U-Boot source review — 2026-10-29

Source: katadelos/uboot-2009.08-lab126, clean checkout at
4b6c52358630564562224b780bd75a5659fbc84b. This modified source tree is not yet
proven identical to this device's production/BIST binaries.

## Source findings

- board/imx60_wario/imx60_wario.c:1787 routes IDME bootmode fastboot through
  bootcmd_fastboot. Production config maps this to bist fastboot; BIST config
  enables CONFIG_GADGET_FASTBOOT. This supports investigating software entry.
- drivers/usb/gadget/fastboot.c:1054 does NOT boot the downloaded RAM payload.
  It executes getenv("bootcmd"). The preceding boot.img comment is misleading
  for this implementation. Do not assume Android-style `fastboot boot` semantics.
- :1065 `continue` exits to the bootloader prompt, not necessarily stock Linux.
- :1072 `reboot` resets without clearing the persistent bootmode variable.
- :708 `setvar` calls idme_update_var; it writes persistent device state.
  A candidate exit path depends on restoring bootmode first, but this has not
  been validated on hardware. Do not set fastboot mode merely to try discovery.
- :1197 runs a USB polling loop without an evident automatic timeout. Cable
  disconnect exits the loop, but does not itself restore main bootmode.
- :689 secure query returns a literal "no". That reply alone is not proof of
  this unit's ROM secure-boot/fuse state or arbitrary-image boot capability.

## Next evidence, collected without boot changes

The existing hardware report includes only version strings. Compare the actual
production and BIST images to this source before a mode change. Prepared
`gleworks-boot-capture.sh` for the existing scriptlet launcher. It reads boot0,
boot1 and the 32 MiB before p1 into ordinary files, plus the userspace idme binary
for static inspection. Expected raw data total is 40 MiB. It records file hashes
and a last-written COMPLETE marker. No raw writes, idme invocation or reboot.

These files can contain device identity and private provisioning data. Keep them
local/private, not in public Git or shared logs. This is NOT a complete backup:
root filesystems, panel SPI contents and an independently tested restore are
not included. Hash verification here checks captured files, not a second read
of the raw source or trustworthiness of the running kernel.

Validation: host sh -n passed. Device execution pending. The agent still sees
read-only USB storage; the user must copy and run the script through KMC.

No flashable image built, no bootmode changed. USB-only independent Linux boot
remains unresolved; Fastboot presence alone does not solve RAM chainloading.

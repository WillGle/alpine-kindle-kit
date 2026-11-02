---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# Vendor kernel reference check — 2026-10-29

Acquired tree: linux-vendor-reference, commit
7ac2585f384573da8cbdcc987a3503e2e6ff1fb4. git status is clean.
README identifies the origin as Amazon's 5.10.1.1 source release, explicitly
lists PW3 and imx60_wario_defconfig. This is older than the installed 5.16.2.1.1
firmware; it is a hardware reference, not proven byte-equivalent source for the
captured 2023 kernel/modules.

## Useful public implementation references

- arch/arm/mach-mx6/board-mx6sl_wario.c includes Muscat board variants and
  regulator, device and power wiring. Translate the correct variant to DT;
  do not import settings for other Wario boards blindly.
- drivers/mfd/max77696-*.c, include/linux/mfd/max77696.h and
  include/max77696_registers.h provide PMIC core/IRQ/register definitions.
  Subdrivers cover regulators, EPD supply, battery, charging, backlight, RTC,
  onkey and other devices. Core file header is GPLv2. Modern API adaptation
  and hardware validation are still required.
- drivers/input/touchscreen/cyttsp4_* and include/linux/cyttsp4_* provide
  the touch stack. Core header is GPLv2-only. Board mx6_cyttsp4.c sets I2C
  address 0x24, matching the live capture; board header maps interrupt to
  GPIO4_IO03 and reset to GPIO4_IO05. Reset/wake timing is available in board
  callbacks. These mappings remain source evidence, not new on-device tests.
- drivers/video/mxc/mxc_epdc_fb.c includes mxc_epdc_fb_lab126.c for vendor
  panel/waveform behavior. Useful for register, waveform and ioctl reference.

## Material gap

The display source references do_reagl_processing_v_2_lab126,
do_reagl_processing_v_4_lab126 and do_reagl_processing_v2_2_1. Whole-tree C/header
search finds their declarations and uses, but no implementation. This agrees
with the captured proprietary mxc_epdc_eink module boundary. Possessing this
GPL tree does not establish that the original complete display stack can be
rebuilt, or that its old binary module can run on Linux 6.18. Determine whether
a public EPDC implementation can provide required basic refresh modes with the
unit's waveform/calibration data; do not assume REAGL is required for every mode
or that removing it automatically yields a working driver.

## Decision

This source acquisition is useful and valid as a public reference. PMIC and
CYTTSP4 now have concrete local porting inputs. A modern device tree/driver port,
display implementation, compatible Alpine/KOReader build and USB recovery are
still incomplete. Do not copy this entire old kernel as the final platform.

Checks performed: commit and clean checkout; README provenance; board variant,
I2C/GPIO definitions; driver file/Kconfig/Makefile inventory; sampled license
headers; whole-tree search for the named display algorithm definitions. No
build, maintained tests, firmware operations or device writes performed.

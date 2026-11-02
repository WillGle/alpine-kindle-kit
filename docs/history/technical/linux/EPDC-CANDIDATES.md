---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# Public EPDC search — 2026-10-29

Status: promising source candidate found; no PW3 build or runtime validation.
No device writes. Ponder/Solarcene is not a dependency.

Update after reconnection: pinned local checkout and waveform capture now
verified; see WAVEFORM-CAPTURE-REVIEW.md. Acquisition failures below describe
the earlier attempt and are resolved by the user's successful download.

Deferred follow-up: [response optimization on existing PW3 hardware](RESPONSE-OPTIMIZATION.md)
records the Modos reference, controller questions and measurement criteria for
revisiting after Alpine bring-up. No optimization or speedup is established.

## Preferred candidate

Public repository: https://github.com/akemnade/linux
Branch inspected through raw GitHub: `kobo/drm-merged-6.15`.
Initial web API lookup failed. A subsequent host `git ls-remote` resolved
`8165a8cc247ab741a163c2d1b42d95e616c10d83`; fetch-epdc.sh pins that commit
and refuses a changed branch. The clone then failed DNS, so no local checkout
exists yet and the earlier web-viewed files have not been matched to this hash.

Under `drivers/gpu/drm/mxc-epdc/`:

- `mxc_epdc_drv.c` matches `fsl,imx6sl-epdc`, uses DRM, and requests
  `imx/epdc/epdc.fw`. This is a driver candidate, not a complete PW3 port.
- `epdc_waveform.c` selects P5N for the 5-bit header flag and sets basic
  INIT/DU/GC16 mode mappings. Those numeric mappings need panel validation.
- `epdc_update.c` implements grayscale conversion and update scheduling;
  its 5-bit handling includes a reduced LUT count. No `reagl` match was
  found in that file. This supports investigating basic refresh without
  Amazon's missing REAGL routines, not a whole-tree dependency guarantee.
- `Makefile` builds four public objects; sampled C headers are GPL-2.0+.
- `epdc_hw.c` uses regulator/clock APIs. PW3 MAX77696 supplies and sequencing
  still need adaptation; Kobo power settings are not interchangeable.

Primary source URLs (append filenames to this base):
https://raw.githubusercontent.com/akemnade/linux/kobo/drm-merged-6.15/drivers/gpu/drm/mxc-epdc/

The author's initial RFC reports operation on i.MX6SL Tolino devices, not PW3:
https://patchew.org/linux/20220206080016.796556-1-andreas%40kemnade.info/

## Waveform and remaining gaps

A waveform is panel-specific drive data, separate from the OS and the REAGL
processing algorithm. Reusing this unit's data does not require running Amazon
userspace. It also does not establish open licensing or redistribution rights
for that data.

The local vendor reference at commit
`7ac2585f384573da8cbdcc987a3503e2e6ff1fb4` provides a source-level extraction lead:
`drivers/video/mxc/mxc_epdc_fb_lab126.c` registers
`/proc/eink/panel/waveform/data`, metadata/checksum endpoints, and panel data.
`proc_panel_wfm_data_read` reads the waveform region from panel SPI flash.
These endpoints must be verified against the installed newer firmware before
capture. Read only; do not write their data/readonly control endpoints.

Preserve original bytes and verify lengths/checksums before considering format
adaptation. Retain panel timing, VCOM, temperature ranges and mode mapping.
Do not substitute Kobo waveforms or assume a filename rename converts formats.

https://github.com/fread-ink/inkwave explicitly lists 5-bit waveforms as
unsupported. It is not presently a verified conversion route for this unit.
FBInk and KOReader ioctl headers are userspace/API references, not replacement
EPDC kernel drivers.

Remaining gates: pin/download candidate, compare waveform formats, adapt PW3
DT/PMIC/touch, review error handling, build against the selected kernel, adapt
KOReader display access, and demonstrate ordinary-USB recovery before flashing.
DRM availability does not establish Kindle framebuffer ioctl compatibility.
No account/subscription is needed to read the public candidate sources.

## Prepared capture

`gleworks-waveform-capture.sh` reads the active waveform twice and, if exposed,
the panel waveform twice, compares copies and hashes the resulting files.
It also records available metadata. COMPLETE means matching reads and local
hash verification only, not validated waveform structure or panel recovery.
The vendor reference marks the active computed-checksum handler as incorrect;
retain both checksum texts as evidence, not an automatic acceptance test.
The decoded panel/data output is not a complete raw SPI backup.

Copy the script to Kindle documents, safely eject, run it from the existing
launcher, then reconnect USB for host review. Captures can include panel
identity and must remain private. FAT user storage does not enforce Unix mode
bits despite umask; protect the host copy when importing it.

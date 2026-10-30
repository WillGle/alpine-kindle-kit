# Waveform capture review — 2026-10-29

Capture: gleworks-waveform-capture-20261029-190452-16018.
Imported to private-captures with directories 0700 and files 0600; every
host file compared byte-for-byte with USB source. COMPLETE marker present,
all 16 manifest entries verified, both waveform pairs identical.

| Input | Captured bytes | Header file_length | CRC over declared length |
| --- | ---: | ---: | --- |
| active-waveform.bin | 6613472 | 257864 | Does not match |
| panel-waveform.bin | 456570 | 257864 | Matches |

CRC independently calculated with the first four checksum bytes zeroed,
following the vendor reference's file_length path. Panel embedded/computed
checksum endpoint texts also agree; active endpoint texts do not. The vendor
reference already warns about the active computed-checksum handler. This is
not evidence of a failed USB copy: duplicate reads and SHA256 all passed.
The active representation and panel trailing bytes require further analysis;
neither file is approved for direct use as epdc.fw. Preserve originals intact.

Both headers indicate 5-bit format, mode version 25, eight modes and fourteen
temperature entries. The shared 48-byte header layout yields data offset 63
for the inspected loaders. Header compatibility alone does not prove payload
or mode-map compatibility. Panel metadata captured is decoded data, not a
complete raw SPI backup; no claim of complete calibration/recovery coverage.

EPDC checkout verified at 8165a8cc247ab741a163c2d1b42d95e616c10d83, clean
git status; source-snapshot.tar matches the fetch manifest. Local driver
inspection confirms P5N handling and basic mode mapping; no REAGL match in
drivers/gpu/drm/mxc-epdc. Earlier web line numbers differ from local source;
use this pinned checkout for further analysis.

No build, driver loading, boot change or device write performed during review.

## Structural analysis follow-up

Reproducible host analysis: `python3 ours/linux/analyze-waveform.py CAPTURE_DIR`.
Result retained in boot-prep/waveform-analysis.json, with no panel identity.

- The 198706 bytes after the panel's declared file length are all zero.
- The first 63 bytes of active and panel captures match exactly. They contain
  the header and fourteen temperature entries; subsequent representation differs.
- Active payload contains eight in-bounds 64-bit mode-table offsets; all 112
  temperature-specific waveform pointers are in bounds and eight-byte aligned.
  These are structural checks, not validation of waveform instructions.
- Advanced flags = 3. Vendor interpretation is voltage plus algorithm control.
  The voltage table covers 112 records of 16 bytes each; all 112 sum8 checksums
  pass independently. All records have identical first seven 16-bit fields in
  this capture; this table does not itself demonstrate changing voltages across
  modes/temperatures. The public DRM driver does not implement this vendor
  per-mode/per-temperature voltage-control path. Constant VCOM alone is not
  demonstrated equivalent. Do not energize EPDC before resolving this gap.
- Vendor mode version 25 maps INIT=0, DU=1, GC16_FAST=2, GL16_FAST=3,
  REAGL=4, REAGLD=5, A2=6, DU4=7. Public default mode_gc4=3 therefore names
  a different mode on this panel. INIT/DU/GC16 mappings agree at source level;
  restrict the initial display-port investigation to explicitly chosen modes
  after waveform and PMIC handling are validated. No refresh mode was tested.

The active representation is a promising already-expanded input for the public
loader; no converter or correctness claim is inferred just from these checks.
Original header CRC describes the panel representation, not the expanded active
buffer. Preserve both originals; do not silently repair its checksum or publish
panel-specific blobs.

The decoded panel VCOM field has the expected signed decimal shape; its value
and the common raw voltage-control record were retained in the private capture
as derived-calibration.json, with no fallback/default substitution. This is
source-based interpretation, not a voltage measurement or PMIC acceptance.

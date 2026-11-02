---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# PW3 response optimization — revisit after Alpine bring-up

Recorded: 2026-10-29. Status: deferred research, no implementation or measured
speedup. Keep the existing PW3 hardware and installed U-Boot; the active goal
remains replacing Amazon OS with independently booted Alpine Linux.

## Motivation

Reference: https://www.youtube.com/watch?v=nHbA2-_qzH4

The user's summary describes Modos Flow reaching 60 fps using an FPGA controller
with per-pixel update scheduling. The video/transcript was not independently
retrieved in this discussion. Treat it as a research lead: controller architecture
can materially limit responsiveness, so do not assume the panel is already the
bottleneck. Conversely, it does not establish that PW3's existing i.MX6SL EPDC
can reproduce that architecture, or that every grayscale transition settles
within one 60 Hz frame. Installing Alpine does not replace the physical EPDC.

## Questions and candidates to revisit

- Determine which limits belong to the EPDC hardware and which come from driver
  scheduling, waveform selection or application behavior. Inspect update slots,
  concurrent regions, overlap/collision handling and completion waits; do not
  assume independent per-pixel scheduling is available.
- Investigate reducing queue delays, merging pending updates and discarding
  obsolete pending states where supported. Preserve required transitions and
  completion semantics; do not blindly cancel active waveform sequences.
- Use partial regions and suitable supported modes for terminal typing, cursor
  and menu changes; evaluate a quality refresh after interaction settles.
  Pre-render/cache the next reader page within the 512 MiB memory budget.
- Check gesture waiting and CPU frequency ramp-up only if measurements identify
  them as bottlenecks. Balance responsiveness against reading/standby power.

These are candidates, not proven improvements. Existing stock KOReader settings
and user acceptance are documented in [READER-PERFORMANCE.md](../READER-PERFORMANCE.md).

## Preconditions and existing display gaps

Revisit hardware experiments after a recoverable kernel boot route and working
Alpine display/touch/power baseline exist. This note does not change the active
boot/recovery constraints or authorize device trials, flashing or hardware mods.

Use the captured unit-specific waveform/calibration and correct temperature and
mode handling. The public EPDC candidate still lacks the reviewed vendor
voltage-control path and REAGL integration; mode mappings also differ. Resolve
those compatibility gaps before energizing the panel with the new driver.
Do not start by changing panel voltages, shortening waveform sequences or
overclocking. See [waveform review](WAVEFORM-CAPTURE-REVIEW.md) and
[EPDC candidates](EPDC-CANDIDATES.md).

## Evaluation

Compare the stock KOReader baseline and Alpine under the same document, layout,
temperature, frontlight and refresh-quality conditions. Separate ordinary
partial updates from full refreshes. Record:

- Input to first visible change and input to settled text, using high-frame-rate
  video where practical; report median and tail latency with sample counts.
- Application render time, driver submission/queue time and controller completion
  separately where observable. A completion signal is not optical proof of
  settled pixels.
- Ghosting, contrast/readability, missed or duplicate input, and power behavior.
  Include page turns, terminal typing and menu interaction.

No 20 ms or 60 fps target is accepted for PW3. Establish the actual bottleneck
and measured improvement before selecting an optimization. Keep this deferred
work separate from the current kexec/Alpine boot work in [EXECUTION.md](../../BACKLOG.md#history-001).

---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# Paint quality evidence — 2026-10-31

- Device: PW3 `G090KB`, Alpine 3.24.3, kernel `3.0.35-lab126`.
- Boot ID during tests: `186d6571-e619-460e-b1f9-b3fcacb696a5`.
- Final deployed shell: `/usr/sbin/gw-shell`
  SHA-256 `0163000eb9e8e45e3b34434fd7ea2712e7caa7d067fcfe11ff3755f5262b9fcd`.
- Deployed policy: `/etc/gleworks/paint-policy` = `quality`.
- Perf/diag build used for measurement:
  `GW_UI_PERF=1 GW_UI_DIAG=1`; final build has perf off, diag on.

## Mode comparison (20 scene transitions = 10× Home ↔ Settings)

| Policy | EPDC submits | Waves | Submits/scene |
| --- | ---: | --- | ---: |
| `quality` | 20 | 20× GC16 full | 1.00 |
| `balanced` | 25 | 20× DU4 partial + 5× GC16 full | 1.25 |
| `fast` | 40 | 20× DU4 partial + 20× REAGL full | 2.00 |

`fast` is the old policy and reproduces the reported double paint: DU4 then a
full REAGL ~600 ms later. `quality` paints one GC16 per scene; `balanced` has
no per-scene cleanup and cleans every fourth scene.

Raw logs: `quality-10cycles-final.log`, `balanced-10cycles-final.log`,
`fast-10cycles-final.log`; summary in `summary-final.json`.

## Idle

5 minutes at Home with no input, `quality`:

- 5 EPDC submits in 300 s = **1/minute** (minute clock only).
- Waveform REAGL partial, area 12432 px (clock label).
- Perf build log growth is not representative; production perf logging is off.

## Typing and feedback

- One keyboard key: A2 submit at **68 ms** (`input_to_submit_us`), then text
  field update at ~254 ms, then small label cleanup.
- 20 key taps: 20 A2 keyboard updates + text field updates; **no full GC16
  during typing** (full cleanup is deferred until the keyboard is dismissed).
- Tapping outside the field closes the keyboard and produces one GC16 cleanup.
- Home tile tap (scene change): GC16 submit at **228 ms**.
- `quality` scene submit ioctl time ~1.33 ms; panel-visible time is not
  measurable from the framebuffer alone.

Logs: `typing-20keys.log` (from the earlier perf build; same policy),
`network-keyboard.png`, `network-typed.png`.

## Soak and stability

25× Home ↔ Settings (50 scenes), `quality`:

- 51 EPDC submits (50 scenes + one clock update).
- `gw-shell` RSS 656 kB → 680 kB; fd count 7 → 7.
- No process restart, no fd growth.

## Handoff and lock

- Home → Reader: service reports `foreground=koreader`; shell exits.
- Reader → Lock: reader exits, shell starts directly on the lock screen.
- Startup while already locked produces **one** GC16, not Home + lock.
- Lock/unlock 5 cycles: 10 GC16 submits, no loop; lock state and light follow
  the service.

## Framebuffer captures

`quality-home.png`, `balanced-home.png`, `fast-home.png`, `settings.png`,
`calendar.png`, `notes.png`, `apps.png`, `update.png`,
`network-settings.png`, `network-keyboard.png`, `network-typed.png`.

UI style has no gradients or shadows (`gw_ui_style.h`/`gw_ui_kit.c` force
`LV_GRAD_DIR_NONE` and `shadow_width=0`).

## Still requiring human/panel judgement

These are not proven by the framebuffer or logs:

- PQ-04/PQ-05 ghost score after 10/30 transitions on the physical panel.
- PQ-01/PQ-02 visible flash count as perceived by the user.
- PQ-07 panel-visible latency (only submit timestamps measured).
- PQ-10 reading 20 EPUB + 10 PDF pages after the policy change.
- PQ-12 reboot/rescue after this exact shell; rescue was not re-run.

Rescue/restore was rerun after the final build: independent USB rescue →
`resume` → Alpine boot `ae4fa08e-4d3c-468e-b339-387773498842`, same shell
`0163000e…` and policy `quality`.

Do not mark the visual items accepted until the user confirms on the device.


## User observation — 2026-10-31

Policy `quality`: 10 rapid Home ↔ Settings transitions reported **fast, clean,
no visible second flash**, ghost score **1/5**. This is the first human
acceptance of the quality path. `balanced` was not visually compared; its
measured trade-off is fewer full refreshes but DU4 residue.


## Balanced device check — 2026-10-31

Same 20 transitions (10× Home ↔ Settings), measured by the `max77696-epd`
GPIO interrupt count on the final shell:

| Policy | EPD updates / 20 scenes |
|---|---:|
| `quality` | 20 |
| `balanced` | 24 |

`balanced` therefore uses fewer full refreshes than `quality` (about 5 GC16
vs 20), but it still has DU4 residue. **User verdict:** balanced is worse —
the screen looked broken/torn and did not show enough information. `quality`
is the final default; balanced is rejected for release.
See `docs/evidence/pw3-ui-audit-2026-10-31/README.md` for the host-side
debounce/long-list evidence.

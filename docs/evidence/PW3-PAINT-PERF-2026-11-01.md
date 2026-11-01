# PW3 paint performance measurement — 2026-11-01

Measured with a temporary `GW_UI_PERF=1 GW_UI_DIAG=1` ARM shell
(`1f7c5c74…`, built with `os/ui/build-arm.sh`) and `/run/gw-ui-perf` enabled.
Driver commands came from `gwctl ui-nav/ui-swipe/ui-drag` over
`/run/gw-shell.diag`. The production shell was restored afterwards
(`3083b5de…`).

`submit_us` is the time spent in the `MXCFB_SEND_UPDATE` ioctl, not the full
panel waveform time. `input_to_submit_us` is measured from the injected input
to that submit and therefore includes the diagnostic sleeps.

| Scenario | Area (px) | Area % | Waveform | `input_to_submit_us` | `submit_us` |
| --- | --- | --- | --- | --- | --- |
| Home → Settings scene | 1,552,256 | 100% | 3 | n/a | 1,627 |
| Settings swipe release | 1,153,620 | 74.3% | 3 | 77,362 (77 ms) | 1,592 |
| Settings area cleanup | 1,153,620 | 74.3% | 2 | 881,236 (881 ms) | 1,529 |
| Home → Network scene | 1,552,256 | 100% | 3 | n/a | 1,589 |
| Network drag preview | 420,036 | 27.1% | 3 | 19,355 (19.4 ms) | 1,587 |
| Network drag preview | 420,036 | 27.1% | 3 | 19,525 (19.5 ms) | 148 |
| Network swipe/scroll release | 420,036 | 27.1% | 8 | 20,009 (20.0 ms) | 126 |
| Network area cleanup | 420,036 | 27.1% | 2 | 20,841 (20.8 s since input) | 166 |
| Idle render handler | — | — | — | — | 9–18 µs |

Observations:

- Text Settings uses one panel submit per swipe (no preview frames) plus one
  area cleanup about 0.8 s later, matching the intended page-snap profile.
- Network keeps two preview submits; the submit path itself is cheap, while
  the visible latency comes from the panel waveform, which this build does not
  instrument.
- `submit_us` for scene paints is ~1.6 ms; area cleanup is ~1.5 ms on the text
  area and ~0.17 ms on the smaller list area.
- Further timing work should add EPDC completion/timeout timestamps to measure
  actual panel waveform duration and to separate diagnostic logging overhead.

## Panel completion (`GW_UI_PERF_WAIT=1`)

A second diagnostic build forced `MXCFB_WAIT_FOR_UPDATE_COMPLETE` after every
submit, so `wait_us` measures the panel waveform rather than only the ioctl.
The production shell does not wait for partial updates; this build serializes
them and is for measurement only.

| Scenario | Waveform | Area % | `submit_us` | `wait_us` |
| --- | --- | ---: | ---: | ---: |
| Home/Settings/Network scene | 3 (GC16_FAST) | 100% | ~1,590 | 658,000–666,000 |
| Settings page-snap release | 3 | 74.3% | 1,590 | 663,720 |
| Settings area cleanup | 2 (GC16) | 74.3% | 1,581 | 13,105 |
| Network preview | 3 | 27.1% | 1,584 | 663,582 |
| Network preview | 3 | 27.1% | 1,586 | 660,190 |
| Network release | 8 (REAGL) | 27.1% | 1,939 | 669,515 |
| Network area cleanup | 2 (GC16) | 27.1% | 32 | 6,129 |

For Settings the user-visible path is roughly: 75 ms input→submit + 1.6 ms
submit + 664 ms panel completion, then the delayed area cleanup about 1.5 s
after input. The cleanup wait is unexpectedly short (6–13 ms); this needs a
follow-up measurement with a fresh marker/queue before treating it as a
general waveform timing.

The important correction to the earlier table: `submit_us` (~1.6 ms) is only
the ioctl cost. Full-screen and large-area GC16_FAST updates take roughly
0.66 s of panel time on this PW3.

## Waveform comparison on the same list area

Same screen (Network), same 420,036 px area (27.1%), forced scroll waveform
via the diagnostic `/run/gw-ui-test-scroll-wave` override:

| Waveform | Mode | `submit_us` | `wait_us` |
| --- | ---: | ---: | ---: |
| GC16_FAST | 3 | ~1,580 | ~660,000–665,000 |
| REAGL | 8 | ~1,550–1,590 | ~5,500 |
| DU4 | 7 | ~1,560–1,600 | ~5,500 |
| GL16 | 5 | ~1,550–1,590 | ~5,500 |
| A2 | 4 | ~1,570–1,590 | ~5,500 |

Caveat: `MXCFB_WAIT_FOR_UPDATE_COMPLETE` returned ~5.5 ms for every non-
GC16_FAST mode, which is not plausible as full panel time and is probably
marker/queue behaviour in this kernel. Only the GC16_FAST full/large-area
measurements (~0.66 s) are trusted as panel duration. A controlled visual
ghosting comparison (same clean text pattern, no page-snap) still needs to be
run before choosing REAGL/DU4 for any profile.

The diagnostic shell was removed and production `3083b5de…` restored after
these measurements.

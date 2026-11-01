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

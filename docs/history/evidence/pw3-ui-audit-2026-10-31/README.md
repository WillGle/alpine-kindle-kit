---
status: historical
date: 2026-11-02
superseded_by: ../../../status/CURRENT.md
---

# UI audit evidence — 2026-10-31

Host-only, source-included harness (`os/ui/host_sim.c` + real `gw-shell.c`).

```text
./os/ui/host-sim /tmp/opencode/pw3-ui-longlist2
lvgl heap: total=262144 used=173256 max_used=69356 free=88888 used_pct=67%
geometry checks: 789 total, 0 failed
```

Selected checks:

- `home/rapid_status_no_rewrite`: PASS — 10 rapid status updates with unchanged text write no label
- `apps/long_list_rows`: PASS — eight app rows are built
- `apps/long_list_last_row_reachable`: PASS — last row is scrollable into view

`apps-long.png` is the 1072x1448 screenshot after scrolling the 8-row app list
to the last row. `longlist-geometry.json` is the full report.

This proves the host-side long-list and repeated-status (no rewrite) behaviour.
It does not prove physical E-ink repaint behaviour; that remains a device gate.

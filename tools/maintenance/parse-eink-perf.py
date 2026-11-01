#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Parse GW_UI performance logs and report median/P95 latency metrics.

Usage:
  parse-eink-perf.py /path/to/log.txt
  parse-eink-perf.py --json /path/to/log.txt
"""
import argparse
import json
import math
import re
import statistics
import sys
from collections import Counter, defaultdict


METRICS = {
    "render": re.compile(r"render handler_us=(\d+)"),
    "flush_copy": re.compile(r"flush copy .* copy_us=(\d+)"),
    "epdc_submit": re.compile(r"epdc submit .* submit_us=(\d+)"),
    "epdc_complete": re.compile(r"epdc complete .* wait_us=(\d+)"),
    "input_to_render": re.compile(r"input_to_render_us=(-?\d+)"),
    "input_to_submit": re.compile(r"input_to_submit_us=(-?\d+)"),
}


def percentile(values, pct):
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    rank = (pct / 100.0) * (len(ordered) - 1)
    low = int(math.floor(rank))
    high = int(math.ceil(rank))
    if low == high:
        return ordered[low]
    frac = rank - low
    return ordered[low] + (ordered[high] - ordered[low]) * frac


def summarize(values):
    if not values:
        return {"count": 0, "median": None, "p95": None, "min": None, "max": None}
    return {
        "count": len(values),
        "median": statistics.median(values),
        "p95": percentile(values, 95),
        "min": min(values),
        "max": max(values),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", help="log file containing GW_UI lines")
    parser.add_argument("--json", action="store_true", help="emit JSON")
    args = parser.parse_args()

    values = defaultdict(list)
    for name in METRICS:
        values[name]  # ensure all metrics are represented
    waveform_counts = Counter()
    update_count = 0
    dirty_lines = 0
    try:
        with open(args.log, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                for name, regex in METRICS.items():
                    match = regex.search(line)
                    if match:
                        values[name].append(int(match.group(1)))
                wm = re.search(r"epdc submit .* waveform=(\d+)", line)
                if wm:
                    waveform_counts[wm.group(1)] += 1
                uc = re.search(r"epdc submit .* count=(\d+)", line)
                if uc:
                    update_count = max(update_count, int(uc.group(1)))
                if "flush copy" in line:
                    dirty_lines += 1
    except OSError as exc:
        parser.error(str(exc))

    report = {
        "metrics_us": {name: summarize(vals) for name, vals in values.items()},
        "waveform_counts": dict(waveform_counts),
        "max_update_count": update_count,
        "flush_copy_events": dirty_lines,
    }

    if args.json:
        print(json.dumps(report, indent=2))
        return

    print("PW3 E-ink performance summary")
    print("============================")
    for name in ("render", "flush_copy", "epdc_submit", "epdc_complete", "input_to_render", "input_to_submit"):
        s = report["metrics_us"][name]
        print(f"{name:16s} n={s['count']:5d} median={s['median']} p95={s['p95']} min={s['min']} max={s['max']} us")
    print("waveform_counts:", report["waveform_counts"])
    print("max_update_count:", report["max_update_count"])
    print("flush_copy_events:", report["flush_copy_events"])


if __name__ == "__main__":
    main()

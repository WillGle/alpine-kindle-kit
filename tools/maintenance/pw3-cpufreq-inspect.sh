#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only cpufreq inspection for PW3. Run on the device.
set -eu
base=/sys/devices/system/cpu/cpu0/cpufreq
for f in scaling_governor scaling_available_governors scaling_available_frequencies \
         scaling_min_freq scaling_max_freq scaling_cur_freq cpuinfo_min_freq cpuinfo_max_freq; do
    if [ -r "$base/$f" ]; then
        printf '%s=' "$f"
        cat "$base/$f"
    fi
done
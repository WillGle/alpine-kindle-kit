/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_epdc_policy.h"

int gw_epdc_should_periodic_clean(int partial_updates, unsigned idle_ms,
                                  int touch_down) {
    return partial_updates >= GW_EPDC_PARTIAL_LIMIT && idle_ms >= 600 &&
           !touch_down;
}

int gw_epdc_scroll_preview_allowed(unsigned count, unsigned max_frames,
                                   unsigned now_ms, unsigned last_ms,
                                   unsigned interval_ms) {
    if (interval_ms == 0 || max_frames == 0) return 0;
    if (count >= max_frames) return 0;
    if (last_ms != 0 && (unsigned)(now_ms - last_ms) < interval_ms) return 0;
    return 1;
}

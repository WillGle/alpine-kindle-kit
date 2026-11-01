/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pure EPDC paint-policy decisions.  No LVGL, ioctl or framebuffer access, so
 * the rules can be exercised on the host. */
#ifndef GW_EPDC_POLICY_H
#define GW_EPDC_POLICY_H

#define GW_EPDC_PARTIAL_LIMIT 40

/* True when the idle safety budget should request one GC16 cleanup. */
int gw_epdc_should_periodic_clean(int partial_updates, unsigned idle_ms,
                                  int touch_down);

/* True when another throttled scroll-preview frame is allowed.  count is the
 * number already painted, interval_ms==0 disables previews. */
int gw_epdc_scroll_preview_allowed(unsigned count, unsigned max_frames,
                                   unsigned now_ms, unsigned last_ms,
                                   unsigned interval_ms);

#endif /* GW_EPDC_POLICY_H */

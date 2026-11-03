/* SPDX-License-Identifier: GPL-3.0-or-later
 * PW3 EPDC/framebuffer port for LVGL.
 *
 * The PW3 framebuffer is 8 bpp (Y8), resolution 1072×1448, pitch 1088 bytes.
 * EPDC waveform updates are triggered via ioctl(MXCFB_SEND_UPDATE) on /dev/fb0.
 *
 * CURRENT STATUS (2026-10-30): stub. The framebuffer is opened and the LVGL
 * display driver is registered with the correct geometry. The flush callback
 * logs the dirty rectangle and returns without touching hardware.
 *
 * BEFORE enabling real rendering:
 *   1. Run the LVGL/EPDC/evdev spike on the real PW3 (8 bpp, pitch-1088).
 *   2. Confirm GC16 waveform updates display correctly at that geometry.
 *   3. Enable GW_UI_EPDC_ACCEPTED=1 and re-test.
 * Do not claim rendering works until the spike passes on real hardware.
 * GW_ENABLE_HW_TRIAL=1 enables the write path for a bring-up trial only.
 *
 * STATUS: SPIKE (accepted=0).
 *
 * Waveform constants are from the vendor i.MX6 EPDC driver header included
 * with the PW3 kernel source; only values confirmed on the actual panel are
 * used here.
 */
#ifndef GW_LEVEL_EPDC_H
#define GW_LEVEL_EPDC_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <fcntl.h>
#include <linux/fb.h>
#include <lvgl/lvgl.h>
#include "gw_epdc_policy.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* High-frequency render/EPDC diagnostics are a development tool, not a
 * production default.  A diagnostic build is made with GW_UI_PERF=1; even
 * then output stays off unless GW_UI_PERF_LOG=1 or /run/gw-ui-perf exists. */
#ifndef GW_UI_PERF
#define GW_UI_PERF 0
#endif

#if GW_UI_PERF
static int gw_perf_log_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        const char *value = getenv("GW_UI_PERF_LOG");
        if (value) enabled = value[0] != '\0' && strcmp(value, "0") != 0;
        else enabled = access("/run/gw-ui-perf", F_OK) == 0;
    }
    return enabled;
}
#define GW_PERF_LOG(...) do { if (gw_perf_log_enabled()) dprintf(2, __VA_ARGS__); } while (0)
#else
#define GW_PERF_LOG(...) do { } while (0)
#endif
/* Host-only LVGL preview: render into a plain memory framebuffer and write
 * PPM snapshots.  Never opened on the device, and /dev/fb0 is not required. */
#ifndef GW_UI_HOST_SIM
#define GW_UI_HOST_SIM 0
#endif

/* Set to 1 only after real-device rendering acceptance. */
#ifndef GW_UI_EPDC_ACCEPTED
#define GW_UI_EPDC_ACCEPTED 0
#endif
/* Explicit trial gate for hardware writes during bring-up.  It is separate
 * from ACCEPTED and never implies acceptance; default is off. */
#ifndef GW_ENABLE_HW_TRIAL
#define GW_ENABLE_HW_TRIAL 0
#endif

/* PW3 panel geometry — verified in dry-run evidence. */
#define GW_FB_WIDTH   1072
#define GW_FB_HEIGHT  1448
#define GW_FB_PITCH   1088   /* bytes per row including padding */
#define GW_FB_BPP        8   /* Y8 greyscale */
#define GW_FB_DEV     "/dev/fb0"

/* EPDC waveform constants from i.MX6 vendor kernel / Kindle header. */
#define WAVEFORM_MODE_DU        1
#define WAVEFORM_MODE_GC16      2
#define WAVEFORM_MODE_GC16_FAST 3   /* 16-level, no GC16 clear phase */
#define WAVEFORM_MODE_A2        4
#define WAVEFORM_MODE_GL16      5
#define WAVEFORM_MODE_DU4       7
#define WAVEFORM_MODE_REAGL     8
#define WAVEFORM_MODE_AUTO      257
#define UPDATE_MODE_PARTIAL     0
#define UPDATE_MODE_FULL        1

/* EPDC flags used by the PW3 kernel (lab126 additions). */
#define EPDC_FLAG_FORCE_MONOCHROME 2

/* Safety budget: after this many non-deep partial updates a GC16 cleanup is
 * allowed when the UI is idle.  This is a fallback only; the normal ghost
 * cleanup point is lock/unlock, where a full refresh is expected. */
#ifndef GW_EPDC_PARTIAL_LIMIT
#define GW_EPDC_PARTIAL_LIMIT 30
#endif

/*
 * PW3 kernel update ABI. The full layout is required even when alternate
 * buffers and histogram waveforms are unused: the ioctl encodes its size.
 */
struct gw_mxcfb_rect { uint32_t top, left, width, height; };
struct gw_mxcfb_alt_buffer_data {
    uint32_t phys_addr, width, height;
    struct gw_mxcfb_rect alt_update_region;
};
struct gw_mxcfb_update_data {
    struct gw_mxcfb_rect update_region;
    uint32_t waveform_mode;
    uint32_t update_mode;
    uint32_t update_marker;
    uint32_t hist_bw_waveform_mode;
    uint32_t hist_gray_waveform_mode;
    int      temp;
    unsigned flags;
    struct gw_mxcfb_alt_buffer_data alt_buffer_data;
};
#define MXCFB_SEND_UPDATE _IOW('F', 0x2E, struct gw_mxcfb_update_data)
struct gw_mxcfb_update_marker { uint32_t update_marker, collision_test; };
#define MXCFB_WAIT_FOR_UPDATE_COMPLETE _IOWR('F', 0x2F, struct gw_mxcfb_update_marker)

/* ── Timing helper (monotonic microseconds) ─────────────────────────────── */
static inline long long gw_now_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (long long)ts.tv_sec * 1000000LL + (long long)ts.tv_nsec / 1000LL;
}

/* ── RGB332 -> Y8 lookup table (profiling/optimization) ─────────────────── */
/* One translation unit owns the EPDC state; every other includer sees extern
 * declarations.  This keeps the host test binary and the shell on the same
 * panel state without including a .c file. */
#ifdef GW_EPDC_STATE_IMPLEMENTATION
#define GW_EPDC_VAR(type, name, init) type name init
#else
#define GW_EPDC_VAR(type, name, init) extern type name
#endif

GW_EPDC_VAR(uint8_t, g_gray_lut[256], );
GW_EPDC_VAR(uint8_t, g_gray_to_rgb332[256], );
GW_EPDC_VAR(int, g_gray_lut_ready, = 0);
static inline void gw_init_gray_lut(void) {
    unsigned i, v;
    if (g_gray_lut_ready) return;
    for (i = 0; i < 256; i++) {
        unsigned r = ((i >> 5) & 7) * 255 / 7;
        unsigned g = ((i >> 2) & 7) * 255 / 7;
        unsigned b = (i & 3) * 255 / 3;
        g_gray_lut[i] = (uint8_t)((299 * r + 587 * g + 114 * b + 500) / 1000);
    }
    /* The panel is Y8 but LVGL is RGB332.  Raw 8 bpp images must be encoded
     * as the RGB332 value that maps back to the intended gray; otherwise a
     * mid-gray byte is treated as dark red and the image renders far too
     * dark.  Prefer an exact match, then the nearest representable gray. */
    for (v = 0; v < 256; v++) {
        unsigned best = 0, best_delta = 0xffffffffu;
        for (i = 0; i < 256; i++) {
            unsigned delta = g_gray_lut[i] > v ? g_gray_lut[i] - v
                                               : v - g_gray_lut[i];
            if (delta < best_delta) {
                best_delta = delta;
                best = i;
                if (!delta) break;
            }
        }
        g_gray_to_rgb332[v] = (uint8_t)best;
    }
    g_gray_lut_ready = 1;
}
static inline uint8_t gw_rgb332_to_gray_lut(uint8_t pixel) {
    if (!g_gray_lut_ready) gw_init_gray_lut();
    return g_gray_lut[pixel];
}
/* Convert a raw Y8 buffer in place to the RGB332 encoding the flush path
 * expects.  Used for Hub frames and the KOReader lock-screen export. */
static inline uint8_t gw_y8_to_rgb332(uint8_t gray) {
    if (!g_gray_lut_ready) gw_init_gray_lut();
    return g_gray_to_rgb332[gray];
}
static inline void gw_y8_to_rgb332_buffer(uint8_t *pixels, size_t count) {
    size_t i;
    if (!pixels) return;
    if (!g_gray_lut_ready) gw_init_gray_lut();
    for (i = 0; i < count; i++) pixels[i] = g_gray_to_rgb332[pixels[i]];
}

/* Port state. */
GW_EPDC_VAR(int, g_fb_fd, = -1);
GW_EPDC_VAR(void *, g_fb_mem, = MAP_FAILED);
/* QEMU smoke mode: build and render into LVGL buffers only, never touch
 * /dev/fb0.  Enabled explicitly with GW_UI_SMOKE_FB=1. */
GW_EPDC_VAR(int, g_epdc_smoke, = 0);

/* Monotonic microsecond timestamp of the most recent completed touch frame.
 * Used to correlate input, render and EPDC submit latency. */
GW_EPDC_VAR(long long, g_last_input_us, = 0);
GW_EPDC_VAR(lv_disp_draw_buf_t, g_draw_buf, );
/* A small draw buffer keeps RAM use low on the PW3; dirty rectangles are
 * coalesced into one EPDC update per cycle, so a full-screen buffer is not
 * needed. */
GW_EPDC_VAR(lv_color_t, g_buf1[GW_FB_WIDTH * 32], );
GW_EPDC_VAR(lv_disp_drv_t, g_disp_drv, );

/* Refresh intent.  Every repaint is attributed to exactly one intent so the
 * waveform cannot be chosen by accident from the dirty area alone. */
typedef enum {
    GW_EINK_INTENT_NONE = 0,
    GW_EINK_INTENT_SCENE,       /* screen change / modal / lock-unlock */
    GW_EINK_INTENT_SCROLL,      /* list drag, painted on release */
    GW_EINK_INTENT_CONTENT,     /* text, clock, one row */
    GW_EINK_INTENT_FEEDBACK,    /* pressed state, toast */
    GW_EINK_INTENT_DEEP_CLEAN   /* explicit GC16 ghost cleanup */
} gw_eink_intent_t;

typedef enum { GW_EINK_STATE_IDLE = 0, GW_EINK_STATE_INTERACTIVE = 1,
               GW_EINK_STATE_SETTLE = 2 } gw_eink_state_t;
GW_EPDC_VAR(gw_eink_state_t, g_eink_state, = GW_EINK_STATE_IDLE);
GW_EPDC_VAR(uint32_t, g_last_input_ms, = 0);
GW_EPDC_VAR(int, g_settle_refresh_pending, = 0);
GW_EPDC_VAR(int, g_force_full_update, = 0);
GW_EPDC_VAR(int, g_eink_text_input, = 0);
GW_EPDC_VAR(uint32_t, g_force_full_waveform, = 0);
GW_EPDC_VAR(gw_eink_intent_t, g_eink_intent, = GW_EINK_INTENT_NONE);
GW_EPDC_VAR(uint32_t, g_eink_scene_waveform, = WAVEFORM_MODE_GC16_FAST);
GW_EPDC_VAR(uint32_t, g_eink_transition_until_ms, = 0);
GW_EPDC_VAR(uint32_t, g_eink_scroll_until_ms, = 0);
/* Touch is held down.  While a scroll intent is active the EPDC submit is
 * postponed so the panel paints once on release (KOReader-style). */
GW_EPDC_VAR(int, g_eink_touch_down, = 0);
GW_EPDC_VAR(int, g_eink_scroll_release, = 0);
GW_EPDC_VAR(int, g_eink_scroll_moved, = 0);
/* Live scroll preview while the finger is down.  0 disables preview; the
 * default ~5 Hz is the e-ink settle limit, not an LCD frame rate. */
GW_EPDC_VAR(uint32_t, g_eink_scroll_preview_ms, = 250);
GW_EPDC_VAR(uint32_t, g_eink_last_scroll_ms, = 0);
GW_EPDC_VAR(uint32_t, g_eink_scroll_waveform, = 0);
GW_EPDC_VAR(uint32_t, g_eink_scroll_release_waveform, = 0);
GW_EPDC_VAR(uint32_t, g_eink_scroll_preview_max, = 4);
GW_EPDC_VAR(uint32_t, g_eink_scroll_preview_count, = 0);
/* Screen-space bounds of the visible virtual keyboard, if any. */
GW_EPDC_VAR(int, g_eink_kbd_valid, = 0);
GW_EPDC_VAR(int32_t, g_eink_kbd_x1, = 0);
GW_EPDC_VAR(int32_t, g_eink_kbd_y1, = 0);
GW_EPDC_VAR(int32_t, g_eink_kbd_x2, = -1);
GW_EPDC_VAR(int32_t, g_eink_kbd_y2, = -1);
GW_EPDC_VAR(int, g_eink_popup_valid, = 0);
GW_EPDC_VAR(int32_t, g_eink_popup_x1, = 0);
GW_EPDC_VAR(int32_t, g_eink_popup_y1, = 0);
GW_EPDC_VAR(int32_t, g_eink_popup_x2, = -1);
GW_EPDC_VAR(int32_t, g_eink_popup_y2, = -1);
static inline uint32_t gw_eink_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
static inline void gw_eink_set_text_input(int active) {
    g_eink_text_input = active ? 1 : 0;
    if (active) g_eink_intent = GW_EINK_INTENT_CONTENT;
}
static inline void gw_eink_set_keyboard_rect(int valid, int32_t x1, int32_t y1,
                                             int32_t x2, int32_t y2) {
    g_eink_kbd_valid = valid ? 1 : 0;
    g_eink_kbd_x1 = x1;
    g_eink_kbd_y1 = y1;
    g_eink_kbd_x2 = x2;
    g_eink_kbd_y2 = y2;
}
static inline uint32_t gw_eink_scene_waveform(void) {
    return g_eink_scene_waveform;
}
static inline void gw_eink_set_scene_waveform(uint32_t waveform) {
    g_eink_scene_waveform = waveform;
}
/* Scene/modal transition: one 16-level partial for the whole panel. */
static inline void gw_eink_request_transition(uint32_t ms) {
    g_eink_transition_until_ms = gw_eink_now_ms() + ms;
    g_eink_intent = GW_EINK_INTENT_SCENE;
}
static inline void gw_eink_request_scene(void) {
    g_eink_transition_until_ms = gw_eink_now_ms() + 800;
    g_eink_intent = GW_EINK_INTENT_SCENE;
}
/* Scroll is painted on release; this only marks the intent and window. */
static inline void gw_eink_request_scroll(uint32_t ms) {
    g_eink_scroll_until_ms = gw_eink_now_ms() + ms;
    g_eink_scroll_moved = 1;
    g_eink_intent = GW_EINK_INTENT_SCROLL;
}
static inline void gw_eink_request_content(void) {
    if (g_eink_intent == GW_EINK_INTENT_NONE)
        g_eink_intent = GW_EINK_INTENT_CONTENT;
}
static inline void gw_eink_request_feedback(void) {
    if (g_eink_intent == GW_EINK_INTENT_NONE)
        g_eink_intent = GW_EINK_INTENT_FEEDBACK;
}
static inline int gw_eink_transition_active(void) {
    return g_eink_transition_until_ms != 0 &&
           (int32_t)(gw_eink_now_ms() - g_eink_transition_until_ms) < 0;
}
static inline int gw_eink_scroll_active(void) {
    return g_eink_scroll_until_ms != 0 &&
           (int32_t)(gw_eink_now_ms() - g_eink_scroll_until_ms) < 0;
}
static inline int gw_eink_scroll_pending(void) {
    return g_eink_intent == GW_EINK_INTENT_SCROLL;
}
static inline gw_eink_intent_t gw_eink_current_intent(void) {
    return g_eink_intent;
}
static inline void gw_eink_set_touch_down(int down) {
    if (down) {
        g_eink_touch_down = 1;
        g_eink_scroll_release = 0;
        g_eink_scroll_preview_count = 0;
    } else {
        if (g_eink_touch_down && g_eink_intent == GW_EINK_INTENT_SCROLL)
            g_eink_scroll_release = 1;
        g_eink_touch_down = 0;
    }
}
static inline int gw_eink_touch_is_down(void) {
    return g_eink_touch_down;
}
/* Input port calls this once a drag has travelled far enough to be a scroll. */
static inline void gw_eink_note_scroll_move(void) {
    g_eink_scroll_moved = 1;
    gw_eink_request_scroll(1500);
}
static inline int gw_eink_scroll_was_moved(void) {
    return g_eink_scroll_moved;
}
static inline void gw_eink_clear_scroll_move(void) {
    g_eink_scroll_moved = 0;
}
static inline void gw_eink_set_scroll_preview_ms(uint32_t ms) {
    g_eink_scroll_preview_ms = ms;
}
static inline uint32_t gw_eink_scroll_preview_ms(void) {
    return g_eink_scroll_preview_ms;
}
static inline void gw_eink_set_scroll_waveform(uint32_t waveform) {
    g_eink_scroll_waveform = waveform;
}
static inline uint32_t gw_eink_scroll_waveform(void) {
    return g_eink_scroll_waveform;
}
static inline void gw_eink_set_scroll_release_waveform(uint32_t waveform) {
    g_eink_scroll_release_waveform = waveform;
}
static inline uint32_t gw_eink_scroll_release_waveform(void) {
    return g_eink_scroll_release_waveform;
}
static inline void gw_eink_set_scroll_preview_max(uint32_t count) {
    g_eink_scroll_preview_max = count;
}
static inline uint32_t gw_eink_scroll_preview_max(void) {
    return g_eink_scroll_preview_max;
}
static inline void gw_eink_set_popup_rect(int valid, int32_t x1, int32_t y1,
                                           int32_t x2, int32_t y2) {
    g_eink_popup_valid = valid ? 1 : 0;
    g_eink_popup_x1 = x1;
    g_eink_popup_y1 = y1;
    g_eink_popup_x2 = x2;
    g_eink_popup_y2 = y2;
}
static inline uint32_t gw_eink_idle_ms(void) {
    uint32_t now;
    if (g_last_input_ms == 0) return 0xffffffffU;
    now = gw_eink_now_ms();
    return now - g_last_input_ms;
}
static inline void gw_eink_note_input(void) {
    g_last_input_ms = gw_eink_now_ms();
    g_eink_state = GW_EINK_STATE_INTERACTIVE;
    g_settle_refresh_pending = 0;
}
static inline void gw_eink_update_state(void) {
    uint32_t idle_for;
    if (g_last_input_ms == 0) return;
    idle_for = gw_eink_idle_ms();
    if (g_eink_state == GW_EINK_STATE_INTERACTIVE && idle_for >= 250)
        g_eink_state = GW_EINK_STATE_SETTLE;
    if (g_eink_state == GW_EINK_STATE_SETTLE && idle_for >= 1200)
        g_eink_state = GW_EINK_STATE_IDLE;
}

#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
GW_EPDC_VAR(uint32_t, g_update_marker, );
GW_EPDC_VAR(int, g_epdc_error, );
GW_EPDC_VAR(int, g_update_pending, );
GW_EPDC_VAR(int, g_last_update_full, );
GW_EPDC_VAR(int, g_dirty_valid, );

typedef struct {
    int32_t x1, y1, x2, y2;
} gw_epdc_rect_t;

#define GW_EPDC_MAX_DIRTY 8
GW_EPDC_VAR(gw_epdc_rect_t, g_dirty_rects[GW_EPDC_MAX_DIRTY], );
GW_EPDC_VAR(int, g_dirty_count, );
/* Union of every area painted during one drag.  The release refresh uses this
 * so GC16 also cleans residue left by the earlier preview frames, not just the
 * last delta. */
GW_EPDC_VAR(gw_epdc_rect_t, g_scroll_union, );
GW_EPDC_VAR(int, g_scroll_union_valid, );
/* Area to clean with a partial GC16 after a drag settles.  Keeping this
 * separate from g_scroll_union lets the release paint finish first. */
GW_EPDC_VAR(gw_epdc_rect_t, g_scroll_cleanup_rect, );
GW_EPDC_VAR(int, g_scroll_cleanup_valid, );
GW_EPDC_VAR(int, g_force_scroll_cleanup, );
GW_EPDC_VAR(int, g_scroll_cleanup_area_only, = 1);
GW_EPDC_VAR(int, g_periodic_full_after, = 0);
GW_EPDC_VAR(int, g_split_page_pending, = 0);
GW_EPDC_VAR(uint32_t, g_scroll_cleanup_at_ms, );
GW_EPDC_VAR(uint32_t, g_last_scroll_cleanup_ms, );
GW_EPDC_VAR(uint32_t, g_scroll_clean_delay_ms, = 800);
GW_EPDC_VAR(uint32_t, g_scroll_clean_min_ms, = 3000);
GW_EPDC_VAR(unsigned, g_partial_updates, );

static inline int gw_eink_scroll_cleanup_pending(void) {
    return g_scroll_union_valid && !g_eink_touch_down &&
           g_eink_intent == GW_EINK_INTENT_SCROLL;
}

/* After a drag, clean the panel only once the user has stopped scrolling.
 * The PW3 panel needs a full GC16 to remove scroll residue; doing it on every
 * release flashed constantly, so delay it the same way an e-reader does. */
static inline void gw_eink_set_scroll_clean_delay(uint32_t ms) {
    g_scroll_clean_delay_ms = ms;
}

static inline void gw_eink_set_scroll_clean_min(uint32_t ms) {
    g_scroll_clean_min_ms = ms;
}
static inline void gw_epdc_set_scroll_cleanup_area(int area_only) {
    g_scroll_cleanup_area_only = area_only ? 1 : 0;
}
static inline void gw_epdc_set_split_page(int split) {
    g_split_page_pending = split ? 1 : 0;
}

static inline void gw_epdc_schedule_scroll_cleanup(void) {
    if (g_scroll_clean_delay_ms) {
        g_scroll_cleanup_at_ms = gw_eink_now_ms() + g_scroll_clean_delay_ms;
        dprintf(2, "GW_UI: scroll cleanup scheduled +%u ms\n",
                g_scroll_clean_delay_ms);
    }
}

static inline int gw_epdc_scroll_cleanup_due(void) {
    uint32_t now = gw_eink_now_ms();
    return g_scroll_cleanup_at_ms != 0 &&
           (int32_t)(now - g_scroll_cleanup_at_ms) >= 0 &&
           !g_eink_touch_down &&
           g_eink_intent != GW_EINK_INTENT_SCROLL &&
           gw_eink_idle_ms() >= g_scroll_clean_delay_ms &&
           (g_last_scroll_cleanup_ms == 0 ||
            (uint32_t)(now - g_last_scroll_cleanup_ms) >=
                g_scroll_clean_min_ms);
}

static inline void gw_epdc_scroll_cleanup_done(void) {
    g_last_scroll_cleanup_ms = gw_eink_now_ms();
    g_scroll_cleanup_at_ms = 0;
}

/* A non-scroll action (for example the brightness controls) must not inherit
 * a cleanup scheduled by an earlier drag. */
static inline void gw_epdc_cancel_scroll_cleanup(void) {
    g_scroll_cleanup_at_ms = 0;
    g_scroll_cleanup_valid = 0;
    g_force_scroll_cleanup = 0;
}
GW_EPDC_VAR(unsigned, g_update_count, );
GW_EPDC_VAR(uint32_t, g_epdc_last_error_log_ms, );
GW_EPDC_VAR(const char *, g_epdc_full_reason, = "explicit");

static unsigned long long gw_epdc_rect_area(const gw_epdc_rect_t *r) {
    return (unsigned long long)(r->x2 - r->x1 + 1) *
           (unsigned long long)(r->y2 - r->y1 + 1);
}

static void gw_epdc_union_rect(gw_epdc_rect_t *dst, const gw_epdc_rect_t *src,
                               int *valid) {
    if (!*valid) {
        *dst = *src;
        *valid = 1;
        return;
    }
    if (src->x1 < dst->x1) dst->x1 = src->x1;
    if (src->y1 < dst->y1) dst->y1 = src->y1;
    if (src->x2 > dst->x2) dst->x2 = src->x2;
    if (src->y2 > dst->y2) dst->y2 = src->y2;
}

static int gw_epdc_near(const gw_epdc_rect_t *a, const gw_epdc_rect_t *b,
                        int32_t gap) {
    return !(a->x2 + gap < b->x1 || b->x2 + gap < a->x1 ||
             a->y2 + gap < b->y1 || b->y2 + gap < a->y1);
}

static void gw_epdc_compact_dirty(void) {
    int i, j;
    for (i = 0; i < g_dirty_count; i++) {
        for (j = i + 1; j < g_dirty_count; j++) {
            if (gw_epdc_near(&g_dirty_rects[i], &g_dirty_rects[j], 2)) {
                if (g_dirty_rects[j].x1 < g_dirty_rects[i].x1)
                    g_dirty_rects[i].x1 = g_dirty_rects[j].x1;
                if (g_dirty_rects[j].y1 < g_dirty_rects[i].y1)
                    g_dirty_rects[i].y1 = g_dirty_rects[j].y1;
                if (g_dirty_rects[j].x2 > g_dirty_rects[i].x2)
                    g_dirty_rects[i].x2 = g_dirty_rects[j].x2;
                if (g_dirty_rects[j].y2 > g_dirty_rects[i].y2)
                    g_dirty_rects[i].y2 = g_dirty_rects[j].y2;
                g_dirty_rects[j] = g_dirty_rects[--g_dirty_count];
                i = -1;
                break;
            }
        }
    }
    g_dirty_valid = g_dirty_count > 0;
}

/* Keep distant dirty regions separate so a keyboard key does not inflate the
 * region of a text field.  Present() still submits one intent per cycle. */
static void gw_epdc_add_dirty(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    gw_epdc_rect_t r;
    int i;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= GW_FB_WIDTH) x2 = GW_FB_WIDTH - 1;
    if (y2 >= GW_FB_HEIGHT) y2 = GW_FB_HEIGHT - 1;
    if (x1 > x2 || y1 > y2) return;
    r.x1 = x1; r.y1 = y1; r.x2 = x2; r.y2 = y2;

    if (g_dirty_count < GW_EPDC_MAX_DIRTY) {
        g_dirty_rects[g_dirty_count++] = r;
    } else {
        unsigned long long best = ~0ULL;
        int best_i = 0;
        for (i = 0; i < g_dirty_count; i++) {
            gw_epdc_rect_t u = g_dirty_rects[i];
            unsigned long long growth;
            if (r.x1 < u.x1) u.x1 = r.x1;
            if (r.y1 < u.y1) u.y1 = r.y1;
            if (r.x2 > u.x2) u.x2 = r.x2;
            if (r.y2 > u.y2) u.y2 = r.y2;
            growth = gw_epdc_rect_area(&u) - gw_epdc_rect_area(&g_dirty_rects[i]);
            if (growth < best) { best = growth; best_i = i; }
        }
        if (r.x1 < g_dirty_rects[best_i].x1) g_dirty_rects[best_i].x1 = r.x1;
        if (r.y1 < g_dirty_rects[best_i].y1) g_dirty_rects[best_i].y1 = r.y1;
        if (r.x2 > g_dirty_rects[best_i].x2) g_dirty_rects[best_i].x2 = r.x2;
        if (r.y2 > g_dirty_rects[best_i].y2) g_dirty_rects[best_i].y2 = r.y2;
    }
    gw_epdc_compact_dirty();
}

/* Queue one full-screen update.  Only explicit deep-clean callers reach this;
 * the dirty area itself can never promote a frame to GC16. */
static void gw_epdc_request_full_waveform_reason(uint32_t waveform,
                                                const char *reason) {
    g_epdc_full_reason = reason ? reason : "explicit";
    g_dirty_rects[0].x1 = 0;
    g_dirty_rects[0].y1 = 0;
    g_dirty_rects[0].x2 = GW_FB_WIDTH - 1;
    g_dirty_rects[0].y2 = GW_FB_HEIGHT - 1;
    g_dirty_count = 1;
    g_dirty_valid = 1;
    g_force_full_update = 1;
    g_force_full_waveform = waveform;
    g_eink_intent = GW_EINK_INTENT_DEEP_CLEAN;
}

static void gw_epdc_request_full_waveform(uint32_t waveform) {
    gw_epdc_request_full_waveform_reason(waveform, "deep_clean");
}

static void gw_epdc_request_full(void) {
    gw_epdc_request_full_waveform_reason(WAVEFORM_MODE_GC16, "lock");
}

static inline void gw_epdc_set_periodic_full_after(int updates) {
    g_periodic_full_after = updates > 0 ? updates : 0;
    /* A budget clean requested by the previous screen must not fire after a
     * switch to a profile that explicitly disables periodic cleaning. */
    if (g_periodic_full_after == 0 && g_force_full_update &&
        g_epdc_full_reason && !strcmp(g_epdc_full_reason, "budget")) {
        g_force_full_update = 0;
        g_epdc_full_reason = "cancelled";
    }
}

static int gw_epdc_finish(void) {
    struct gw_mxcfb_update_marker marker;
    if (g_epdc_smoke) return 0;
#if GW_UI_PERF
    long long t0, t1;
#endif
    if (!g_update_pending) return 0;
#if GW_UI_PERF
    t0 = gw_now_us();
#endif
    memset(&marker, 0, sizeof(marker));
    marker.update_marker = g_update_marker;
    if (ioctl(g_fb_fd, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &marker) < 0) {
        uint32_t now = gw_eink_now_ms();
        if (g_epdc_last_error_log_ms == 0 ||
            now - g_epdc_last_error_log_ms >= 5000) {
            dprintf(2, "GW_UI: EPDC update did not finish\n");
            g_epdc_last_error_log_ms = now;
        }
        g_epdc_error = 1;
    }
#if GW_UI_PERF
    t1 = gw_now_us();
#endif
    g_update_pending = 0;
#if GW_UI_PERF
    GW_PERF_LOG("GW_UI: epdc complete marker=%u wait_us=%lld err=%d\n",
                g_update_marker, t1 - t0, g_epdc_error);
#endif
    return g_epdc_error ? -1 : 0;
}

static const char *gw_epdc_intent_name(gw_eink_intent_t intent) {
    switch (intent) {
    case GW_EINK_INTENT_SCENE: return "scene";
    case GW_EINK_INTENT_SCROLL: return "scroll";
    case GW_EINK_INTENT_CONTENT: return "content";
    case GW_EINK_INTENT_FEEDBACK: return "feedback";
    case GW_EINK_INTENT_DEEP_CLEAN: return "deep_clean";
    default: return "none";
    }
}

static int gw_epdc_submit(const gw_epdc_rect_t *r, uint32_t waveform,
                          uint32_t mode, const char *reason) {
    struct gw_mxcfb_update_data upd;
#ifdef GW_UI_EPDC_UNIT_TEST
    /* Count submissions without touching /dev/fb0. */
    (void)r;
    (void)waveform;
    (void)reason;
    g_update_count++;
    g_update_pending = 1;
    g_last_update_full = mode == UPDATE_MODE_FULL;
    return 0;
#endif
    if (g_epdc_smoke) return 0;
    unsigned long long area = gw_epdc_rect_area(r);
#if GW_UI_PERF
    long long t0, t1;
#endif
#if GW_UI_PERF
    t0 = gw_now_us();
#endif
    memset(&upd, 0, sizeof(upd));
    upd.update_region.top    = (uint32_t)r->y1;
    upd.update_region.left   = (uint32_t)r->x1;
    upd.update_region.width  = (uint32_t)(r->x2 - r->x1 + 1);
    upd.update_region.height = (uint32_t)(r->y2 - r->y1 + 1);
    upd.waveform_mode = waveform;
    upd.update_mode   = mode;
    upd.hist_bw_waveform_mode   = WAVEFORM_MODE_DU;
    upd.hist_gray_waveform_mode = waveform == WAVEFORM_MODE_GL16
                                  ? WAVEFORM_MODE_GL16
                                  : WAVEFORM_MODE_GC16_FAST;
    upd.flags = 0;
    if (++g_update_marker == 0) ++g_update_marker;
    upd.update_marker = g_update_marker;
    upd.temp          = 0x1000; /* TEMP_USE_AMBIENT on the PW3 kernel */

    if (ioctl(g_fb_fd, MXCFB_SEND_UPDATE, &upd) < 0) {
        /* Firmware fallback: if a newer 16-level waveform is rejected, retry
         * once with DU4 so the UI is not left stale, and log the downgrade. */
        if (waveform == WAVEFORM_MODE_REAGL) {
            upd.waveform_mode = WAVEFORM_MODE_GC16_FAST;
            dprintf(2, "GW_UI: REAGL rejected, falling back to GC16_FAST\n");
            if (ioctl(g_fb_fd, MXCFB_SEND_UPDATE, &upd) == 0)
                goto submitted;
            waveform = WAVEFORM_MODE_GC16_FAST;
        }
        if (waveform == WAVEFORM_MODE_GC16_FAST ||
            waveform == WAVEFORM_MODE_GL16) {
            upd.waveform_mode = WAVEFORM_MODE_DU4;
            dprintf(2, "GW_UI: waveform %u rejected, falling back to DU4\n",
                    waveform);
            if (ioctl(g_fb_fd, MXCFB_SEND_UPDATE, &upd) == 0)
                goto submitted;
        }
        {
            uint32_t now = gw_eink_now_ms();
            if (g_epdc_last_error_log_ms == 0 ||
                now - g_epdc_last_error_log_ms >= 5000) {
                dprintf(2, "GW_UI: EPDC update failed\n");
                g_epdc_last_error_log_ms = now;
            }
            g_epdc_error = 1;
            return -1;
        }
    }
submitted:
    g_epdc_error = 0;
    g_update_pending = 1;
    g_last_update_full = mode == UPDATE_MODE_FULL;
    g_update_count++;
#if GW_UI_PERF
    t1 = gw_now_us();
    GW_PERF_LOG("GW_UI: epdc submit marker=%u waveform=%u mode=%u area=%llu intent=%s reason=%s partials=%u rect=%d,%d-%d,%d kbd=%d,%d-%d,%d text=%d merge_risk=0 submit_us=%lld count=%u input_to_submit_us=%lld\n",
                g_update_marker, waveform, mode, area,
                gw_epdc_intent_name(g_eink_intent), reason ? reason : "none",
                g_partial_updates, r->x1, r->y1, r->x2, r->y2,
                g_eink_kbd_x1, g_eink_kbd_y1, g_eink_kbd_x2, g_eink_kbd_y2,
                g_eink_text_input, t1 - t0, g_update_count,
                g_last_input_us ? (t1 - g_last_input_us) : -1);
#else
    (void)area;
    (void)reason;
#endif
#if GW_UI_PERF_WAIT
    /* Diagnostic-only: block until the panel reports completion so wait_us
     * measures the waveform, not just the ioctl submission. */
    gw_epdc_finish();
#endif
    return 0;
}

/* Submit exactly one intent per render cycle.
 *
 * Contract:
 *   - while a scroll drag is in progress the dirty list is retained, so the
 *     panel paints once on release (on-release scrolling);
 *   - the dirty area never promotes a frame to GC16;
 *   - every rect in a batch uses the same waveform and update mode, so the
 *     i.MX6 driver cannot merge them into FULL/AUTO;
 *   - non-scroll flushes submit each separated dirty rectangle on its own,
 *     while nearby rectangles were already compacted into one. */
static int gw_epdc_submit(const gw_epdc_rect_t *r, uint32_t waveform,
                          uint32_t mode, const char *reason);

static int gw_epdc_present(void) {
    gw_epdc_rect_t box;
    int count, i, scroll_intent;
    unsigned long long total = 0;
    uint32_t waveform;

#ifndef GW_UI_EPDC_UNIT_TEST
    if (g_epdc_smoke) return 0;
#endif
    if (g_force_scroll_cleanup) {
        gw_epdc_rect_t clean = g_scroll_cleanup_rect;
        g_force_scroll_cleanup = 0;
        g_scroll_cleanup_valid = 0;
        if (gw_epdc_finish() < 0) {
            g_force_scroll_cleanup = 1;
            return -1;
        }
        return gw_epdc_submit(&clean, WAVEFORM_MODE_GC16, UPDATE_MODE_PARTIAL,
                              "scroll_area_cleanup");
    }
    if (!g_dirty_valid || g_dirty_count == 0) {
        /* An explicit full clean has no dirty rect of its own. */
        if (g_force_full_update) {
            g_dirty_rects[0] = (gw_epdc_rect_t){ 0, 0,
                                                 GW_FB_WIDTH - 1,
                                                 GW_FB_HEIGHT - 1 };
            g_dirty_count = 1;
            g_dirty_valid = 1;
        } else if (g_scroll_union_valid &&
                   g_eink_intent == GW_EINK_INTENT_SCROLL &&
                   !g_eink_touch_down) {
            /* The last preview may already show the final viewport, so LVGL
             * has nothing left to invalidate on release.  Still run the
             * cleanup pass over the union of everything the drag painted. */
            g_dirty_rects[0] = g_scroll_union;
            g_dirty_count = 1;
            g_dirty_valid = 1;
        } else {
            return 0;
        }
    }

    /* Scroll: paint a throttled live preview while the finger is down, then
     * always paint the final viewport on release. */
    {
        int scroll_drag = g_eink_touch_down &&
                          g_eink_intent == GW_EINK_INTENT_SCROLL &&
                          !g_eink_scroll_release;
        if (scroll_drag) {
            /* Zero means "no preview".  Do not fall through to the normal
             * submit path: that repaints every drag frame and leaves heavy
             * residue on the PW3 panel. */
            if (!gw_epdc_scroll_preview_allowed(
                    g_eink_scroll_preview_count, g_eink_scroll_preview_max,
                    gw_eink_now_ms(), g_eink_last_scroll_ms,
                    g_eink_scroll_preview_ms))
                return 0;
        }
    }

    count = g_dirty_count;
    box = g_dirty_rects[0];
    for (i = 1; i < count; i++) {
        if (g_dirty_rects[i].x1 < box.x1) box.x1 = g_dirty_rects[i].x1;
        if (g_dirty_rects[i].y1 < box.y1) box.y1 = g_dirty_rects[i].y1;
        if (g_dirty_rects[i].x2 > box.x2) box.x2 = g_dirty_rects[i].x2;
        if (g_dirty_rects[i].y2 > box.y2) box.y2 = g_dirty_rects[i].y2;
        total += gw_epdc_rect_area(&g_dirty_rects[i]);
    }
    scroll_intent = g_eink_intent == GW_EINK_INTENT_SCROLL;
    if (scroll_intent) {
        gw_epdc_union_rect(&g_scroll_union, &box, &g_scroll_union_valid);
        /* On release, clean the whole area touched by the drag, including the
         * earlier preview frames, instead of only the last delta. */
        if (!g_eink_touch_down && g_scroll_union_valid) {
            box = g_scroll_union;
            g_scroll_cleanup_rect = g_scroll_union;
            g_scroll_cleanup_valid = 1;
        }
    } else {
        g_scroll_union_valid = 0;
    }

    if (g_split_page_pending && !g_eink_touch_down) {
        uint32_t split_wave = gw_eink_scroll_release_waveform();
        int32_t height = box.y2 - box.y1 + 1;
        int32_t band = (height + 3) / 4;
        int i;
        g_split_page_pending = 0;
        if (!split_wave) split_wave = WAVEFORM_MODE_GC16;
        if (gw_epdc_finish() < 0) { g_split_page_pending = 1; return -1; }
        for (i = 0; i < 4; i++) {
            gw_epdc_rect_t part = box;
            part.y1 = box.y1 + i * band;
            part.y2 = (i == 3) ? box.y2 : part.y1 + band - 1;
            if (part.y1 > box.y2) break;
            if (part.y2 > box.y2) part.y2 = box.y2;
            if (gw_epdc_submit(&part, split_wave, UPDATE_MODE_PARTIAL,
                               "page_band") < 0)
                return -1;
            /* Finish each band so the top of the page appears progressively. */
            if (gw_epdc_finish() < 0) return -1;
            dprintf(2, "GW_UI: page band %d %d..%d\n", i + 1,
                    part.y1, part.y2);
        }
        g_dirty_count = 0;
        g_dirty_valid = 0;
        g_scroll_union_valid = 0;
        g_eink_intent = GW_EINK_INTENT_NONE;
        g_eink_scroll_release = 0;
        g_eink_last_scroll_ms = 0;
        g_eink_scroll_preview_count = 0;
        gw_eink_clear_scroll_move();
        /* One page snap is one budget unit even though it is four panel
         * submissions.  Counting each band would fire the periodic full clean
         * after four pages; the split path previously counted nothing, which
         * disabled the budget entirely for text pages. */
        g_partial_updates++;
        if (g_periodic_full_after > 0 &&
            g_partial_updates >= (unsigned)g_periodic_full_after &&
            gw_eink_idle_ms() >= 600 && !g_eink_touch_down) {
            gw_epdc_request_full_waveform_reason(WAVEFORM_MODE_GC16, "budget");
        }
        return 0;
    }
    if (g_force_full_update) {
        gw_epdc_rect_t whole = { 0, 0, GW_FB_WIDTH - 1, GW_FB_HEIGHT - 1 };
        waveform = g_force_full_waveform ? g_force_full_waveform
                                         : WAVEFORM_MODE_GC16;
        g_force_full_update = 0;
        g_force_full_waveform = 0;
        dprintf(2, "GW_UI: full update reason=%s\n", g_epdc_full_reason);
        if (gw_epdc_finish() < 0) {
            /* Keep the dirty set so the next flush can retry. */
            g_force_full_update = 1;
            g_epdc_full_reason = "retry";
            dprintf(2, "GW_UI: full update finish failed; dirty kept\n");
            return -1;
        }
        g_partial_updates = 0;
        g_eink_intent = GW_EINK_INTENT_DEEP_CLEAN;
        {
            int rc = gw_epdc_submit(&whole, waveform, UPDATE_MODE_FULL,
                                    g_epdc_full_reason);
            g_eink_intent = GW_EINK_INTENT_NONE;
            g_eink_scroll_release = 0;
            if (rc == 0) {
                g_dirty_count = 0;
                g_dirty_valid = 0;
                g_scroll_union_valid = 0;
            } else {
                dprintf(2, "GW_UI: full update submit failed; dirty kept\n");
            }
            return rc;
        }
    }

    if (g_eink_intent == GW_EINK_INTENT_SCROLL) {
        if (g_eink_touch_down && !g_eink_scroll_release)
            waveform = g_eink_scroll_waveform ? g_eink_scroll_waveform
                                              : g_eink_scene_waveform;
        else
            waveform = g_eink_scroll_release_waveform
                           ? g_eink_scroll_release_waveform
                           : WAVEFORM_MODE_REAGL;
    } else {
        waveform = g_eink_scene_waveform;
    }
    if (g_update_pending && g_last_update_full) {
        if (gw_epdc_finish() < 0) return -1;
    }
    g_partial_updates++;
    {
        int scroll_drag = scroll_intent && g_eink_touch_down &&
                          !g_eink_scroll_release;
        const char *reason = scroll_drag ? "scroll_preview"
                             : (scroll_intent ? "scroll_release" : "intent");
        int rc = 0;
        if (!scroll_intent && count > 1) {
            /* Distant dirty rectangles are submitted separately; merging them
             * into one bounding box would repaint the untouched gap between
             * e.g. a toolbar label and the keyboard. */
            int k;
            for (k = 0; k < count; k++) {
                rc = gw_epdc_submit(&g_dirty_rects[k], waveform,
                                    UPDATE_MODE_PARTIAL, reason);
                if (rc < 0) break;
            }
        } else {
            rc = gw_epdc_submit(&box, waveform, UPDATE_MODE_PARTIAL, reason);
        }
        if (rc < 0) {
            dprintf(2, "GW_UI: partial update submit failed; dirty kept\n");
            return rc;
        }
        g_dirty_count = 0;
        g_dirty_valid = 0;
        if (scroll_drag) {
            g_eink_last_scroll_ms = gw_eink_now_ms();
            g_eink_scroll_preview_count++;
            /* Keep the intent so the next dragged frame also previews. */
            g_eink_intent = GW_EINK_INTENT_SCROLL;
        } else {
            g_eink_intent = GW_EINK_INTENT_NONE;
            g_eink_scroll_release = 0;
            g_eink_last_scroll_ms = 0;
            g_eink_scroll_preview_count = 0;
            g_scroll_union_valid = 0;
            gw_eink_clear_scroll_move();
            if (scroll_intent)
                gw_epdc_schedule_scroll_cleanup();
        }
    }
    /* Fallback ghost cleanup, only when the UI is idle.  Lock/unlock remains
     * the primary cleanup point (U3). */
    if (g_periodic_full_after > 0 &&
        g_partial_updates >= (unsigned)g_periodic_full_after &&
        gw_eink_idle_ms() >= 600 && !g_eink_touch_down) {
        gw_epdc_request_full_waveform_reason(WAVEFORM_MODE_GC16, "budget");
    }
    (void)total;
    return 0;
}

#else
static int gw_epdc_finish(void) { return 0; }
static int gw_epdc_present(void) { return 0; }
static inline void gw_eink_set_scroll_clean_delay(uint32_t ms) { (void)ms; }
static inline void gw_eink_set_scroll_clean_min(uint32_t ms) { (void)ms; }
static inline void gw_epdc_cancel_scroll_cleanup(void) { }
static inline void gw_epdc_set_scroll_cleanup_area(int area_only) { (void)area_only; }
static inline void gw_epdc_set_periodic_full_after(int updates) { (void)updates; }
static inline void gw_epdc_set_split_page(int split) { (void)split; }
#endif

/* ── Flush callback ──────────────────────────────────────────────────────── */

#if GW_UI_HOST_SIM

/* 8-bit grayscale host framebuffer, same geometry as the PW3 panel. */
GW_EPDC_VAR(uint8_t, g_host_fb[GW_FB_WIDTH * GW_FB_HEIGHT], );

static void lv_port_epdc_flush(lv_disp_drv_t *drv, const lv_area_t *area,
                               lv_color_t *color_p) {
    int32_t y;
    for (y = area->y1; y <= area->y2; y++) {
        uint8_t *row = g_host_fb + (size_t)y * GW_FB_WIDTH + area->x1;
        int32_t x;
        for (x = area->x1; x <= area->x2; x++) {
            uint8_t px = lv_color_to8(*color_p++);
            unsigned r = ((px >> 5) & 7) * 255 / 7;
            unsigned g = ((px >> 2) & 7) * 255 / 7;
            unsigned b = (px & 3) * 255 / 3;
            *row++ = (uint8_t)((299 * r + 587 * g + 114 * b + 500) / 1000);
        }
    }
    lv_disp_flush_ready(drv);
}

static int lv_port_epdc_init(void) {
    lv_disp_draw_buf_init(&g_draw_buf, g_buf1, NULL, GW_FB_WIDTH * 32);
    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res  = GW_FB_WIDTH;
    g_disp_drv.ver_res  = GW_FB_HEIGHT;
    g_disp_drv.flush_cb = lv_port_epdc_flush;
    g_disp_drv.draw_buf = &g_draw_buf;
    g_disp_drv.full_refresh = 0;
    lv_disp_drv_register(&g_disp_drv);
    dprintf(2, "GW_UI: host preview display %dx%d (no /dev/fb0)\n",
            GW_FB_WIDTH, GW_FB_HEIGHT);
    return 0;
}

/* Write the current host framebuffer as an 8-bit P5 PPM. */
static int gw_host_fb_save_ppm(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P5\n%d %d\n255\n", GW_FB_WIDTH, GW_FB_HEIGHT);
    if (fwrite(g_host_fb, 1, sizeof(g_host_fb), f) != sizeof(g_host_fb)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

#else /* !GW_UI_HOST_SIM */

static void lv_port_epdc_flush(lv_disp_drv_t *drv, const lv_area_t *area,
                               lv_color_t *color_p) {
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
    if (g_epdc_smoke) {
        lv_disp_flush_ready(drv);
        return;
    }
    if (area->x1 < 0 || area->y1 < 0 || area->x2 >= GW_FB_WIDTH ||
        area->y2 >= GW_FB_HEIGHT || area->x2 < area->x1 || area->y2 < area->y1) {
        dprintf(2, "GW_UI: invalid flush area\n");
        lv_disp_flush_ready(drv);
        return;
    }
    /* Write pixels to mmap'd framebuffer and remember the dirty rectangle.
     * The actual EPDC update is sent once per render cycle by
     * gw_epdc_present(), after LVGL has finished all flush calls. */
    int32_t y;
#if GW_UI_PERF
    long long t0 = gw_now_us(), t1;
#else
    (void)0;
#endif
    gw_init_gray_lut();
    for (y = area->y1; y <= area->y2; y++) {
        uint8_t *row = (uint8_t *)g_fb_mem + (size_t)y * GW_FB_PITCH + area->x1;
        int32_t x;
        for (x = area->x1; x <= area->x2; x++) {
            /* LVGL 8-bit colors are RGB332; the PW3 framebuffer is Y8. */
            *row++ = gw_rgb332_to_gray_lut(lv_color_to8(*color_p++));
        }
    }
#if GW_UI_PERF
    t1 = gw_now_us();
#endif
#if GW_UI_PERF
    {
        unsigned long long px = (unsigned long long)(area->x2 - area->x1 + 1) *
                                (unsigned long long)(area->y2 - area->y1 + 1);
        GW_PERF_LOG("GW_UI: flush copy x=%d y=%d w=%d h=%d px=%llu copy_us=%lld\n",
                    area->x1, area->y1, area->x2 - area->x1 + 1,
                    area->y2 - area->y1 + 1, px, t1 - t0);
    }
#endif
    gw_epdc_add_dirty(area->x1, area->y1, area->x2, area->y2);

#else
    /* Stub: record the dirty rect without touching hardware. */
    GW_PERF_LOG("GW_UI: flush stub [%d,%d-%d,%d] (EPDC not accepted)\n",
                area->x1, area->y1, area->x2, area->y2);
    (void)area;
    (void)color_p;
#endif
    lv_disp_flush_ready(drv);
}

/* ── Init ────────────────────────────────────────────────────────────────── */

static int lv_port_epdc_init(void) {
    const char *smoke = getenv("GW_UI_SMOKE_FB");
    gw_init_gray_lut();
    g_epdc_smoke = smoke && smoke[0] == '1';
    if (!g_epdc_smoke) {
        g_fb_fd = open(GW_FB_DEV, O_RDWR | O_CLOEXEC);
        if (g_fb_fd < 0) {
            dprintf(2, "GW_UI: open %s failed\n", GW_FB_DEV);
            return -1;
        }
    }
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
    if (g_epdc_smoke) goto register_display;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0 ||
        vinfo.xres != GW_FB_WIDTH || vinfo.yres != GW_FB_HEIGHT ||
        vinfo.xoffset != 0 || vinfo.yoffset != 0 ||
        vinfo.bits_per_pixel != GW_FB_BPP || finfo.line_length != GW_FB_PITCH ||
        finfo.smem_len < (size_t)GW_FB_PITCH * GW_FB_HEIGHT) {
        dprintf(2, "GW_UI: unsupported framebuffer geometry\n");
        close(g_fb_fd); g_fb_fd = -1;
        return -1;
    }
    g_fb_mem = mmap(NULL,
                    (size_t)GW_FB_PITCH * GW_FB_HEIGHT,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED, g_fb_fd, 0);
    if (g_fb_mem == MAP_FAILED) {
        dprintf(2, "GW_UI: mmap fb0 failed\n");
        close(g_fb_fd); g_fb_fd = -1;
        return -1;
    }
register_display:
#endif
    lv_disp_draw_buf_init(&g_draw_buf, g_buf1, NULL,
                          GW_FB_WIDTH * 32);
    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res      = GW_FB_WIDTH;
    g_disp_drv.ver_res      = GW_FB_HEIGHT;
    g_disp_drv.flush_cb     = lv_port_epdc_flush;
    g_disp_drv.draw_buf     = &g_draw_buf;
    g_disp_drv.full_refresh = 0;  /* partial by default; full on demand */
    lv_disp_drv_register(&g_disp_drv);
    dprintf(2, "GW_UI: EPDC port init %dx%d pitch=%d accepted=%d%s\n",
            GW_FB_WIDTH, GW_FB_HEIGHT, GW_FB_PITCH, GW_UI_EPDC_ACCEPTED,
            g_epdc_smoke ? " (QEMU smoke, no /dev/fb0)" : "");
    return 0;
}

#endif /* !GW_UI_HOST_SIM */

#endif /* GW_LEVEL_EPDC_H */

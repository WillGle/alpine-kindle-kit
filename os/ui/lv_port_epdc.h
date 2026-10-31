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
#ifndef GW_UI_EINK_FAST_MODE
#define GW_UI_EINK_FAST_MODE 0
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
#ifndef GW_UI_EPDC_SPIKE
#define GW_UI_EPDC_SPIKE 0
#endif

/* PW3 panel geometry — verified in dry-run evidence. */
#define GW_FB_WIDTH   1072
#define GW_FB_HEIGHT  1448
#define GW_FB_PITCH   1088   /* bytes per row including padding */
#define GW_FB_BPP        8   /* Y8 greyscale */
#define GW_FB_DEV     "/dev/fb0"

/* EPDC update mode constants from i.MX6 vendor kernel. */
#define WAVEFORM_MODE_DU     1
#define WAVEFORM_MODE_GC16   2
#define WAVEFORM_MODE_A2     4
#define WAVEFORM_MODE_DU4    7
#define WAVEFORM_MODE_REAGL  8
#define WAVEFORM_MODE_AUTO   257
#define UPDATE_MODE_PARTIAL    0
#define UPDATE_MODE_FULL       1

/* EPDC flags used by the PW3 kernel (lab126 additions). */
#define EPDC_FLAG_FORCE_MONOCHROME 2

/* Full GC16 cleanup after this many partial updates. REAGL is the same
 * partial waveform KOReader uses for page turns; the periodic full update
 * clears any residue. */
#define GW_EPDC_PARTIAL_LIMIT 20

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
static uint8_t g_gray_lut[256];
static int g_gray_lut_ready = 0;
static inline void gw_init_gray_lut(void) {
    unsigned i;
    if (g_gray_lut_ready) return;
    for (i = 0; i < 256; i++) {
        unsigned r = ((i >> 5) & 7) * 255 / 7;
        unsigned g = ((i >> 2) & 7) * 255 / 7;
        unsigned b = (i & 3) * 255 / 3;
        g_gray_lut[i] = (uint8_t)((299 * r + 587 * g + 114 * b + 500) / 1000);
    }
    g_gray_lut_ready = 1;
}
static inline uint8_t gw_rgb332_to_gray_lut(uint8_t pixel) {
    if (!g_gray_lut_ready) gw_init_gray_lut();
    return g_gray_lut[pixel];
}

/* Port state. */
static int   g_fb_fd  = -1;
static void *g_fb_mem = MAP_FAILED;
/* QEMU smoke mode: build and render into LVGL buffers only, never touch
 * /dev/fb0.  Enabled explicitly with GW_UI_SMOKE_FB=1. */
static int   g_epdc_smoke = 0;

/* Monotonic microsecond timestamp of the most recent completed touch frame.
 * Used to correlate input, render and EPDC submit latency. */
static long long g_last_input_us = 0;
static lv_disp_draw_buf_t g_draw_buf;
/* A small draw buffer keeps RAM use low on the PW3; dirty rectangles are
 * coalesced into one EPDC update per cycle, so a full-screen buffer is not
 * needed. */
static lv_color_t         g_buf1[GW_FB_WIDTH * 32];
static lv_disp_drv_t      g_disp_drv;

/* Fast-mode state machine (Phase 3), always available so the input port can
 * record activity even when EPDC is not accepted. */
typedef enum { GW_EINK_STATE_IDLE = 0, GW_EINK_STATE_INTERACTIVE = 1,
               GW_EINK_STATE_SETTLE = 2 } gw_eink_state_t;
static gw_eink_state_t g_eink_state = GW_EINK_STATE_IDLE;
static uint32_t g_last_input_ms = 0;
static int g_settle_refresh_pending = 0;
static int g_force_full_update = 0;
static int g_eink_text_input = 0;
static int g_eink_du_used = 0;
/* Only large fast updates (screen transitions) need a full cleanup pass;
 * small button feedback must not trigger a screen-wide repaint. */
static int g_eink_du_large = 0;
static uint32_t g_eink_fast_until_ms = 0;
static uint32_t g_eink_transition_until_ms = 0;
static uint32_t g_force_full_waveform = 0;
/* Screen-space bounds of the visible virtual keyboard, if any. */
static int g_eink_kbd_valid = 0;
static int32_t g_eink_kbd_x1 = 0, g_eink_kbd_y1 = 0;
static int32_t g_eink_kbd_x2 = -1, g_eink_kbd_y2 = -1;
/* Screen-space bounds of an open dropdown/menu popup. */
static uint32_t g_eink_no_full_until_ms = 0;
static int g_eink_popup_valid = 0;
static int32_t g_eink_popup_x1 = 0, g_eink_popup_y1 = 0;
static int32_t g_eink_popup_x2 = -1, g_eink_popup_y2 = -1;
static inline uint32_t gw_eink_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
static inline void gw_eink_set_text_input(int active) {
    g_eink_text_input = active ? 1 : 0;
}
static inline void gw_eink_set_keyboard_rect(int valid, int32_t x1, int32_t y1,
                                             int32_t x2, int32_t y2) {
    g_eink_kbd_valid = valid ? 1 : 0;
    g_eink_kbd_x1 = x1;
    g_eink_kbd_y1 = y1;
    g_eink_kbd_x2 = x2;
    g_eink_kbd_y2 = y2;
}
static inline void gw_eink_hold_partial(uint32_t ms) {
    g_eink_no_full_until_ms = gw_eink_now_ms() + ms;
}
/* Use the fast monochrome DU waveform during an interaction burst.  The
 * caller schedules one GC16 cleanup after input settles. */
static inline void gw_eink_request_fast(uint32_t ms) {
    g_eink_fast_until_ms = gw_eink_now_ms() + ms;
}
/* A screen transition gets the 4-level fast waveform (DU4): visibly smoother
 * than 1-bit DU but still far quicker than a flashing GC16. */
static inline void gw_eink_request_transition(uint32_t ms) {
    g_eink_transition_until_ms = gw_eink_now_ms() + ms;
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
    g_eink_fast_until_ms = g_last_input_ms + 700;
    /* New typing cancels a queued grayscale cleanup; it will be re-armed
     * after this interaction settles. */
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

#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
static uint32_t g_update_marker;
static int g_epdc_error;
static int g_update_pending;
static int g_last_update_full;
static int g_dirty_valid;

typedef struct {
    int32_t x1, y1, x2, y2;
} gw_epdc_rect_t;

#define GW_EPDC_MAX_DIRTY 8
static gw_epdc_rect_t g_dirty_rects[GW_EPDC_MAX_DIRTY];
static int g_dirty_count;
static unsigned g_partial_updates;
static unsigned g_update_count;
static uint32_t g_epdc_last_error_log_ms;
/* Localized high-quality cleanup for the text field after typing pauses. */
static gw_epdc_rect_t g_text_cleanup_rect;
static int g_text_cleanup_pending;
static uint32_t g_text_cleanup_deadline_ms;
static int g_text_cleanup_force;

static unsigned long long gw_epdc_rect_area(const gw_epdc_rect_t *r) {
    return (unsigned long long)(r->x2 - r->x1 + 1) *
           (unsigned long long)(r->y2 - r->y1 + 1);
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

/* Keep distant dirty regions separate.  Merging the password field and the
 * keyboard into one bounding box makes LVGL's 100 px key update + the text
 * field look like a 1000 px region, which the panel promotes to a full GC16
 * flash.  Nearby regions (a key border and its glyph, thumbnails, etc.) are
 * still merged. */
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
        /* Bound the list: merge into the rectangle with the smallest area
         * increase rather than dropping a change. */
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

/* Queue one full-screen GC16 update using the already-correct framebuffer.
 * Used to clear residue after a fast interactive burst without repainting the
 * widget tree. */
static void gw_epdc_request_full_waveform(uint32_t waveform) {
    g_dirty_rects[0].x1 = 0;
    g_dirty_rects[0].y1 = 0;
    g_dirty_rects[0].x2 = GW_FB_WIDTH - 1;
    g_dirty_rects[0].y2 = GW_FB_HEIGHT - 1;
    g_dirty_count = 1;
    g_dirty_valid = 1;
    g_force_full_update = 1;
    g_force_full_waveform = waveform;
    g_eink_du_used = 0;
    g_eink_du_large = 0;
}

static void gw_epdc_request_full(void) {
    gw_epdc_request_full_waveform(WAVEFORM_MODE_GC16);
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

static int gw_epdc_submit(const gw_epdc_rect_t *r, uint32_t waveform,
                          uint32_t mode, int fast) {
    struct gw_mxcfb_update_data upd;
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
    upd.hist_bw_waveform_mode   = waveform == WAVEFORM_MODE_REAGL ?
                                  WAVEFORM_MODE_REAGL : WAVEFORM_MODE_DU;
    upd.hist_gray_waveform_mode = waveform == WAVEFORM_MODE_REAGL ?
                                  WAVEFORM_MODE_REAGL : WAVEFORM_MODE_GC16;
    upd.flags = waveform == WAVEFORM_MODE_DU ? EPDC_FLAG_FORCE_MONOCHROME : 0;
    if (++g_update_marker == 0) ++g_update_marker;
    upd.update_marker = g_update_marker;
    upd.temp          = 0x1000; /* TEMP_USE_AMBIENT on the PW3 kernel */

    if (ioctl(g_fb_fd, MXCFB_SEND_UPDATE, &upd) < 0) {
        uint32_t now = gw_eink_now_ms();
        if (g_epdc_last_error_log_ms == 0 ||
            now - g_epdc_last_error_log_ms >= 5000) {
            dprintf(2, "GW_UI: EPDC update failed\n");
            g_epdc_last_error_log_ms = now;
        }
        g_epdc_error = 1;
        return -1;
    }
    g_epdc_error = 0;
    g_update_pending = 1;
    g_last_update_full = mode == UPDATE_MODE_FULL;
    g_update_count++;
#if GW_UI_PERF
    t1 = gw_now_us();
    GW_PERF_LOG("GW_UI: epdc submit marker=%u waveform=%u mode=%u area=%llu fast=%d partials=%u rect=%d,%d-%d,%d kbd=%d,%d-%d,%d text=%d submit_us=%lld count=%u input_to_submit_us=%lld\n",
                g_update_marker, waveform, mode, area, fast,
                g_partial_updates, r->x1, r->y1, r->x2, r->y2,
                g_eink_kbd_x1, g_eink_kbd_y1, g_eink_kbd_x2, g_eink_kbd_y2,
                g_eink_text_input, t1 - t0, g_update_count,
                g_last_input_us ? (t1 - g_last_input_us) : -1);
#else
    (void)area;
    (void)fast;
#endif
    return 0;
}

/* Submit the accumulated dirty regions.  Distant regions stay separate:
 * a keyboard key uses A2 (fast monochrome, as KOReader does for highlights)
 * and the text field uses DU during active typing.  Large or overlapping
 * repaints still coalesce into one GC16 full update. */
static int gw_epdc_present(void) {
    gw_epdc_rect_t rects[GW_EPDC_MAX_DIRTY];
    if (g_epdc_smoke) return 0;
    int count, i, count_large = 0;
    unsigned long long total = 0;
    const unsigned long long screen =
        (unsigned long long)GW_FB_WIDTH * GW_FB_HEIGHT;
    int full;
    int fast_text = GW_UI_EINK_FAST_MODE && g_eink_text_input &&
                    g_eink_state == GW_EINK_STATE_INTERACTIVE;
    int fast_nav = GW_UI_EINK_FAST_MODE && g_eink_fast_until_ms != 0 &&
                   (int32_t)(gw_eink_now_ms() - g_eink_fast_until_ms) < 0;
    int transition = GW_UI_EINK_FAST_MODE && g_eink_transition_until_ms != 0 &&
                     (int32_t)(gw_eink_now_ms() - g_eink_transition_until_ms) < 0;

    if (!g_dirty_valid || g_dirty_count == 0)
        return 0;

    count = g_dirty_count;
    memcpy(rects, g_dirty_rects, (size_t)count * sizeof(rects[0]));
    g_dirty_count = 0;
    g_dirty_valid = 0;

    for (i = 0; i < count; i++) total += gw_epdc_rect_area(&rects[i]);
    /* Small section updates (clock, battery, one row) must not accumulate
     * into a screen-wide GC16 flash.  Only medium/large regions count toward
     * the partial-refresh budget; nearby small rects stay isolated. */
    {
        int large_count = 0;
        for (i = 0; i < count; i++)
            if (gw_epdc_rect_area(&rects[i]) * 20 >= screen) large_count++;
        count_large = large_count;
    }
    /* While a menu popup is open, never promote the popup + keyboard-dismiss
     * frame to a full-screen GC16.  It is a transient UI state and the DU
     * partial path is fast; one GC16 cleanup follows on close. */
    {
        int no_full = g_eink_no_full_until_ms != 0 &&
                      (int32_t)(gw_eink_now_ms() - g_eink_no_full_until_ms) < 0;
        if (!no_full) g_eink_no_full_until_ms = 0;
        full = g_force_full_update ||
               (!no_full && !g_eink_text_input && !g_eink_popup_valid &&
                total * 2 >= screen) ||
               (!no_full && !g_eink_text_input && !g_eink_popup_valid &&
                g_partial_updates + (unsigned)count_large >= GW_EPDC_PARTIAL_LIMIT);
    }
    g_force_full_update = 0;

    if (full) {
        gw_epdc_rect_t whole = { 0, 0, GW_FB_WIDTH - 1, GW_FB_HEIGHT - 1 };
        uint32_t waveform = g_force_full_waveform ? g_force_full_waveform
                                                  : WAVEFORM_MODE_GC16;
        g_force_full_waveform = 0;
        if (gw_epdc_finish() < 0) return -1;
        g_partial_updates = 0;
        return gw_epdc_submit(&whole, waveform, UPDATE_MODE_FULL, 0);
    }

    /* A scene transition is one intent: submit one whole-screen DU4 instead
     * of several rectangles.  Multiple sequential partial submits look like
     * flicker/tearing even when each one is fast. */
    if (transition) {
        gw_epdc_rect_t whole = { 0, 0, GW_FB_WIDTH - 1, GW_FB_HEIGHT - 1 };
        if (gw_epdc_finish() < 0) return -1;
        g_eink_du_large = 1;
        g_partial_updates++;
        return gw_epdc_submit(&whole, WAVEFORM_MODE_DU4, UPDATE_MODE_PARTIAL, 1);
    }

    /* Two passes: key A2 highlights and popup DU regions first, then the
     * text field.  Each key rectangle is submitted on its own, so a press on
     * one row cannot merge with a release on another row into a large
     * keyboard-sized flash.  Typing frames are never promoted to full GC16. */
    {
        int pass;
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; i < count; i++) {
                gw_epdc_rect_t *r = &rects[i];
                unsigned long long area = gw_epdc_rect_area(r);
                int in_keyboard = g_eink_kbd_valid &&
                                  r->y1 >= g_eink_kbd_y1 - 8 &&
                                  r->y2 <= g_eink_kbd_y2 + 8;
                int in_popup = g_eink_popup_valid &&
                               r->x1 >= g_eink_popup_x1 && r->x2 <= g_eink_popup_x2 &&
                               r->y1 >= g_eink_popup_y1 && r->y2 <= g_eink_popup_y2;
                int key_highlight = in_keyboard && area <= 520ULL * 1024ULL;
                int priority = key_highlight || in_popup;
                uint32_t waveform;
                uint32_t mode = UPDATE_MODE_PARTIAL;
                int fast;

                if ((pass == 0) != priority) continue;

                if (key_highlight)
                    waveform = WAVEFORM_MODE_A2;
                else if (GW_UI_EINK_FAST_MODE &&
                         (in_popup || g_eink_popup_valid))
                    waveform = WAVEFORM_MODE_DU;
                else if (fast_text && area * 20 < screen)
                    waveform = WAVEFORM_MODE_DU;
                else if (transition)
                    waveform = WAVEFORM_MODE_DU4;
                else if (fast_nav && area * 4 < screen)
                    waveform = WAVEFORM_MODE_DU;
                else
                    waveform = WAVEFORM_MODE_REAGL;

                {
                    int quality = g_text_cleanup_force &&
                                  r->x1 >= g_text_cleanup_rect.x1 &&
                                  r->x2 <= g_text_cleanup_rect.x2 &&
                                  r->y1 >= g_text_cleanup_rect.y1 &&
                                  r->y2 <= g_text_cleanup_rect.y2;
                    if (quality) {
                        waveform = WAVEFORM_MODE_REAGL;
                        mode = UPDATE_MODE_FULL;
                        g_text_cleanup_force = 0;
                    } else if (pass == 1 && fast_text && !in_keyboard && !in_popup) {
                        uint32_t now = gw_eink_now_ms();
                        if (gw_eink_idle_ms() < 250) {
                            gw_epdc_add_dirty(r->x1, r->y1, r->x2, r->y2);
                            continue;
                        }
                        if (g_text_cleanup_pending) {
                            if (r->x1 < g_text_cleanup_rect.x1) g_text_cleanup_rect.x1 = r->x1;
                            if (r->y1 < g_text_cleanup_rect.y1) g_text_cleanup_rect.y1 = r->y1;
                            if (r->x2 > g_text_cleanup_rect.x2) g_text_cleanup_rect.x2 = r->x2;
                            if (r->y2 > g_text_cleanup_rect.y2) g_text_cleanup_rect.y2 = r->y2;
                        } else {
                            g_text_cleanup_rect = *r;
                            g_text_cleanup_pending = 1;
                        }
                        g_text_cleanup_deadline_ms = now + 350;
                    }
                }

                fast = waveform == WAVEFORM_MODE_DU ||
                       waveform == WAVEFORM_MODE_DU4 ||
                       waveform == WAVEFORM_MODE_A2;
                if (waveform == WAVEFORM_MODE_REAGL ||
                    (g_update_pending && g_last_update_full)) {
                    if (gw_epdc_finish() < 0) return -1;
                }
                if (fast) {
                    g_eink_du_used = 1;
                    if (transition || area * 4 >= screen) g_eink_du_large = 1;
                    if (area * 20 >= screen) g_partial_updates++;
                }
                if (waveform == WAVEFORM_MODE_REAGL &&
                    area * 20 >= screen) g_partial_updates++;
                if (gw_epdc_submit(r, waveform, mode, fast) < 0)
                    return -1;
            }
        }
    }
    g_text_cleanup_force = 0;
    return 0;
}

#else
static int gw_epdc_finish(void) { return 0; }
static int gw_epdc_present(void) { return 0; }
#endif

/* ── Flush callback ──────────────────────────────────────────────────────── */

#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
static uint8_t gw_rgb332_to_gray(uint8_t pixel) {
    unsigned r = ((pixel >> 5) & 7) * 255 / 7;
    unsigned g = ((pixel >> 2) & 7) * 255 / 7;
    unsigned b = (pixel & 3) * 255 / 3;
    return (uint8_t)((299 * r + 587 * g + 114 * b + 500) / 1000);
}
#endif

#if GW_UI_HOST_SIM

/* 8-bit grayscale host framebuffer, same geometry as the PW3 panel. */
static uint8_t g_host_fb[GW_FB_WIDTH * GW_FB_HEIGHT];

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
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
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
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
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

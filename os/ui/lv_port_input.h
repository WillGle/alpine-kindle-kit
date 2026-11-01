/* SPDX-License-Identifier: GPL-3.0-or-later
 * PW3 evdev touch input port for LVGL.
 *
 * Reads ABS_X/ABS_Y/ABS_MT_POSITION_X/Y from /dev/input/eventN for the
 * cyttsp4 touchscreen. The event node is found by scanning for ABS_MT_POSITION_X
 * capability, matching the touch module loading in gleworks-display.sh.
 *
 * CURRENT STATUS (2026-10-30): stub. Event node scanning is implemented;
 * LVGL indev callbacks are registered. Touch data is logged but not forwarded
 * to LVGL until GW_UI_TOUCH_ACCEPTED=1.
 *
 * STATUS: SPIKE (accepted=0).
 *
 * Touch and display acceptance are separate — either can be tested first.
 */
#ifndef GW_LV_PORT_INPUT_H
#define GW_LV_PORT_INPUT_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <fcntl.h>
#include <linux/input.h>
#include <lvgl/lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "lv_port_epdc.h"

#ifndef GW_UI_TOUCH_ACCEPTED
#define GW_UI_TOUCH_ACCEPTED 0
#endif
#ifndef GW_UI_TOUCH_SPIKE
#define GW_UI_TOUCH_SPIKE 0
#endif
/* Test-only: build with -DGW_UI_TOUCH_DEV_OVERRIDE=1 to point the port at a
 * uinput-created event node (used by tools/maintenance/pw3-uinput-touch.py). */
#ifndef GW_UI_TOUCH_DEV_OVERRIDE
#define GW_UI_TOUCH_DEV_OVERRIDE 0
#endif

#define GW_INPUT_MAX_NODES 8

static int              g_touch_fd = -1;
static lv_indev_drv_t   g_indev_drv;
static lv_indev_t      *g_indev    = NULL;

/* Last known touch state. */
static int g_touch_x   = 0;
static int g_touch_y   = 0;
static int g_touching  = 0;   /* 1 = finger down */
static int g_touch_press_x = 0;
static int g_touch_press_y = 0;
static int g_touch_scroll_seen = 0;

/* Raw touch ranges from EVIOCGABS.  The panel is not necessarily 1:1 with the
 * digitizer; scaling keeps taps on the control the user actually touched. */
static int g_touch_min_x = 0, g_touch_max_x = GW_FB_WIDTH - 1;
static int g_touch_min_y = 0, g_touch_max_y = GW_FB_HEIGHT - 1;

static void lv_port_input_query_range(int fd) {
    struct input_absinfo info;
    if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &info) == 0 &&
        info.maximum > info.minimum) {
        g_touch_min_x = info.minimum;
        g_touch_max_x = info.maximum;
    } else if (ioctl(fd, EVIOCGABS(ABS_X), &info) == 0 &&
               info.maximum > info.minimum) {
        g_touch_min_x = info.minimum;
        g_touch_max_x = info.maximum;
    }
    if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &info) == 0 &&
        info.maximum > info.minimum) {
        g_touch_min_y = info.minimum;
        g_touch_max_y = info.maximum;
    } else if (ioctl(fd, EVIOCGABS(ABS_Y), &info) == 0 &&
               info.maximum > info.minimum) {
        g_touch_min_y = info.minimum;
        g_touch_max_y = info.maximum;
    }
    dprintf(2, "GW_UI: touch range x=%d..%d y=%d..%d\n",
            g_touch_min_x, g_touch_max_x, g_touch_min_y, g_touch_max_y);
}

static lv_coord_t lv_port_input_scale(int value, int minimum, int maximum,
                                      int target_max) {
    long long scaled;
    if (maximum <= minimum || target_max <= 0) return (lv_coord_t)value;
    scaled = ((long long)value - minimum) * target_max / (maximum - minimum);
    if (scaled < 0) scaled = 0;
    if (scaled > target_max) scaled = target_max;
    return (lv_coord_t)scaled;
}

/* g_last_input_us is defined in lv_port_epdc.h and updated here. */

/* Find cyttsp4 touch node: looks for ABS_MT_POSITION_X capability. */
static int lv_port_input_open(void) {
    char path[48]; int n;
#if GW_UI_TOUCH_DEV_OVERRIDE
    /* Test-only: point the port at a uinput-created event node. */
    {
        const char *override = getenv("GW_UI_TOUCH_DEV");
        if (override != NULL && override[0] != '\0') {
            int fd = open(override, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd >= 0) {
                dprintf(2, "GW_UI: touch override %s\n", override);
                lv_port_input_query_range(fd);
                return fd;
            }
        }
    }
#endif
    for (n = 0; n < GW_INPUT_MAX_NODES; n++) {
        unsigned long absbits[ABS_MAX / (8*sizeof(long)) + 1] = {0};
        unsigned long evbits = 0;
        snprintf(path, sizeof(path), "/dev/input/event%d", n);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) >= 0 &&
            (evbits & (1UL << EV_ABS)) &&
            ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) >= 0 &&
            (absbits[ABS_MT_POSITION_X / (8*sizeof(long))] &
             (1UL << (ABS_MT_POSITION_X % (8*sizeof(long)))))) {
#if GW_UI_TOUCH_ACCEPTED || GW_UI_TOUCH_SPIKE
            if (ioctl(fd, EVIOCGRAB, 1) < 0) {
                /* Grab may fail if another process holds the device; fall back
                 * to shared (non-exclusive) reads so the shell still runs. */
                dprintf(2, "GW_UI: touch grab failed, using shared access\n");
            }
#endif
            dprintf(2, "GW_UI: touch node %s\n", path);
            lv_port_input_query_range(fd);
            return fd;
        }
        close(fd);
    }
    return -1;
}

/* LVGL read callback — called from lv_task_handler(). */
static void lv_port_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
#if GW_UI_TOUCH_ACCEPTED || GW_UI_TOUCH_SPIKE
    data->continue_reading = false;
    if (g_touch_fd < 0) { data->state = LV_INDEV_STATE_REL; return; }
    struct input_event ev;
    int saw_event = 0;
    while (read(g_touch_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        saw_event = 1;
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) g_touch_x = ev.value;
            if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) g_touch_y = ev.value;
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            g_touching = ev.value;
            if (g_touching) {
                g_touch_press_x = g_touch_x;
                g_touch_press_y = g_touch_y;
                g_touch_scroll_seen = 0;
            }
#if defined(GW_LEVEL_EPDC_H)
            gw_eink_set_touch_down(g_touching ? 1 : 0);
#endif
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            /* A drag is recognised from the raw evdev stream, before LVGL's
             * scroll event fires.  Scrolling is on-release: the EPDC port
             * holds the dirty list until the finger lifts. */
            if (g_touching && !g_touch_scroll_seen) {
                int dx = g_touch_x - g_touch_press_x;
                int dy = g_touch_y - g_touch_press_y;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx > 8 || dy > 8) {
                    g_touch_scroll_seen = 1;
#if defined(GW_LEVEL_EPDC_H)
                    gw_eink_note_scroll_move();
#endif
                }
            }
            data->continue_reading = g_touch_scroll_seen ? true : false;
#if defined(GW_LEVEL_EPDC_H)
            gw_eink_note_input();
#endif
            g_last_input_us = gw_now_us();
            GW_PERF_LOG("GW_UI: input event x=%d y=%d state=%d scroll=%d t=%lld us\n",
                        g_touch_x, g_touch_y, g_touching, g_touch_scroll_seen,
                        g_last_input_us);
            if (!g_touch_scroll_seen) break;
        }
    }
    (void)saw_event;
    if (!g_touching) {
#if defined(GW_LEVEL_EPDC_H)
        gw_eink_set_touch_down(0);
#endif
    }
    data->point.x = lv_port_input_scale(g_touch_x, g_touch_min_x,
                                       g_touch_max_x, GW_FB_WIDTH - 1);
    data->point.y = lv_port_input_scale(g_touch_y, g_touch_min_y,
                                       g_touch_max_y, GW_FB_HEIGHT - 1);
    data->state   = g_touching ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
#else
    /* Stub: no touch input forwarded until acceptance. */
    data->state = LV_INDEV_STATE_REL;
    (void)g_touch_x; (void)g_touch_y; (void)g_touching;
#endif
}

static int lv_port_input_init(void) {
#if GW_UI_HOST_SIM
    dprintf(2, "GW_UI: host preview input (no evdev)\n");
    return 0;
#else
    {
        const char *smoke = getenv("GW_UI_SMOKE_FB");
        if (smoke && smoke[0] == '1') {
            dprintf(2, "GW_UI: QEMU smoke input (no evdev)\n");
            return 0;
        }
    }
    g_touch_fd = lv_port_input_open();
    if (g_touch_fd < 0) {
        dprintf(2, "GW_UI: touch node not found (touch accepted=%d)\n",
                GW_UI_TOUCH_ACCEPTED);
        return -1;
    }

    lv_indev_drv_init(&g_indev_drv);
    g_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    g_indev_drv.read_cb = lv_port_input_read;
    /* Frontlight +/- repeat every 300 ms while held. */
    g_indev_drv.long_press_time = 400;
    g_indev_drv.long_press_repeat_time = 300;
    g_indev = lv_indev_drv_register(&g_indev_drv);
    return 0;
#endif /* GW_UI_HOST_SIM */
}

#endif /* GW_LV_PORT_INPUT_H */

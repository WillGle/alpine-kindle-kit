/* SPDX-License-Identifier: GPL-3.0-or-later
 * Kindle Hub screen: a full-panel frame from gw-agent plus one exit target.
 * All navigation inside the frame is remote; the shell only forwards touch.
 */
#include "gw_shell_internal.h"

static int g_hub_last_x = -1;
static int g_hub_last_y = -1;

static void hub_send_point(const char *action) {
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;
    int x, y;

    if (!indev) return;
    lv_indev_get_point(indev, &point);
    x = point.x * 65535 / (GW_UI_PANEL_W - 1);
    y = point.y * 65535 / (GW_UI_PANEL_H - 1);
    if (x < 0) x = 0;
    if (x > 65535) x = 65535;
    if (y < 0) y = 0;
    if (y > 65535) y = 65535;
    gw_hub_send_touch(action, x, y);
}

GW_SHELL_INTERNAL void hub_touch_cb(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;

    if (code == LV_EVENT_PRESSED) {
        g_hub_last_x = g_hub_last_y = -1;
        hub_send_point("down");
    } else if (code == LV_EVENT_PRESSING) {
        if (!indev) return;
        lv_indev_get_point(indev, &point);
        if (g_hub_last_x >= 0 &&
            abs(point.x - g_hub_last_x) < 8 &&
            abs(point.y - g_hub_last_y) < 8) {
            return;
        }
        g_hub_last_x = point.x;
        g_hub_last_y = point.y;
        hub_send_point("move");
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        hub_send_point("up");
    }
}

GW_SHELL_INTERNAL void hub_exit_cb(lv_event_t *event) {
    (void)event;
    gw_hub_set_view(0);
    gw_back_cb(event);
}

GW_SHELL_INTERNAL void hub_open_cb(lv_event_t *event) {
    (void)event;
    gw_open_secondary(GW_SCREEN_HUB, GW_SCREEN_SETTINGS);
}

GW_SHELL_INTERNAL void build_hub_screen(void) {
    g_hub_screen = make_screen();
    g_hub_img = lv_img_create(g_hub_screen);
    lv_obj_set_size(g_hub_img, GW_UI_PANEL_W, GW_UI_PANEL_H);
    lv_obj_set_pos(g_hub_img, 0, 0);
    lv_obj_add_flag(g_hub_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_hub_img, hub_touch_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_hub_img, hub_touch_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(g_hub_img, hub_touch_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_hub_img, hub_touch_cb, LV_EVENT_PRESS_LOST, NULL);

    g_hub_status = gw_ui_label(g_hub_screen, GW_STR_HUB_WAITING,
                               &gw_font_28, gw_ui_c_secondary());
    lv_obj_align(g_hub_status, LV_ALIGN_CENTER, 0, 0);

    g_hub_exit = make_button(g_hub_screen, GW_STR_EXIT, 160,
                             GW_UI_TOUCH_MIN, 1);
    lv_obj_align(g_hub_exit, LV_ALIGN_TOP_RIGHT, -GW_UI_MARGIN,
                 GW_UI_MARGIN);
    lv_obj_add_event_cb(g_hub_exit, hub_exit_cb, LV_EVENT_CLICKED, NULL);

    gw_hub_attach(g_hub_img, g_hub_status);
}

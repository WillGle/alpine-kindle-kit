/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host-only LVGL preview and geometry harness for the PW3 Home UI.
 *
 * This includes the real shell source, so the screenshots and geometry checks
 * exercise the same widget tree the ARM binary will run.  The display port is
 * replaced by lv_port_epdc.h's GW_UI_HOST_SIM memory framebuffer; no /dev/fb0,
 * evdev, EPDC ioctl, gw-system socket or physical device is touched.
 *
 * Build: make -C os/ui host-sim
 * Run:   ./gw-ui-host-sim <output-directory>
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#define GW_UI_HOST_SIM 1
#include "gw_shell_test.h"

#include <sys/stat.h>

typedef struct {
    FILE *json;
    int total;
    int failed;
} geom_report_t;

static void geom_result(geom_report_t *r, const char *screen,
                        const char *test, int pass, const char *detail) {
    r->total++;
    if (!pass) r->failed++;
    if (r->json) {
        fprintf(r->json,
                "%s{\"screen\":\"%s\",\"test\":\"%s\",\"pass\":%s,\"detail\":\"%s\"}",
                r->total > 1 ? ",\n  " : "\n  ", screen, test,
                pass ? "true" : "false", detail ? detail : "");
    }
    if (!pass)
        printf("FAIL %-9s %-34s %s\n", screen, test, detail ? detail : "");
}

static const char *obj_class_name(const lv_obj_t *obj) {
    const lv_obj_class_t *c = lv_obj_get_class(obj);
    if (c == &lv_btn_class) return "btn";
    if (c == &lv_label_class) return "label";
    if (c == &lv_obj_class) return "obj";
    if (c == &lv_dropdown_class) return "dropdown";
    if (c == &lv_dropdownlist_class) return "dropdownlist";
    if (c == &lv_textarea_class) return "textarea";
    if (c == &lv_slider_class) return "slider";
    if (c == &lv_keyboard_class) return "keyboard";
    if (c == &lv_bar_class) return "bar";
    return "other";
}

static void geom_label_check(geom_report_t *r, const char *screen, lv_obj_t *obj) {
    const char *text;
    lv_coord_t cw, ch, sw, sh;
    lv_label_long_mode_t mode;
    if (!lv_obj_is_visible(obj)) return;
    if (!lv_obj_check_type(obj, &lv_label_class)) return;
    text = lv_label_get_text(obj);
    if (!text || !text[0]) return;
    cw = lv_obj_get_content_width(obj);
    ch = lv_obj_get_content_height(obj);
    sw = lv_obj_get_self_width(obj);
    sh = lv_obj_get_self_height(obj);
    mode = lv_label_get_long_mode(obj);
    if (cw <= 0 || ch <= 0) {
        char detail[160];
        snprintf(detail, sizeof(detail), "label has no content box (%d x %d)", cw, ch);
        geom_result(r, screen, "label_has_space", 0, detail);
        return;
    }
    if (mode == LV_LABEL_LONG_DOT || mode == LV_LABEL_LONG_CLIP ||
        mode == LV_LABEL_LONG_SCROLL || mode == LV_LABEL_LONG_SCROLL_CIRCULAR) {
        int pass = ch + 2 >= lv_font_get_line_height(
                       lv_obj_get_style_text_font(obj, LV_PART_MAIN));
        char detail[160];
        snprintf(detail, sizeof(detail), "mode=%d height=%d line=%d",
                 (int)mode, ch,
                 lv_font_get_line_height(lv_obj_get_style_text_font(obj, LV_PART_MAIN)));
        geom_result(r, screen, "label_not_clipped", pass, detail);
    } else {
        int pass = sw <= cw + 2 && sh <= ch + 2;
        char detail[192];
        snprintf(detail, sizeof(detail),
                 "text=%d x %d box=%d x %d mode=%d", sw, sh, cw, ch, (int)mode);
        geom_result(r, screen, "label_not_clipped", pass, detail);
    }
}

static int inside_scrollable(lv_obj_t *obj, lv_obj_t *root) {
    lv_obj_t *parent = lv_obj_get_parent(obj);
    while (parent && parent != root) {
        if (lv_obj_has_flag(parent, LV_OBJ_FLAG_SCROLLABLE)) return 1;
        parent = lv_obj_get_parent(parent);
    }
    return 0;
}

static void geom_walk(geom_report_t *r, const char *screen, lv_obj_t *obj,
                      lv_obj_t *root) {
    uint32_t i;
    if (!obj) return;
    if (obj != root && lv_obj_is_visible(obj)) {
        lv_area_t a;
        lv_obj_get_coords(obj, &a);
        /* Scrollable parents intentionally place content outside their clip
         * box; the object coordinates are not expected to stay on-screen. */
        if (!inside_scrollable(obj, root)) {
            int pass = a.x1 >= 0 && a.y1 >= 0 &&
                       a.x2 < GW_UI_PANEL_W && a.y2 < GW_UI_PANEL_H;
            char detail[160];
            snprintf(detail, sizeof(detail), "coords=(%d,%d)-(%d,%d)",
                     a.x1, a.y1, a.x2, a.y2);
            geom_result(r, screen, "inside_screen", pass, detail);
        }
        /* Check real controls.  Plain LVGL containers are clickable by
         * default but are not touch targets. */
        if (!lv_obj_check_type(obj, &lv_dropdownlist_class) &&
            (lv_obj_check_type(obj, &lv_btn_class) ||
             lv_obj_check_type(obj, &lv_slider_class) ||
             lv_obj_check_type(obj, &lv_textarea_class) ||
             lv_obj_check_type(obj, &lv_dropdown_class) ||
             lv_obj_check_type(obj, &lv_keyboard_class))) {
            lv_coord_t w = lv_obj_get_width(obj);
            lv_coord_t h = lv_obj_get_height(obj);
            lv_coord_t pad = (obj->spec_attr != NULL)
                                 ? obj->spec_attr->ext_click_pad : 0;
            int pass = w + 2 * pad >= GW_UI_TOUCH_MIN &&
                       h + 2 * pad >= GW_UI_TOUCH_MIN;
            char detail[160];
            snprintf(detail, sizeof(detail), "touch=%d x %d class=%s", w, h,
                     obj_class_name(obj));
            geom_result(r, screen, "touch_min_96", pass, detail);
        }
        geom_label_check(r, screen, obj);
    }
    for (i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *child = lv_obj_get_child(obj, (int32_t)i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        geom_walk(r, screen, child, root);
    }
}

/* E09: a non-scrollable container must not contain a child outside its own
 * box.  Scrollable containers are allowed to clip children by design. */
static void check_overflow_rec(geom_report_t *r, const char *screen,
                               lv_obj_t *obj) {
    uint32_t i;
    lv_area_t p;
    if (!obj || !lv_obj_is_visible(obj)) return;
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE)) return;
    lv_obj_get_coords(obj, &p);
    for (i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *child = lv_obj_get_child(obj, (int32_t)i);
        lv_area_t c;
        if (!lv_obj_is_visible(child)) continue;
        lv_obj_get_coords(child, &c);
        if (c.x1 < p.x1 - 2 || c.y1 < p.y1 - 2 ||
            c.x2 > p.x2 + 2 || c.y2 > p.y2 + 2) {
            char detail[192];
            snprintf(detail, sizeof(detail),
                     "child=%s child=(%d,%d)-(%d,%d) parent=(%d,%d)-(%d,%d)",
                     obj_class_name(child), c.x1, c.y1, c.x2, c.y2,
                     p.x1, p.y1, p.x2, p.y2);
            geom_result(r, screen, "container_overflow", 0, detail);
            return;
        }
        check_overflow_rec(r, screen, child);
    }
}

static void check_pair(geom_report_t *r, const char *screen, const char *name,
                       lv_obj_t *a, lv_obj_t *b, int vertical) {
    lv_area_t aa, ba;
    int pass;
    char detail[192];
    if (!a || !b) return;
    lv_obj_get_coords(a, &aa);
    lv_obj_get_coords(b, &ba);
    if (vertical)
        pass = aa.y2 < ba.y1;
    else
        pass = aa.x2 < ba.x1;
    snprintf(detail, sizeof(detail), "(%d,%d)-(%d,%d) vs (%d,%d)-(%d,%d)",
             aa.x1, aa.y1, aa.x2, aa.y2, ba.x1, ba.y1, ba.x2, ba.y2);
    geom_result(r, screen, name, pass, detail);
}

static void check_margin(geom_report_t *r, const char *screen, const char *name,
                         lv_obj_t *obj, int left, int right) {
    lv_area_t a;
    int pass;
    char detail[160];
    if (!obj) return;
    lv_obj_get_coords(obj, &a);
    pass = (left < 0 || a.x1 == left) && (right < 0 || a.x2 == right);
    snprintf(detail, sizeof(detail), "x=%d..%d expected left=%d right=%d",
             a.x1, a.x2, left, right);
    geom_result(r, screen, name, pass, detail);
}

/* ── Navigation and interaction checks ─────────────────────────────────── */

static int has_clickable(lv_obj_t *obj) {
    return lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

static void check_nested_clickable(geom_report_t *r, const char *name,
                                   lv_obj_t *obj, int parent_clickable) {
    uint32_t i;
    /* Scroll containers are click targets for dragging, not actions; their
     * child buttons are expected. Only action objects are checked. */
    int clickable = has_clickable(obj) &&
                    !lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    if (parent_clickable && clickable) {
        char detail[160];
        snprintf(detail, sizeof(detail), "class=%s", obj_class_name(obj));
        geom_result(r, name, "no_nested_clickable", 0, detail);
    }
    for (i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *child = lv_obj_get_child(obj, (int32_t)i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        check_nested_clickable(r, name, child, clickable);
    }
}

static int count_primary(lv_obj_t *obj) {
    uint32_t i;
    int count = 0;
    if (!lv_obj_is_visible(obj)) return 0;
    if (lv_obj_check_type(obj, &lv_btn_class)) {
        lv_color_t bg = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
        lv_opa_t opa = lv_obj_get_style_bg_opa(obj, LV_PART_MAIN);
        if (opa == LV_OPA_COVER &&
            lv_color_to8(bg) == lv_color_to8(gw_ui_c_text()))
            count++;
    }
    for (i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *child = lv_obj_get_child(obj, (int32_t)i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        count += count_primary(child);
    }
    return count;
}

static void check_primary(geom_report_t *r, const char *name, lv_obj_t *screen,
                          int expected) {
    int count = count_primary(screen);
    char detail[96];
    snprintf(detail, sizeof(detail), "primary=%d expected=%d", count, expected);
    geom_result(r, name, "one_primary_map", count == expected, detail);
}

static void find_label_text(lv_obj_t *obj, const char *needle, int *found) {
    uint32_t i;
    if (*found) return;
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *text = lv_label_get_text(obj);
        if (text && strstr(text, needle)) { *found = 1; return; }
    }
    for (i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *child = lv_obj_get_child(obj, (int32_t)i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        find_label_text(child, needle, found);
    }
}

static int count_label_text(lv_obj_t *obj, const char *needle, int exact) {
    uint32_t i;
    int count = 0;
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *text = lv_label_get_text(obj);
        if (text && (exact ? (!strcmp(text, needle) != 0)
                         : (strstr(text, needle) != NULL)))
            count++;
    }
    for (i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *child = lv_obj_get_child(obj, (int32_t)i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        count += count_label_text(child, needle, exact);
    }
    return count;
}

static void check_absent_text(geom_report_t *r, const char *name,
                              lv_obj_t *screen, const char *needle) {
    int found = 0;
    find_label_text(screen, needle, &found);
    geom_result(r, name, "no_legacy_text", !found, needle);
}

static void check_status_target(geom_report_t *r, const char *name,
                                lv_obj_t *obj) {
    int pass = obj && lv_obj_is_visible(obj) &&
               lv_obj_get_width(obj) >= GW_UI_TOUCH_MIN &&
               lv_obj_get_height(obj) >= GW_UI_TOUCH_MIN &&
               has_clickable(obj);
    char detail[128];
    snprintf(detail, sizeof(detail), "size=%dx%d clickable=%d",
             obj ? lv_obj_get_width(obj) : 0,
             obj ? lv_obj_get_height(obj) : 0,
             obj ? has_clickable(obj) : 0);
    geom_result(r, name, "status_target_96", pass, detail);
}

static void check_home_geometry(geom_report_t *r) {
    lv_area_t reading, calendar, notes, apps;
    char detail[192];
    lv_obj_get_coords(g_home_reading_card, &reading);
    lv_obj_get_coords(g_home_calendar_card, &calendar);
    lv_obj_get_coords(g_home_notes_card, &notes);
    lv_obj_get_coords(g_home_apps, &apps);

    geom_result(r, "home", "status_below_bar",
                GW_UI_STATUS_BAR_H <= reading.y1,
                "status bar ends before the reading card");
    geom_result(r, "home", "reading_before_widgets", reading.y2 < calendar.y1,
                "continue vs widgets");
    check_pair(r, "home", "widgets_side_by_side", g_home_calendar_card,
               g_home_notes_card, 0);
    geom_result(r, "home", "widgets_content_sized",
                calendar.y1 == notes.y1 &&
                lv_obj_get_height(g_home_calendar_card) < 600 &&
                lv_obj_get_height(g_home_notes_card) < 600,
                "widget cards are content-bounded, not equal-stretched");
    geom_result(r, "home", "widgets_inside_content",
                calendar.y2 < GW_UI_PANEL_H - GW_UI_GAP &&
                notes.y2 < GW_UI_PANEL_H - GW_UI_GAP,
                "widgets stay inside the content area");
    {
        uint32_t i, tiles = 0;
        lv_coord_t tile_w = -1, tile_h = -1;
        int equal = 1;
        for (i = 0; i < lv_obj_get_child_cnt(g_home_apps); i++) {
            lv_obj_t *tile = lv_obj_get_child(g_home_apps, (int32_t)i);
            lv_coord_t w, h;
            if (!lv_obj_is_visible(tile)) continue;
            w = lv_obj_get_width(tile);
            h = lv_obj_get_height(tile);
            if (tile_w < 0) { tile_w = w; tile_h = h; }
            if (w < tile_w - 1 || w > tile_w + 1 || h != tile_h) equal = 0;
            tiles++;
        }
        snprintf(detail, sizeof(detail), "tiles=%u width=%d height=%d",
                 tiles, tile_w, tile_h);
        geom_result(r, "home", "apps_tile_count", tiles == 4, detail);
        geom_result(r, "home", "apps_tile_geometry",
                    equal && tile_h >= GW_UI_TOUCH_MIN &&
                    tile_w >= GW_UI_TOUCH_MIN,
                    "four equal app tiles meet the touch minimum");
        host_ipc_app_list_set("APP demo 1.0 Demo app\nAPP END\n");
        home_apps_refresh();
        {
            uint32_t core = lv_obj_get_child_cnt(g_home_apps);
            uint32_t installed = lv_obj_get_child_cnt(g_home_installed_apps);
            lv_obj_t *tile = installed ? lv_obj_get_child(g_home_installed_apps,
                                                          (int32_t)installed - 1)
                                       : NULL;
            const char *id = tile ? lv_obj_get_user_data(tile) : NULL;
            geom_result(r, "home", "installed_app_tile",
                        core == 4 && installed == 1 && id &&
                            !strcmp(id, "demo") &&
                            !lv_obj_has_flag(g_home_installed_apps,
                                             LV_OBJ_FLAG_HIDDEN),
                        "installed apps get their own visible tile row");
        }
        host_ipc_app_list_set(NULL);
        home_apps_refresh();
    }
    geom_result(r, "home", "app_launcher_order",
                reading.y2 < apps.y1 && calendar.y2 < apps.y1 &&
                notes.y2 < apps.y1,
                "app launcher follows the reading and widget cards");
    geom_result(r, "home", "app_launcher_inside_content",
                apps.y2 < GW_UI_PANEL_H - GW_UI_GAP,
                "app launcher stays inside the content area");
    check_margin(r, "home", "reading_full_margin", g_home_reading_card,
                 GW_UI_MARGIN, GW_UI_PANEL_W - GW_UI_MARGIN - 1);
    check_margin(r, "home", "calendar_left_margin", g_home_calendar_card,
                 GW_UI_MARGIN, -1);
    check_margin(r, "home", "notes_right_margin", g_home_notes_card,
                 -1, GW_UI_PANEL_W - GW_UI_MARGIN - 1);
    geom_result(r, "home", "reading_height",
                lv_obj_get_height(g_home_reading_card) >=
                    GW_UI_HOME_READING_CARD_H - 2,
                "populated continue card keeps its reading height");
    geom_result(r, "home", "widgets_equal_width",
                lv_obj_get_width(g_home_calendar_card) ==
                lv_obj_get_width(g_home_notes_card), "calendar/notes width");
    snprintf(detail, sizeof(detail), "reading=%dx%d calendar=%dx%d notes=%dx%d",
             lv_obj_get_width(g_home_reading_card),
             lv_obj_get_height(g_home_reading_card),
             lv_obj_get_width(g_home_calendar_card),
             lv_obj_get_height(g_home_calendar_card),
             lv_obj_get_width(g_home_notes_card),
             lv_obj_get_height(g_home_notes_card));
    geom_result(r, "home", "card_band_geometry", 1, detail);
    {
        int saved_status[GW_TODO_MAX];
        int i, populated, quiet;
        for (i = 0; i < g_todo_count; i++) saved_status[i] = g_todos[i].status;
        populated = lv_obj_get_height(g_home_notes_card);
        for (i = 0; i < g_todo_count; i++) g_todos[i].status = GW_TODO_DONE;
        apply_model();
        lv_obj_update_layout(g_home_notes_card);
        quiet = lv_obj_get_height(g_home_notes_card);
        for (i = 0; i < g_todo_count; i++) g_todos[i].status = saved_status[i];
        apply_model();
        lv_obj_update_layout(g_home_notes_card);
        geom_result(r, "home", "notes_height_tracks_content",
                    quiet < populated,
                    "To-dos card shrinks when its rows disappear");
    }

    check_status_target(r, "home", g_wifi_button);
    check_status_target(r, "home", g_light_button);
    check_status_target(r, "home", g_lock_button);
    check_status_target(r, "home", g_battery_button);
    geom_result(r, "home", "light_lock_one_tap",
                lv_obj_get_parent(g_light_button) == lv_obj_get_parent(g_lock_button) &&
                has_clickable(g_light_button) && has_clickable(g_lock_button),
                "Light and Lock are status-bar targets");
    check_absent_text(r, "home", g_home_screen, "QUICK ACCESS");
    check_absent_text(r, "home", g_home_screen, "Today.");
    geom_result(r, "home", "upcoming_rows_72",
                g_widget_event_rows[0] && g_widget_event_rows[1] &&
                lv_obj_get_height(g_widget_event_rows[0]) == 72 &&
                lv_obj_get_height(g_widget_event_rows[1]) == 72 &&
                !lv_obj_has_flag(g_widget_event_rows[0], LV_OBJ_FLAG_HIDDEN) &&
                !lv_obj_has_flag(g_widget_event_rows[1], LV_OBJ_FLAG_HIDDEN),
                "two 72 px upcoming rows are visible");
    {
        int normal = lv_obj_get_style_border_width(g_home_reading_card,
                                                   LV_PART_MAIN);
        int pressed;
        lv_obj_add_state(g_home_reading_card, LV_STATE_PRESSED);
        pressed = lv_obj_get_style_border_width(g_home_reading_card,
                                                LV_PART_MAIN);
        lv_obj_clear_state(g_home_reading_card, LV_STATE_PRESSED);
        {
            char detail[96];
            snprintf(detail, sizeof(detail), "normal=%d pressed=%d", normal,
                     pressed);
            geom_result(r, "home", "card_pressed_border",
                        normal == 2 && pressed == 4, detail);
        }
    }
}

static void check_secondary_geometry(geom_report_t *r, const char *name,
                                     lv_obj_t *screen) {
    uint32_t i;
    for (i = 0; i < lv_obj_get_child_cnt(screen); i++) {
        lv_obj_t *child = lv_obj_get_child(screen, (int32_t)i);
        lv_area_t a;
        if (!lv_obj_is_visible(child)) continue;
        lv_obj_get_coords(child, &a);
        if (a.y2 - a.y1 <= 1) continue;                 /* hairline */
        if (a.y1 == 0 && a.y2 == GW_UI_APP_BAR_H - 1) continue; /* app bar */
        geom_result(r, name, "content_below_appbar",
                    a.y1 >= GW_UI_APP_BAR_H, "content starts below app bar");
        geom_result(r, name, "content_above_bottom",
                    a.y2 <= GW_UI_PANEL_H, "content inside panel");
    }
}

static void paint_now(lv_obj_t *screen, int force_full) {
    int i;
    if (force_full) lv_obj_invalidate(screen);
    for (i = 0; i < 12; i++) {
        lv_tick_inc(20);
        lv_timer_handler();
        lv_refr_now(NULL);
        usleep(12000);
    }
}

static int save_current(const char *outdir, const char *name, geom_report_t *r) {
    char path[512];
    paint_now(lv_scr_act(), 0);
    snprintf(path, sizeof(path), "%s/%s.ppm", outdir, name);
    if (gw_host_fb_save_ppm(path) < 0) {
        printf("FAIL %s screenshot write\n", name);
        return -1;
    }
    geom_walk(r, name, lv_scr_act(), lv_scr_act());
    return 0;
}

static int dump_screen(const char *outdir, const char *name, gw_screen_t screen,
                       geom_report_t *r, int force_full) {
    char path[512];
    paint_now(lv_scr_act(), force_full);
    snprintf(path, sizeof(path), "%s/%s.ppm", outdir, name);
    if (gw_host_fb_save_ppm(path) < 0) {
        printf("FAIL %s screenshot write\n", name);
        return -1;
    }
    geom_walk(r, name, lv_scr_act(), lv_scr_act());
    check_overflow_rec(r, name, lv_scr_act());
    if (screen == GW_SCREEN_HOME) check_home_geometry(r);
    else if (screen != GW_SCREEN_LOCK) check_secondary_geometry(r, name, lv_scr_act());
    return 0;
}

static void check_state(geom_report_t *r, gw_screen_t screen, lv_obj_t *expected) {
    char detail[160];
    gw_ui_show(screen);
    snprintf(detail, sizeof(detail), "active=%d expected_screen=%p actual=%p",
             g_active_screen, (void *)expected, (void *)lv_scr_act());
    geom_result(r, gw_screen_name(screen), "navigation_transition",
                g_active_screen == (int)screen && lv_scr_act() == expected,
                detail);
}

static void check_callback(geom_report_t *r, const char *name,
                           lv_event_cb_t cb, gw_screen_t screen,
                           lv_obj_t *expected) {
    char detail[160];
    cb(NULL);
    snprintf(detail, sizeof(detail), "callback=%s active=%d expected=%p actual=%p",
             name, g_active_screen, (void *)expected, (void *)lv_scr_act());
    geom_result(r, gw_screen_name(screen), "widget_callback",
                g_active_screen == (int)screen && lv_scr_act() == expected,
                detail);
}

static int has_glyph(const lv_font_t *font, uint32_t codepoint) {
    lv_font_glyph_dsc_t d;
    return font && lv_font_get_glyph_dsc(font, &d, codepoint, 0);
}

static void check_catalogue_ascii(geom_report_t *r) {
    const char *paths[3] = { "os/ui/gw_ui_strings.h", "gw_ui_strings.h",
                             "../gw_ui_strings.h" };
    FILE *f = NULL;
    int i, ascii = 1;
    char detail[160] = "file not found";
    for (i = 0; i < 3 && !f; i++) f = fopen(paths[i], "rb");
    if (f) {
        int c;
        ascii = 1;
        while ((c = fgetc(f)) != EOF)
            if (c < 0 || c > 127) { ascii = 0; break; }
        fclose(f);
        snprintf(detail, sizeof(detail), "path=%s", paths[i - 1]);
    } else {
        ascii = 0;
    }
    geom_result(r, "strings", "catalogue_ascii_only", ascii, detail);
}

int main(int argc, char **argv) {
    const char *outdir = argc > 1 ? argv[1] : "/tmp/gw-ui-preview";
    char json_path[512];
    geom_report_t report;
    mkdir(outdir, 0755);
    install_crash_handler();
    if (getenv("GW_UI_TEST_CRASH")) raise(SIGSEGV);

    lv_init();
    if (lv_port_epdc_init() < 0) {
        fprintf(stderr, "host display init failed\n");
        return 1;
    }
    if (lv_port_input_init() < 0) {
        fprintf(stderr, "host input init failed\n");
        return 1;
    }

    gw_ui_build();
    paint_now(lv_scr_act(), 1);
    {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        printf("lvgl heap: total=%u used=%u max_used=%u free=%u used_pct=%u%% frag=%u%%\n",
               mon.total_size, mon.total_size - mon.free_size, mon.max_used,
               mon.free_size, mon.used_pct, mon.frag_pct);
    }

    snprintf(json_path, sizeof(json_path), "%s/geometry.json", outdir);
    report.json = fopen(json_path, "w");
    report.total = 0;
    report.failed = 0;
    if (!report.json) {
        fprintf(stderr, "cannot write geometry.json: %s\n", json_path);
        return 1;
    }
    fprintf(report.json, "{\n  \"checks\": [");

    /* ── TODO store and behaviour ─────────────────────────────────────── */
    {
        gw_todo_t saved[GW_TODO_MAX];
        int saved_count = g_todo_count;
        int saved_next = g_todo_next_id;
        int saved_history = g_todo_history;
        int before = g_todo_count;
        int rc, id;
        memcpy(saved, g_todos, sizeof(saved));
        rc = todo_add("Host test task", "2026-11-12");
        id = (rc == 0) ? g_todos[g_todo_count - 1].id : -1;
        geom_result(&report, "todos", "store_add",
                    rc == 0 && g_todo_count == before + 1 && id > 0,
                    "new task becomes active");
        geom_result(&report, "todos", "due_roundtrip",
                    todo_find(id) && !strcmp(todo_find(id)->due, "2026-11-12"),
                    todo_find(id) ? todo_find(id)->due : "missing");
        todo_toggle(id);
        geom_result(&report, "todos", "store_complete",
                    todo_find(id) && todo_find(id)->status == GW_TODO_DONE,
                    "checkbox marks task done");
        todo_toggle(id);
        geom_result(&report, "todos", "store_reopen",
                    todo_find(id) && todo_find(id)->status == GW_TODO_ACTIVE,
                    "unchecked task returns to active");
        todo_delete(id);
        geom_result(&report, "todos", "store_delete",
                    todo_find(id) && todo_find(id)->status == GW_TODO_DELETED,
                    "delete is a soft delete");
        todo_restore(id);
        geom_result(&report, "todos", "store_restore",
                    todo_find(id) && todo_find(id)->status == GW_TODO_ACTIVE,
                    "restore returns task to active");

        /* Active view excludes done/deleted tasks. */
        todo_toggle(id);
        g_todo_history = 0;
        todo_rebuild_list();
        check_absent_text(&report, "todos", g_todo_list, "Host test task");
        geom_result(&report, "todos", "active_excludes_done",
                    todo_find(id)->status == GW_TODO_DONE,
                    "done task is not in the active view");
        /* History view contains the done row with a DONE label. */
        g_todo_history = 1;
        todo_rebuild_list();
        {
            int done = 0, deleted = 0;
            find_label_text(g_todo_list, GW_STR_DONE, &done);
            find_label_text(g_todo_list, GW_STR_DELETED, &deleted);
            geom_result(&report, "todos", "history_labels",
                        done && deleted, "DONE and DELETED labels present");
        }
        g_todo_history = saved_history;
        memcpy(g_todos, saved, sizeof(saved));
        g_todo_count = saved_count;
        g_todo_next_id = saved_next;
        todo_rebuild_list();
    }
    {
        gw_todo_t parsed;
        geom_result(&report, "todos", "tsv_legacy_five_fields",
                    todo_parse_line("7\t0\t1\t2\tLegacy task\n", &parsed) == 0 &&
                    !parsed.due[0] && !strcmp(parsed.text, "Legacy task"),
                    "old five-field line loads with empty due");
        geom_result(&report, "todos", "tsv_six_fields",
                    todo_parse_line("8\t0\t1\t2\t2026-11-12\tNew task\n",
                                    &parsed) == 0 &&
                    !strcmp(parsed.due, "2026-11-12") &&
                    !strcmp(parsed.text, "New task"),
                    "six-field line keeps due and text");
    }
    /* Delete -> Undo restores. */
    {
        int before = g_todo_count;
        int before_next = g_todo_next_id;
        int rc = todo_add("Undo fixture task", "");
        int id = (rc == 0) ? g_todos[g_todo_count - 1].id : -1;
        todo_delete(id);
        todo_commit();
        gw_ui_kit_undo_create(GW_STR_TASK_DELETED, GW_STR_UNDO,
                              todo_restore_action, (void *)(intptr_t)id);
        geom_result(&report, "todos", "undo_bar_open",
                    gw_ui_kit_undo_is_open(), "undo strip created");
        {
            lv_obj_t *action = gw_ui_kit_undo_action_button();
            if (action) lv_event_send(action, LV_EVENT_CLICKED, NULL);
            geom_result(&report, "todos", "undo_restores",
                        todo_find(id) && todo_find(id)->status == GW_TODO_ACTIVE,
                        "task returned to active after Undo");
        }
        geom_result(&report, "todos", "undo_closes",
                    !gw_ui_kit_undo_is_open(), "undo strip dismissed");
        g_todo_count = before;
        g_todo_next_id = before_next;
        todo_rebuild_list();
    }

    /* ── English dates and calendar ────────────────────────────────────── */
    {
        char date[96];
        format_hero_date(date, sizeof(date));
        geom_result(&report, "calendar", "english_date",
                    !strcmp(date, "Saturday, 31 October 2026"), date);
        {
            char detail[96];
            snprintf(detail, sizeof(detail), "2028=%d 2027=%d",
                     days_in_month(2028, 1), days_in_month(2027, 1));
            geom_result(&report, "calendar", "leap_year",
                        days_in_month(2028, 1) == 29 &&
                        days_in_month(2027, 1) == 28, detail);
        }
        {
            /* Sunday-first header + tm_wday: the day label and the weekday
             * header in the same column must agree for known dates. */
            struct { int year, month, day; const char *weekday; } cases[] = {
                { 2026, 9, 1, "Thu" },
                { 2026, 10, 1, "Sun" },
                { 2027, 1, 1, "Mon" }
            };
            int saved_year = g_calendar_year;
            int saved_month = g_calendar_month;
            int saved_day = g_calendar_selected_day;
            unsigned c;
            for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
                struct tm first;
                int index;
                char name[96];
                memset(&first, 0, sizeof(first));
                first.tm_year = cases[c].year - 1900;
                first.tm_mon = cases[c].month;
                first.tm_mday = cases[c].day;
                first.tm_hour = 12;
                mktime(&first);
                index = first.tm_wday;
                g_calendar_year = cases[c].year;
                g_calendar_month = cases[c].month;
                g_calendar_selected_day = cases[c].day;
                calendar_render();
                lv_obj_update_layout(g_calendar_screen);
                snprintf(name, sizeof(name), "%04d-%02d-%02d",
                         cases[c].year, cases[c].month + 1, cases[c].day);
                geom_result(&report, "calendar", "weekday_label_matches_cell",
                            g_calendar_weekday_labels[index] &&
                            !strcmp(lv_label_get_text(
                                        g_calendar_weekday_labels[index]),
                                    cases[c].weekday) &&
                            !strcmp(lv_label_get_text(
                                        g_calendar_day_labels[index]), "1"),
                            name);
            }
            g_calendar_year = saved_year;
            g_calendar_month = saved_month;
            g_calendar_selected_day = saved_day;
            calendar_render();
        }
        gw_ui_show(GW_SCREEN_CALENDAR);
        paint_now(lv_scr_act(), 0);
        geom_result(&report, "calendar", "cell_geometry",
                    lv_obj_get_height(g_calendar_cells[0]) == 96 &&
                    lv_obj_get_width(g_calendar_cells[0]) >= 96,
                    "calendar cells are at least 96 px");
        geom_result(&report, "calendar", "month_title",
                    !strcmp(lv_label_get_text(g_calendar_title_label),
                            "October 2026"), "October 2026");
        {
            unsigned before = g_full_paint_requests;
            int selected = g_calendar_selected_day;
            calendar_select_day(15);
            geom_result(&report, "calendar", "selection_partial",
                        g_full_paint_requests == before &&
                        g_calendar_selected_day == 15 &&
                        selected == 31, "day selection is a partial update");
            geom_result(&report, "calendar", "empty_day",
                        !strcmp(lv_label_get_text(g_calendar_detail_empty),
                                GW_STR_NO_ITEMS_DAY),
                        "empty day message");
        }
        {
            unsigned before = g_full_paint_requests;
            calendar_month_shift(1);
            geom_result(&report, "calendar", "month_next",
                        !strcmp(lv_label_get_text(g_calendar_title_label),
                                "November 2026") &&
                        g_full_paint_requests == before + 1,
                        "next month repaints once");
            geom_result(&report, "calendar", "selected_clamped",
                        g_calendar_selected_day == 15,
                        "selected day retained when valid");
            calendar_month_shift(-1);
            geom_result(&report, "calendar", "month_back",
                        !strcmp(lv_label_get_text(g_calendar_title_label),
                                "October 2026"),
                        "previous month returns");
            calendar_select_day(31);
        }
    }

    /* ── Network parsing and modal behaviour ───────────────────────────── */
    {
        gw_net_ap_t ap;
        int parsed;
        parsed = net_result_parse("NET SSID Cafe\n", &ap);
        geom_result(&report, "network", "old_plain_ssid",
                    parsed == 0 && !strcmp(ap.ssid, "Cafe") && ap.signal == 0 &&
                    ap.secure == 0, "legacy plain SSID accepted");
        parsed = net_result_parse("NET SSID 486f6d652057692d4669 -50 1\n", &ap);
        geom_result(&report, "network", "new_hex_ssid",
                    parsed == 0 && !strcmp(ap.ssid, "Home Wi-Fi") &&
                    ap.signal == -50 && ap.secure == 1,
                    "hex SSID, signal and security accepted");
        parsed = net_result_parse("NET SSID zzzz -50 1\n", &ap);
        geom_result(&report, "network", "malformed_hex_rejected",
                    parsed != 0, "malformed hex is rejected");
        parsed = net_result_parse("NET SSID 4 -50 1\n", &ap);
        geom_result(&report, "network", "odd_hex_rejected", parsed != 0,
                    "odd-length hex is rejected");
        geom_result(&report, "network", "signal_mapping",
                    signal_bars_for(-50) == 4 && signal_bars_for(-60) == 3 &&
                    signal_bars_for(-70) == 2 && signal_bars_for(-80) == 1 &&
                    signal_bars_for(0) == 0,
                    "signal bar thresholds");
    }
    {
        unsigned before = g_full_paint_requests;
        int state = g_net_state;
        g_net_state = 1;
        wifi_disconnect(NULL);
        geom_result(&report, "network", "confirm_blocks",
                    gw_ui_kit_confirm_is_open() && g_net_state == 1,
                    "underlying action runs only after confirmation");
        geom_result(&report, "network", "modal_one_repaint",
                    g_full_paint_requests == before + 1 &&
                    gw_eink_transition_active(),
                    "one DU4 transition repaint on modal open");
        gw_ui_kit_confirm_close();
        geom_result(&report, "network", "modal_close_partial",
                    g_full_paint_requests == before + 2 &&
                    gw_eink_transition_active(),
                    "close repaints the covered panel with DU4 partial");
        g_net_state = state;
    }
    {
        lv_obj_t *holder = lv_obj_create(lv_layer_top());
        lv_obj_t *button;
        lv_obj_set_size(holder, 400, 200);
        gw_ui_style_plain_panel(holder);
        button = gw_ui_kit_button_create(holder, "Busy", 200, 96,
                                         GW_UI_BUTTON_PRIMARY);
        gw_ui_kit_busy_set(button, 1, GW_STR_SCANNING);
        geom_result(&report, "network", "busy_disables",
                    gw_ui_kit_button_is_busy(button) &&
                    lv_obj_has_state(button, LV_STATE_DISABLED),
                    "busy button is disabled and rejects a second tap");
        gw_ui_kit_busy_set(button, 0, NULL);
        lv_obj_del(holder);
    }

    /* ── Glyphs and catalogue ──────────────────────────────────────────── */
    {
        int glyphs = has_glyph(&gw_font_28, 0xF02D) &&
                     has_glyph(&gw_font_28, 0xF073) &&
                     has_glyph(&gw_font_28, 0xF14A) &&
                     has_glyph(&gw_font_28, 0xF023) &&
                     has_glyph(&gw_font_28, 0xF012) &&
                     has_glyph(&gw_font_28, 0xF2ED) &&
                     has_glyph(&gw_font_28, 0xF06E) &&
                     has_glyph(&gw_font_28, 0xF070) &&
                     has_glyph(&gw_font_28, 0xF053) &&
                     has_glyph(&gw_font_28, 0xF054) &&
                     has_glyph(&gw_font_28, 0xF067) &&
                     has_glyph(&gw_font_28, 0xF068) &&
                     has_glyph(&gw_font_28, 0xF287);
        geom_result(&report, "fonts", "icon_glyphs", glyphs,
                    "book/calendar/check/lock/signal/trash/eye/chevron/plus/minus/usb");
        geom_result(&report, "fonts", "body_32",
                    has_glyph(&gw_font_32, 'A') &&
                    has_glyph(&gw_font_32_bold, 'A'),
                    "32 px regular and bold fonts present");
        check_catalogue_ascii(&report);
    }

    /* Navigation state machine. */
    check_state(&report, GW_SCREEN_HOME, g_home_screen);
    check_state(&report, GW_SCREEN_SETTINGS, g_settings_screen);
    check_state(&report, GW_SCREEN_CALENDAR, g_calendar_screen);
    check_state(&report, GW_SCREEN_NOTES, g_notes_screen);
    check_state(&report, GW_SCREEN_NETWORK, g_network_screen);
    check_state(&report, GW_SCREEN_APPS, g_apps_screen);
    check_state(&report, GW_SCREEN_HOME, g_home_screen);

    /* Exercise the exact callbacks connected to the rendered widgets. */
    check_callback(&report, "nav_settings_cb", nav_settings_cb,
                   GW_SCREEN_SETTINGS, g_settings_screen);
    check_callback(&report, "calendar_open_cb", calendar_open_cb,
                   GW_SCREEN_CALENDAR, g_calendar_screen);
    check_callback(&report, "notes_open_cb", notes_open_cb,
                   GW_SCREEN_NOTES, g_notes_screen);
    check_callback(&report, "network_open_cb", network_open_cb,
                   GW_SCREEN_NETWORK, g_network_screen);
    check_callback(&report, "apps_open_cb", apps_open_cb,
                   GW_SCREEN_APPS, g_apps_screen);
    check_callback(&report, "home_hub_open_cb", home_hub_open_cb,
                   GW_SCREEN_HUB, g_hub_screen);
    reader_launch(NULL);
    geom_result(&report, "home", "reader_callback_host_safe",
                g_exit_for_reader == 0, "host preview does not exit on launch");

    /* Keyboard typing policy: no delayed password reveal, no blinking
     * cursor, no dark pressed-state inversion. */
    geom_result(&report, "network", "password_no_delayed_reveal",
                lv_textarea_get_password_show_time(g_net_password) == 0,
                "password characters hide immediately");
    geom_result(&report, "network", "password_cursor_no_blink",
                lv_obj_get_style_anim_time(g_net_password, LV_PART_CURSOR) == 0,
                "cursor anim_time is zero");
    {
        lv_style_value_t v;
        unsigned pressed = 0, wanted = lv_color_to8(gw_ui_c_text());
        int found = lv_obj_get_local_style_prop(
                        g_net_keyboard, LV_STYLE_BG_COLOR, &v,
                        LV_PART_ITEMS | LV_STATE_PRESSED) == LV_STYLE_RES_FOUND;
        if (found) pressed = lv_color_to8(v.color);
        {
            char detail[128];
            snprintf(detail, sizeof(detail), "pressed=%u wanted=%u found=%d",
                     pressed, wanted, found);
            geom_result(&report, "network", "keyboard_press_highlight",
                        found && pressed == wanted, detail);
        }
    }
    {
        lv_style_value_t v;
        int found = lv_obj_get_local_style_prop(
                        g_net_keyboard, LV_STYLE_COLOR_FILTER_OPA, &v,
                        LV_PART_ITEMS | LV_STATE_PRESSED) == LV_STYLE_RES_FOUND;
        geom_result(&report, "network", "keyboard_press_no_filter",
                    found && v.num == LV_OPA_TRANSP,
                    "theme darkening filter disabled");
    }
    {
        int symbols = has_glyph(&gw_font_28, 0xF55A) &&
                      has_glyph(&gw_font_28, 0xF8A2) &&
                      has_glyph(&gw_font_28, 0xF00C) &&
                      has_glyph(&gw_font_28, 0xF11C);
        geom_result(&report, "network", "keyboard_symbol_glyphs", symbols,
                    "backspace/enter/check/keyboard glyphs present");
    }
    geom_result(&report, "todos", "todos_cursor_no_blink",
                lv_obj_get_style_anim_time(g_notes_area, LV_PART_CURSOR) == 0,
                "cursor anim_time is zero");
    /* Empty / loading / populated states. */
    {
        char saved_title[sizeof(g_model.book_title)];
        int saved_progress = g_model.book_progress;
        snprintf(saved_title, sizeof(saved_title), "%s", g_model.book_title);
        g_model.book_title[0] = '\0';
        g_model.book_progress = -1;
        apply_model();
        geom_result(&report, "home", "empty_reading_state",
                    !lv_obj_has_flag(g_reading_empty, LV_OBJ_FLAG_HIDDEN) &&
                    lv_obj_has_flag(g_reading_body, LV_OBJ_FLAG_HIDDEN),
                    "empty card visible, body hidden");
        {
            int found = 0;
            find_label_text(g_reading_empty, GW_STR_NO_BOOK_OPEN, &found);
            geom_result(&report, "home", "empty_reading_text", found,
                        "explicit no-book message");
        }
        lv_obj_update_layout(g_home_reading_card);
        {
            char detail[64];
            snprintf(detail, sizeof(detail), "height=%d limit=%d title=%d",
                     (int)lv_obj_get_height(g_home_reading_card),
                     GW_UI_HOME_EMPTY_CARD_H, (int)strlen(g_model.book_title));
            geom_result(&report, "home", "empty_reading_height",
                        lv_obj_get_height(g_home_reading_card) <=
                            GW_UI_HOME_EMPTY_CARD_H,
                        detail);
        }
        gw_ui_show(GW_SCREEN_HOME);
        paint_now(lv_scr_act(), 1);
        geom_walk(&report, "home-empty", lv_scr_act(), lv_scr_act());
        {
            char path[512];
            snprintf(path, sizeof(path), "%s/home-empty.ppm", outdir);
            if (gw_host_fb_save_ppm(path) < 0)
                geom_result(&report, "home-empty", "screenshot_write", 0, path);
        }
        snprintf(g_model.book_title, sizeof(g_model.book_title), "%s", saved_title);
        g_model.book_progress = saved_progress;
        apply_model();
        geom_result(&report, "home", "populated_reading_state",
                    g_reading_empty == NULL &&
                    !lv_obj_has_flag(g_reading_body, LV_OBJ_FLAG_HIDDEN),
                    "reading body restored");
    }
    {
        int i;
        int statuses[GW_TODO_MAX];
        for (i = 0; i < g_todo_count; i++) statuses[i] = g_todos[i].status;
        for (i = 0; i < g_todo_count; i++)
            if (g_todos[i].status == GW_TODO_ACTIVE)
                g_todos[i].status = GW_TODO_DONE;
        apply_model();
        geom_result(&report, "home", "empty_notes_state",
                    !strcmp(lv_label_get_text(g_widget_todo_count),
                            GW_STR_NO_TASKS_YET),
                    "explicit no-tasks message");
        for (i = 0; i < g_todo_count; i++) g_todos[i].status = statuses[i];
        apply_model();
    }
    {
        char saved_event[sizeof(g_model.event_title)];
        int i;
        int statuses[GW_TODO_MAX];
        snprintf(saved_event, sizeof(saved_event), "%s", g_model.event_title);
        for (i = 0; i < g_todo_count; i++) statuses[i] = g_todos[i].status;
        g_model.event_title[0] = '\0';
        for (i = 0; i < g_todo_count; i++)
            if (g_todos[i].status == GW_TODO_ACTIVE)
                g_todos[i].status = GW_TODO_DONE;
        apply_model();
        geom_result(&report, "home", "upcoming_empty_state",
                    !strcmp(lv_label_get_text(g_widget_event_labels[0]),
                            GW_STR_NO_UPCOMING_EVENTS) &&
                    lv_obj_has_flag(g_widget_event_rows[1], LV_OBJ_FLAG_HIDDEN),
                    "one empty message and no second row");
        snprintf(g_model.event_title, sizeof(g_model.event_title), "%s",
                 saved_event);
        for (i = 0; i < g_todo_count; i++) g_todos[i].status = statuses[i];
        apply_model();
    }
    {
        g_wifi_scan_pending = 0;
        network_scan_apply();
        geom_result(&report, "network", "scan_result_rows",
                    g_net_ap_count == 3 &&
                    !strncmp(g_net_aps[0].ssid, "Home", 4),
                    "mock scan result populated the network list");
        {
            lv_obj_t *first_row = g_net_rows[0];
            network_scan_apply();
            geom_result(&report, "network", "scan_no_rebuild_unchanged",
                        first_row != NULL && g_net_rows[0] == first_row,
                        "identical scan results keep the existing rows");
        }
        {
            update_scan();
            g_update_selected = 0;
            update_selection_refresh();
            if (g_update_install_button)
                lv_event_send(g_update_install_button, LV_EVENT_CLICKED, NULL);
            {
                char detail[96];
                snprintf(detail, sizeof(detail), "count=%d selected=%d last=%s",
                         g_update_package_count, g_update_selected,
                         g_host_last_ipc);
                geom_result(&report, "update", "install_sends_selected_index",
                            !strcmp(g_host_last_ipc, "APP INSTALL USB 0"),
                            detail);
            }
        }
        {
            /* Removing the middle AP must move the last row's identity with
             * it, not leave it pointing at the deleted neighbour. */
            g_net_aps[1] = g_net_aps[2];
            g_net_ap_count = 2;
            network_build_rows();
            geom_result(&report, "network", "row_identity_after_delete",
                        g_net_row_count == 2 &&
                        !strcmp((const char *)lv_obj_get_user_data(
                                    g_net_rows[0]), "Home Wi-Fi") &&
                        !strcmp((const char *)lv_obj_get_user_data(
                                    g_net_rows[1]), "Cafe"),
                        "deleting the middle AP keeps row user data correct");
            network_scan_apply();
        }
    }
    {
        unsigned before;
        int i;
        status_update(NULL);
        before = g_ui_text_updates;
        for (i = 0; i < 10; i++) status_update(NULL);
        geom_result(&report, "home", "rapid_status_no_rewrite",
                    g_ui_text_updates == before,
                    "10 rapid status updates with unchanged text write no label");
    }
    {
        lv_obj_t *label = g_todo_due_buttons[2]
                              ? lv_obj_get_child(g_todo_due_buttons[2], 0)
                              : NULL;
        {
            const lv_font_t *f = label ? lv_obj_get_style_text_font(label, 0)
                                       : NULL;
            char detail[128];
            snprintf(detail, sizeof(detail), "font=%p want=%p", (void *)f,
                     (void *)&gw_font_28);
            geom_result(&report, "todos", "due_button_font",
                        f == &gw_font_28, detail);
        }
    }
    {
        /* E02: more than three due items produce a "+N more" row. */
        gw_todo_t saved[GW_TODO_MAX];
        int saved_count = g_todo_count;
        int saved_next = g_todo_next_id;
        int i;
        memcpy(saved, g_todos, sizeof(saved));
        for (i = 0; i < 4; i++) todo_add("Due overflow fixture", NULL);
        gw_ui_now(&(struct tm){0});
        {
            struct tm now;
            char iso[11];
            gw_ui_now(&now);
            format_iso_date(now.tm_year + 1900, now.tm_mon, now.tm_mday,
                            iso, sizeof(iso));
            for (i = saved_count; i < g_todo_count; i++)
                snprintf(g_todos[i].due, sizeof(g_todos[i].due), "%s", iso);
        }
        g_calendar_year = 2026;
        g_calendar_month = 9;
        g_calendar_selected_day = 31;
        calendar_detail_render();
        geom_result(&report, "calendar", "overflow_more_row",
                    g_calendar_detail_more &&
                    !lv_obj_has_flag(g_calendar_detail_more,
                                     LV_OBJ_FLAG_HIDDEN),
                    "fourth+ item is summarised as +N more");
        memcpy(g_todos, saved, sizeof(saved));
        g_todo_count = saved_count;
        g_todo_next_id = saved_next;
        todo_rebuild_list();
        apply_model();
    }

    /* ── Refresh policy and remediation checks (A0/A9/B/C/D/E) ─────────── */
    {
        gw_shell_apply_profile(GW_SCREEN_NETWORK);
        uint32_t ms = gw_eink_scroll_preview_ms();
        geom_result(&report, "refresh", "scroll_preview_throttle",
                    ms >= 100 && ms <= 300,
                    "live scroll preview is capped to the list profile");
        gw_eink_set_scroll_preview_ms(150);
        geom_result(&report, "refresh", "scroll_preview_configurable",
                    gw_eink_scroll_preview_ms() == 150,
                    "preview interval can be tuned without code changes");
        gw_eink_set_scroll_preview_ms(ms);
        geom_result(&report, "refresh", "scroll_preview_cap",
                    gw_eink_scroll_preview_max() >= 1 &&
                    gw_eink_scroll_preview_max() <= 8,
                    "long drags limit preview count to reduce ghost");
        gw_shell_apply_profile(GW_SCREEN_HOME);
    }
    {
        unsigned before = g_deep_clean_requests;
        gw_ui_show(GW_SCREEN_HOME);
        geom_result(&report, "refresh", "scene_no_gc16",
                    g_deep_clean_requests == before &&
                    gw_eink_current_intent() != GW_EINK_INTENT_DEEP_CLEAN,
                    "navigation requests a scene partial, not a deep clean");
        geom_result(&report, "refresh", "scene_waveform_16_level",
                    gw_eink_scene_waveform() == WAVEFORM_MODE_GC16_FAST ||
                    gw_eink_scene_waveform() == WAVEFORM_MODE_GL16,
                    "default scene waveform keeps 16 gray levels");
    }
    {
        /* A tap inside a scrollable settings page emits RELEASED but is not a
         * drag; it must not schedule a scroll/GC16 cleanup. */
        g_eink_intent = GW_EINK_INTENT_NONE;
        gw_eink_clear_scroll_move();
        if (g_settings_content)
            lv_event_send(g_settings_content, LV_EVENT_RELEASED, NULL);
        geom_result(&report, "refresh", "tap_release_no_scroll_cleanup",
                    !gw_eink_scroll_pending(),
                    "plain release on a scrollable page is not a scroll");
    }
    {
        unsigned before = g_deep_clean_requests;
        gw_scene_show(GW_SCREEN_LOCK, 1);
        geom_result(&report, "refresh", "lock_deep_clean",
                    g_deep_clean_requests == before + 1,
                    "lock performs the allowed GC16 cleanup");
        gw_ui_show(GW_SCREEN_HOME);
    }
    {
        /* On-release scroll: while the finger is down the intent stays
         * SCROLL, so the EPDC port retains the dirty list. */
        gw_eink_set_touch_down(1);
        gw_eink_request_scroll(1000);
        geom_result(&report, "refresh", "scroll_on_release_intent",
                    gw_eink_scroll_pending() && gw_eink_touch_is_down(),
                    "drag marks scroll intent while touch is held");
        gw_eink_set_touch_down(0);
        gw_eink_clear_scroll_move();
        gw_ui_show(GW_SCREEN_HOME);
    }
    {
        unsigned before = g_deep_clean_requests;
        g_text_input_active = 1;
        gw_eink_set_text_input(1);
        g_settle_refresh_pending = 1;
        g_text_input_active = 0;
        gw_eink_set_text_input(0);
        geom_result(&report, "refresh", "keyboard_dismiss_no_gc16",
                    g_deep_clean_requests == before,
                    "keyboard dismissal does not request GC16");
    }
    {
        recovery_set("Test toast");
        geom_result(&report, "feedback", "toast_on_current_screen",
                    gw_ui_kit_toast_is_open(),
                    "error/status toast appears on the top layer");
        gw_ui_kit_toast_close();
        geom_result(&report, "feedback", "toast_closes",
                    !gw_ui_kit_toast_is_open(), "toast dismisses cleanly");
    }
    {
        g_back_screen = GW_SCREEN_HOME;
        gw_open_secondary(GW_SCREEN_NETWORK, GW_SCREEN_SETTINGS);
        geom_result(&report, "navigation", "back_context",
                    g_active_screen == GW_SCREEN_NETWORK &&
                    g_back_screen == GW_SCREEN_SETTINGS,
                    "Network remembers Settings as its parent");
        gw_back_cb(NULL);
        geom_result(&report, "navigation", "back_returns_to_parent",
                    g_active_screen == GW_SCREEN_SETTINGS,
                    "Back returns to the screen that opened Network");
    }
    {
        gw_ui_show(GW_SCREEN_APPS);
        if (g_update_selected >= 0) {
            g_update_selected = -1;
            update_selection_refresh();
        }
        geom_result(&report, "update", "package_found",
                    g_update_package_count == 1,
                    "scan lists the USB package");
        geom_result(&report, "update", "install_disabled_without_selection",
                    g_update_selected < 0 && g_update_install_button &&
                    lv_obj_has_state(g_update_install_button,
                                     LV_STATE_DISABLED),
                    "Install is inactive until a package is selected");
        g_update_selected = 0;
        update_selection_refresh();
        lv_obj_update_layout(g_apps_screen);
        geom_result(&report, "update", "install_enabled_with_selection",
                    !lv_obj_has_state(g_update_install_button,
                                      LV_STATE_DISABLED),
                    "Install becomes active after selecting a package");
        geom_result(&report, "update", "one_primary_after_selection",
                    count_primary(g_apps_screen) == 1,
                    "Install selected is the only primary action");
        g_update_selected = -1;
        update_selection_refresh();
    }
    {
        status_apply("STATUS battery=82 light=37 power=battery "
                     "net=disconnected usb=host-class lock=unlocked\n");
        geom_result(&report, "home", "usb_icon_on_data_link",
                    g_usb_label && !lv_obj_has_flag(g_usb_label,
                                                   LV_OBJ_FLAG_HIDDEN) &&
                    strcmp(lv_label_get_text(g_usb_label), LV_SYMBOL_USB) == 0,
                    "USB icon (no text) appears for a host-class data link");
        status_apply("STATUS battery=82 light=37 power=battery "
                     "net=disconnected usb=none lock=unlocked\n");
        geom_result(&report, "home", "usb_icon_off_without_link",
                    g_usb_label && lv_obj_has_flag(g_usb_label,
                                                   LV_OBJ_FLAG_HIDDEN),
                    "USB icon stays hidden for charger-only/none");
    }
    {
        char text[48];
        format_storage_value(871.4 * 1024.0 * 1024.0,
                             3.0 * 1024.0 * 1024.0 * 1024.0,
                             text, sizeof(text));
        geom_result(&report, "settings", "storage_user_units",
                    !strcmp(text, "871 MB free of 3.0 GB"), text);
        format_storage_value(20.1 * 1024.0 * 1024.0 * 1024.0,
                             32.0 * 1024.0 * 1024.0 * 1024.0,
                             text, sizeof(text));
        geom_result(&report, "settings", "storage_gb_units",
                    !strcmp(text, "20.1 GB free of 32.0 GB"), text);
    }
    {
        geom_result(&report, "home", "battery_icon_mapping",
                    !strcmp(battery_icon(5, 0), LV_SYMBOL_BATTERY_EMPTY) &&
                    !strcmp(battery_icon(30, 0), LV_SYMBOL_BATTERY_1) &&
                    !strcmp(battery_icon(60, 0), LV_SYMBOL_BATTERY_2) &&
                    !strcmp(battery_icon(90, 0), LV_SYMBOL_BATTERY_3) &&
                    !strcmp(battery_icon(90, 1), LV_SYMBOL_CHARGE),
                    "battery icon follows percentage and charging");
        geom_result(&report, "home", "wifi_tray_single_indicator",
                    g_wifi_button != NULL &&
                    lv_obj_get_child_cnt(g_wifi_button) == 1 &&
                    g_wifi_bars != NULL,
                    "status tray has one network indicator (signal bars)");
    }
    {
        lv_coord_t pad = (g_light_bar && g_light_bar->spec_attr)
                             ? g_light_bar->spec_attr->ext_click_pad : 0;
        geom_result(&report, "settings", "frontlight_is_slider",
                    g_light_bar && lv_obj_check_type(g_light_bar,
                                                     &lv_slider_class) &&
                    pad >= 28,
                    "frontlight uses a real slider with a 96 px touch area");
    }
    {
        int apps_ok;
        lv_obj_update_layout(g_apps_screen);
        lv_obj_update_layout(g_network_screen);
        lv_obj_update_layout(g_notes_screen);
        apps_ok = lv_obj_get_style_pad_right(g_apps_list, LV_PART_MAIN) >= 16 &&
                      lv_obj_get_child_cnt(g_apps_list) >= 4;
        int net_ok = lv_obj_get_style_pad_right(g_net_list, LV_PART_MAIN) >= 16;
        int todo_ok = lv_obj_get_style_pad_right(g_todo_list, LV_PART_MAIN) >= 16;
        {
            char detail[160];
            snprintf(detail, sizeof(detail),
                     "apps pad=%d net pad=%d todo pad=%d ptr=%p cnt=%d ac=%d",
                     (int)lv_obj_get_style_pad_right(g_apps_list, LV_PART_MAIN),
                     (int)lv_obj_get_style_pad_right(g_net_list, LV_PART_MAIN),
                     (int)lv_obj_get_style_pad_right(g_todo_list, LV_PART_MAIN),
                     (void *)g_apps_list,
                     (int)lv_obj_get_child_cnt(g_apps_list), g_app_count);
            geom_result(&report, "layout", "scrollbar_gutter",
                        apps_ok && net_ok && todo_ok, detail);
        }
    }
    {
        int id = -1;
        lv_obj_t *row = NULL;
        int i;
        for (i = 0; i < g_todo_count; i++)
            if (g_todos[i].status == GW_TODO_ACTIVE) { id = g_todos[i].id; break; }
        if (id > 0) row = g_todo_rows[0].row;
        todo_rebuild_list();
        geom_result(&report, "todos", "incremental_row_reuse",
                    !row || (g_todo_rows[0].row == row),
                    "unchanged task keeps its row widget");
    }
    {
        gw_app_entry_t entries[GW_APP_MAX];
        int count = apps_fetch(entries, GW_APP_MAX);
        int before_children = (int)lv_obj_get_child_cnt(g_apps_list);
        unsigned rebuilds = g_apps_full_rebuilds;
        if (count > 0) {
            gw_ui_show(GW_SCREEN_APPS);
            apps_apply_entries(entries, count - 1);
            geom_result(&report, "apps", "incremental_remove",
                        (int)lv_obj_get_child_cnt(g_apps_list) ==
                            before_children - 1 &&
                        g_apps_full_rebuilds == rebuilds,
                        "removing one app deletes one row, not the list");
            apps_apply_entries(entries, count);
        }
    }
    {
        char list[GW_APP_LIST_MAX];
        char description[128];
        gw_app_entry_t entries[GW_APP_MAX];
        size_t used = 0;
        int i, count;
        memset(description, 'd', sizeof(description) - 1);
        description[sizeof(description) - 1] = '\0';
        for (i = 0; i < GW_APP_MAX; i++) {
            int n;
            if (used >= sizeof(list)) break;
            n = snprintf(list + used, sizeof(list) - used,
                         "APP app-%02d 1.%d.0 %s\n", i, i, description);
            if (n < 0 || (size_t)n >= sizeof(list) - used) break;
            used += (size_t)n;
        }
        if (used < sizeof(list))
            snprintf(list + used, sizeof(list) - used, "APP END\n");
        host_ipc_app_list_set(list);
        count = apps_fetch(entries, GW_APP_MAX);
        host_ipc_app_list_set(NULL);
        geom_result(&report, "apps", "max_list_not_truncated",
                    count == GW_APP_MAX &&
                    !strcmp(entries[GW_APP_MAX - 1].id, "app-31") &&
                    strlen(entries[GW_APP_MAX - 1].description) == 127,
                    "32 maximum-length app rows fit in the batch buffer");
    }
    {
        lv_area_t count_area, row_area;
        lv_obj_get_coords(g_widget_todo_count, &count_area);
        if (g_widget_todo_rows[0])
            lv_obj_get_coords(g_widget_todo_rows[0], &row_area);
        geom_result(&report, "home", "todo_rows_follow_count",
                    g_widget_todo_rows[0] &&
                    row_area.y1 - count_area.y2 <= 80,
                    "home To-dos rows start directly below the count");
    }

    /* ── Screenshots and object-tree checks ────────────────────────────── */
    gw_ui_show(GW_SCREEN_HOME);
    if (dump_screen(outdir, "home", GW_SCREEN_HOME, &report, 1) < 0) return 1;
    check_primary(&report, "home", g_home_screen, 0);
    check_nested_clickable(&report, "home", g_home_screen, 0);

    gw_ui_show(GW_SCREEN_SETTINGS);
    if (dump_screen(outdir, "settings", GW_SCREEN_SETTINGS, &report, 1) < 0) return 1;
    {
        int device = 0, time = 0, zone = 0;
        find_label_text(g_settings_screen, "host-pw3", &device);
        find_label_text(g_settings_screen, "2026-11-03 16:41", &time);
        find_label_text(g_settings_screen, "Asia/Ho_Chi_Minh", &zone);
        geom_result(&report, "settings", "device_time_rows",
                    device && time && zone,
                    "device name, date/time and time zone values are visible");
    }
    check_primary(&report, "settings", g_settings_screen, 0);
    check_nested_clickable(&report, "settings", g_settings_screen, 0);
    check_absent_text(&report, "settings", g_settings_screen, GW_STR_OPEN);
    {
        int clock = 0, battery = 0;
        find_label_text(g_settings_screen, "09:41", &clock);
        find_label_text(g_settings_screen, "82%", &battery);
        geom_result(&report, "settings", "secondary_status_visible",
                    clock && battery, "time and battery stay visible");
    }
    gw_eink_request_scroll(1000);
    geom_result(&report, "settings", "scroll_partial_window",
                gw_eink_scroll_active(),
                "drag sets the fast partial scroll window");
    geom_result(&report, "settings", "scroll_content_flags",
                lv_obj_has_flag(g_settings_content, LV_OBJ_FLAG_SCROLLABLE) &&
                lv_obj_has_flag(g_settings_content, LV_OBJ_FLAG_CLICKABLE),
                "settings content is scrollable and receives drags");
    {
        int before = g_light_value;
        light_plus_cb(NULL);
        geom_result(&report, "settings", "frontlight_step_1",
                    g_light_value == before + 1,
                    "one step changes the frontlight by 1 percent");
        light_minus_cb(NULL);
    }
    geom_result(&report, "settings", "facts_present",
                g_settings_battery_label && g_settings_storage_label &&
                g_settings_version_label && g_settings_uptime_label &&
                g_settings_date_label,
                "Battery/Storage/Version/Uptime/Date rows exist");
    {
        char path[512];
        lv_obj_t *scroll = g_settings_content;
        paint_now(lv_scr_act(), 0);
        if (scroll) {
            lv_obj_scroll_to_y(scroll, lv_obj_get_scroll_bottom(scroll),
                               LV_ANIM_OFF);
            paint_now(lv_scr_act(), 0);
            snprintf(path, sizeof(path), "%s/settings-scrolled.ppm", outdir);
            if (gw_host_fb_save_ppm(path) < 0) return 1;
            geom_walk(&report, "settings-scrolled", lv_scr_act(), lv_scr_act());
            geom_result(&report, "settings", "scroll_reaches_last_row",
                        lv_obj_get_scroll_top(scroll) > 0,
                        "settings content is scrollable to the last row");
            {
                lv_area_t date, view;
                lv_obj_get_coords(g_settings_date_label, &date);
                lv_obj_get_coords(scroll, &view);
                geom_result(&report, "settings",
                            "scrolled_about_device_visible",
                            date.y1 >= view.y1 && date.y2 <= view.y2,
                            "scrolled view shows the About device rows");
            }
        }
    }

    gw_ui_show(GW_SCREEN_CALENDAR);
    if (dump_screen(outdir, "calendar", GW_SCREEN_CALENDAR, &report, 1) < 0) return 1;
    check_primary(&report, "calendar", g_calendar_screen, 0);
    check_nested_clickable(&report, "calendar", g_calendar_screen, 0);
    {
        calendar_month_shift(1);
        if (save_current(outdir, "calendar-next-month", &report) < 0) return 1;
        calendar_month_shift(-1);
        calendar_select_day(31);
        if (save_current(outdir, "calendar-selected", &report) < 0) return 1;
        calendar_select_day(15);
        if (save_current(outdir, "calendar-empty", &report) < 0) return 1;
        calendar_select_day(31);
    }

    gw_ui_show(GW_SCREEN_NOTES);
    if (dump_screen(outdir, "todos", GW_SCREEN_NOTES, &report, 1) < 0) return 1;
    check_primary(&report, "todos", g_notes_screen, 1);
    {
        int i, fit = 1;
        lv_area_t list_area;
        lv_obj_get_coords(g_todo_due_buttons[0], &list_area);
        for (i = 0; i < 5; i++) {
            lv_area_t area;
            if (!g_todo_due_buttons[i]) { fit = 0; continue; }
            lv_obj_get_coords(g_todo_due_buttons[i], &area);
            if (area.x1 < list_area.x1 || area.x2 > GW_UI_PANEL_W - GW_UI_MARGIN)
                fit = 0;
        }
        geom_result(&report, "todos", "due_chips_all_visible", fit,
                    "all five due chips fit the content width");
    }
    geom_result(&report, "todos", "list_scroll_flags",
                lv_obj_has_flag(g_todo_list, LV_OBJ_FLAG_SCROLLABLE) &&
                lv_obj_has_flag(g_todo_list, LV_OBJ_FLAG_CLICKABLE),
                "task list receives drags");
    check_nested_clickable(&report, "todos", g_notes_screen, 0);
    {
        gw_todo_t saved[GW_TODO_MAX];
        int saved_count = g_todo_count;
        memcpy(saved, g_todos, sizeof(saved));
        g_todo_count = 0;
        todo_rebuild_list();
        if (save_current(outdir, "todos-empty", &report) < 0) return 1;
        memcpy(g_todos, saved, sizeof(saved));
        g_todo_count = saved_count;
        todo_rebuild_list();
    }
    g_todo_history = 1;
    todo_filter_style();
    todo_rebuild_list();
    if (save_current(outdir, "todos-history", &report) < 0) return 1;
    g_todo_history = 0;
    todo_filter_style();
    todo_rebuild_list();
    {
        gw_ui_kit_undo_create(GW_STR_TASK_DELETED, GW_STR_UNDO,
                              todo_restore_action, NULL);
        if (save_current(outdir, "todos-undo", &report) < 0) return 1;
        gw_ui_kit_undo_close();
    }

    gw_ui_show(GW_SCREEN_NETWORK);
    g_net_selected_ssid[0] = '\0';
    network_update_ui();
    geom_result(&report, "network", "connect_disabled_without_selection",
                g_net_connect_button &&
                lv_obj_has_state(g_net_connect_button, LV_STATE_DISABLED),
                "Connect is inactive until a row is selected");
    if (dump_screen(outdir, "network", GW_SCREEN_NETWORK, &report, 1) < 0) return 1;
    check_primary(&report, "network", g_network_screen, 0);
    geom_result(&report, "network", "list_scroll_flags",
                lv_obj_has_flag(g_net_list, LV_OBJ_FLAG_SCROLLABLE) &&
                lv_obj_has_flag(g_net_list, LV_OBJ_FLAG_CLICKABLE),
                "network list receives drags");
    check_nested_clickable(&report, "network", g_network_screen, 0);
    {
        int clock = 0, battery = 0;
        find_label_text(g_network_screen, "09:41", &clock);
        find_label_text(g_network_screen, "82%", &battery);
        geom_result(&report, "network", "secondary_status_visible",
                    clock && battery, "time and battery stay visible");
    }
    geom_result(&report, "network", "connection_card_token",
                lv_obj_get_height(g_net_card) ==
                    GW_UI_NETWORK_CONNECTION_CARD_H,
                "connection card uses the layout token");
    {
        int i, rows_ok = 1;
        for (i = 0; i < g_net_ap_count; i++) {
            uint32_t j;
            int found = 0;
            if (!g_net_rows[i]) { rows_ok = 0; continue; }
            for (j = 0; j < lv_obj_get_child_cnt(g_net_rows[i]); j++) {
                lv_obj_t *child = lv_obj_get_child(g_net_rows[i], (int32_t)j);
                if (lv_obj_get_width(child) == 64 &&
                    lv_obj_get_height(child) == 40 &&
                    (int)(intptr_t)lv_obj_get_user_data(child) ==
                        signal_bars_for(g_net_aps[i].signal))
                    found = 1;
            }
            if (!found) rows_ok = 0;
        }
        geom_result(&report, "network", "list_signal_bars",
                    g_net_ap_count > 0 && rows_ok,
                    "every network row has a 4-bar signal widget");
    }
    {
        snprintf(g_net_selected_ssid, sizeof(g_net_selected_ssid), "%s",
                 g_net_ap_count > 0 ? g_net_aps[0].ssid : "");
        g_net_state = 0;
        network_update_ui();
        if (save_current(outdir, "network-password", &report) < 0) return 1;
        g_net_poll_deadline_ms = gw_eink_now_ms() + 15000;
        gw_ui_kit_busy_set(g_net_connect_button, 1, GW_STR_CONNECTING);
        label_set_if_changed(g_net_message, GW_STR_CONNECTING);
        if (save_current(outdir, "network-connecting", &report) < 0) return 1;
        gw_ui_kit_busy_set(g_net_connect_button, 0, NULL);
        g_net_poll_deadline_ms = 0;
        g_net_state = 1;
        network_update_ui();
        if (save_current(outdir, "network-connected", &report) < 0) return 1;
        if (g_net_ap_count > 1)
            snprintf(g_net_selected_ssid, sizeof(g_net_selected_ssid), "%s",
                     g_net_aps[1].ssid);
        network_update_ui();
        if (save_current(outdir, "network-list", &report) < 0) return 1;
        wifi_disconnect(NULL);
        if (save_current(outdir, "network-confirm", &report) < 0) return 1;
        gw_ui_kit_confirm_close();
        {
            int saved = g_net_ap_count;
            g_net_ap_count = 0;
            network_build_rows();
            network_update_ui();
            geom_result(&report, "network",
                        "empty_clears_stale_selection",
                        !g_net_selected_ssid[0] &&
                        g_net_connect_button &&
                        lv_obj_has_state(g_net_connect_button,
                                         LV_STATE_DISABLED),
                        "empty scan drops the selection and disables Connect");
            lv_obj_update_layout(g_network_screen);
            geom_result(&report, "network", "empty_scan_actions",
                        count_label_text(g_network_screen, GW_STR_SCAN, 1) == 1,
                        "the empty state owns the only Scan action");
            geom_result(&report, "network", "empty_select_messages",
                        count_label_text(g_network_screen,
                                         GW_STR_SELECT_NETWORK, 1) == 1,
                        "only the status line says Select a network");
            if (save_current(outdir, "network-empty", &report) < 0) return 1;
            g_net_ap_count = saved;
            network_build_rows();
            network_update_ui();
        }
        g_net_selected_ssid[0] = '\0';
        network_update_ui();
    }

    gw_ui_show(GW_SCREEN_APPS);
    if (dump_screen(outdir, "apps", GW_SCREEN_APPS, &report, 1) < 0) return 1;
    check_primary(&report, "apps", g_apps_screen, 0);
    {
        int legacy = 0;
        find_label_text(g_apps_screen, "App updates", &legacy);
        geom_result(&report, "apps", "single_update_path", !legacy,
                    "update is a section, not a second destination");
        find_label_text(g_apps_screen, "App manager", &legacy);
        geom_result(&report, "apps", "app_manager_title", legacy,
                    "screen is labelled App manager, not Applications");
    }
    geom_result(&report, "apps", "usb_scan_actions",
                count_label_text(g_apps_screen, GW_STR_SCAN_USB, 1) == 1,
                "exactly one USB Scan action");
    geom_result(&report, "apps", "usb_install_actions",
                count_label_text(g_apps_screen, GW_STR_INSTALL_SELECTED, 1) == 1 &&
                count_label_text(g_apps_screen,
                                 GW_STR_INSTALL_UPDATE_USB, 1) == 0,
                "one install-selected action; no install-all action");
    {
        lv_area_t btn, status;
        lv_obj_get_coords(g_update_install_button, &btn);
        lv_obj_get_coords(g_update_status, &status);
        geom_result(&report, "apps", "status_next_to_action",
                    status.y1 >= btn.y2 && status.y1 - btn.y2 <= 48,
                    "update result sits directly under Scan/Install");
    }
    geom_result(&report, "apps", "list_scroll_flags",
                lv_obj_has_flag(g_apps_list, LV_OBJ_FLAG_SCROLLABLE) &&
                lv_obj_has_flag(g_apps_list, LV_OBJ_FLAG_CLICKABLE),
                "application list receives drags");
    check_nested_clickable(&report, "apps", g_apps_screen, 0);
    {
        int clock = 0, battery = 0;
        find_label_text(g_apps_screen, "09:41", &clock);
        find_label_text(g_apps_screen, "82%", &battery);
        geom_result(&report, "apps", "secondary_status_visible",
                    clock && battery, "time and battery stay visible");
    }
    {
        gw_app_entry_t saved[GW_APP_MAX];
        int saved_count = g_app_count;
        memcpy(saved, g_app_entries, sizeof(saved));
        apps_apply_entries(NULL, 0);
        lv_obj_update_layout(g_apps_screen);
        geom_result(&report, "apps", "empty_hides_sticky_actions",
                    g_apps_action_row &&
                    lv_obj_has_flag(g_apps_action_row, LV_OBJ_FLAG_HIDDEN),
                    "empty state owns the single row action");
        geom_result(&report, "apps", "empty_one_refresh",
                    count_label_text(g_apps_screen, GW_STR_REFRESH, 1) == 1,
                    "empty state exposes exactly one Refresh action");
        {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "primary=%d vis=%d bg=%d opa=%d dis=%d bgdis=%d bgpr=%d",
                     count_primary(g_apps_screen),
                     lv_obj_is_visible(g_update_install_button),
                     lv_color_to8(lv_obj_get_style_bg_color(
                         g_update_install_button, LV_PART_MAIN)),
                     lv_obj_get_style_bg_opa(g_update_install_button,
                                             LV_PART_MAIN),
                     lv_obj_has_state(g_update_install_button,
                                      LV_STATE_DISABLED),
                     lv_color_to8(lv_obj_get_style_bg_color(
                         g_update_install_button,
                         LV_PART_MAIN | LV_STATE_DISABLED)),
                     lv_color_to8(lv_obj_get_style_bg_color(
                         g_update_install_button,
                         LV_PART_MAIN | LV_STATE_PRESSED)));
            {
                lv_obj_t *lbl = g_update_install_button
                    ? lv_obj_get_child(g_update_install_button, 0) : NULL;
                if (lbl) {
                    size_t n = strlen(detail);
                    snprintf(detail + n, sizeof(detail) - n, " label=%s",
                             lv_label_get_text(lbl));
                }
            }
            geom_result(&report, "apps", "empty_one_primary",
                        count_primary(g_apps_screen) == 1, detail);
        }
        if (save_current(outdir, "apps-empty", &report) < 0) return 1;
        apps_apply_entries(saved, saved_count);
    }
    {
        int rows = g_apps_list ? lv_obj_get_child_cnt(g_apps_list) : 0;
        unsigned rebuilds = g_apps_full_rebuilds;
        unsigned updates = g_apps_row_updates;
        geom_result(&report, "apps", "long_list_rows",
                    g_app_count == 8 && rows >= 8,
                    "eight app rows are built");
        apps_refresh();
        geom_result(&report, "apps", "incremental_unchanged",
                    g_apps_full_rebuilds == rebuilds &&
                    g_apps_row_updates == updates,
                    "unchanged refresh does not rebuild the panel");
        {
            gw_app_entry_t entries[GW_APP_MAX];
            int count = apps_fetch(entries, GW_APP_MAX);
            snprintf(entries[0].description, sizeof(entries[0].description),
                     "%s", "Changed row fixture");
            apps_apply_entries(entries, count);
            geom_result(&report, "apps", "single_row_update",
                        g_apps_full_rebuilds == rebuilds &&
                        g_apps_row_updates == updates + 1,
                        "one changed row updates without a panel rebuild");
        }
        if (rows > 0) {
            lv_obj_t *last = lv_obj_get_child(g_apps_list, rows - 1);
            lv_obj_scroll_to_view(last, LV_ANIM_OFF);
            geom_result(&report, "apps", "long_list_last_row_reachable",
                        last != NULL, "last row is scrollable into view");
            geom_result(&report, "apps", "partial_scroll_row",
                        lv_obj_get_scroll_top(g_apps_list) > 0 &&
                        last != NULL,
                        "scrolling moves the viewport to a partial row");
            if (save_current(outdir, "apps-long", &report) < 0) return 1;
        }
    }
    {
        gw_ui_kit_confirm_create(GW_STR_REMOVE_TITLE, GW_STR_REMOVE_MESSAGE,
                                 GW_STR_REMOVE, apps_remove_action,
                                 (void *)(intptr_t)0);
        if (save_current(outdir, "confirm-remove", &report) < 0) return 1;
        gw_ui_kit_confirm_close();
    }

    gw_ui_show(GW_SCREEN_HUB);
    if (save_current(outdir, "hub", &report) < 0) return 1;
    {
        lv_area_t img, exit_btn;
        lv_obj_get_coords(g_hub_img, &img);
        lv_obj_get_coords(g_hub_exit, &exit_btn);
        geom_result(&report, "hub", "full_panel_image",
                    img.x1 == 0 && img.y1 == 0 &&
                    img.x2 == GW_UI_PANEL_W - 1 &&
                    img.y2 == GW_UI_PANEL_H - 1,
                    "frame covers the whole panel");
        geom_result(&report, "hub", "exit_target",
                    lv_obj_get_width(g_hub_exit) >= GW_UI_TOUCH_MIN &&
                    lv_obj_get_height(g_hub_exit) >= GW_UI_TOUCH_MIN,
                    "exit target is at least 96x96");
        geom_result(&report, "hub", "waiting_state",
                    !gw_hub_has_frame(),
                    "no frame yet -> explicit waiting state");
        (void)exit_btn;
    }

    {
        char resp[64];
        snprintf(resp, sizeof(resp), "stale");
        host_ipc_fail_next("STATUS");
        geom_result(&report, "ipc", "failure_clears_response",
                    ipc_cmd("STATUS", resp, sizeof(resp)) != 0 && resp[0] == '\0',
                    "failed IPC must not leave stale response text");
    }
    {
        /* Deleting an earlier app shifts indices; surviving rows must still
         * resolve to the same app ID. */
        apps_refresh();
        if (g_app_count > 2) {
            char survived[64];
            int i, found = 0;
            snprintf(survived, sizeof(survived), "%s", g_app_row_ids[1]);
            apps_delete_slot(0);
            for (i = 0; i < g_app_count; i++)
                if (g_app_row_ids[i] &&
                    !strcmp(g_app_row_ids[i], survived) &&
                    lv_obj_get_user_data(g_app_rows[i]) == g_app_row_ids[i])
                    found = 1;
            geom_result(&report, "apps", "delete_reindexes_rows",
                        found && apps_slot_for(survived) >= 0,
                        "row identity follows the app ID after delete");
        }
        apps_refresh();
    }
    {
        /* Selection follows the SSID, not a row index, when a scan reorders
         * the list. */
        if (g_net_ap_count > 1) {
            gw_net_ap_t swap = g_net_aps[0];
            snprintf(g_net_selected_ssid, sizeof(g_net_selected_ssid), "%s",
                     g_net_aps[1].ssid);
            g_net_aps[0] = g_net_aps[1];
            g_net_aps[1] = swap;
            network_build_rows();
            network_update_ui();
            {
                int i, selected_rows = 0, wrong_rows = 0;
                for (i = 0; i < g_net_row_count; i++) {
                    int selected = !strcmp(g_net_row_ssid[i],
                                           g_net_selected_ssid);
                    int width = lv_obj_get_style_border_width(g_net_rows[i],
                                                              LV_PART_MAIN);
                    if (selected && width == 3) selected_rows++;
                    if (!selected && width == 3) wrong_rows++;
                }
                geom_result(&report, "network", "selection_follows_ssid",
                            selected_rows == 1 && wrong_rows == 0,
                            "reordered scan keeps the selected SSID highlighted");
            }
        }
    }
    {
        /* Adding a todo must repaint Home without a manual apply_model. */
        char saved[128];
        snprintf(saved, sizeof(saved), "%s", g_widget_todo_count
                     ? lv_label_get_text(g_widget_todo_count) : "");
        if (todo_add("Home refresh regression", "") == 0) {
            todo_commit();
            geom_result(&report, "todos", "home_refreshes_after_commit",
                        g_widget_todo_count &&
                        strcmp(saved, lv_label_get_text(g_widget_todo_count)) != 0 &&
                        strstr(lv_label_get_text(g_widget_todo_count), "active") != NULL,
                        "todo commit repaints Home with the new count");
            g_todo_count--;
            g_todo_next_id--;
        }
        apply_model();
    }
    {
        /* Scan timeout releases the busy control and reports failure. */
        g_wifi_scan_pending = 1;
        g_wifi_scan_started_ms = gw_eink_now_ms() - 20000;
        if (g_net_scan_button) gw_ui_kit_busy_set(g_net_scan_button, 1, "...");
        /* A stuck scan must time out even when STATUS is unreachable. */
        host_ipc_fail_next("STATUS");
        ui_tick_cb(NULL);
        geom_result(&report, "network", "scan_timeout_clears_busy",
                    !g_wifi_scan_pending &&
                    (g_net_scan_button == NULL ||
                     !gw_ui_kit_button_is_busy(g_net_scan_button)),
                    "hard scan timeout clears pending and busy state");
        g_net_poll_deadline_ms = gw_eink_now_ms() - 1;
        if (g_net_connect_button) gw_ui_kit_busy_set(g_net_connect_button, 1, "...");
        g_net_state = 0;
        ui_tick_cb(NULL);
        geom_result(&report, "network", "connect_timeout_clears_busy",
                    (g_net_connect_button == NULL ||
                     !gw_ui_kit_button_is_busy(g_net_connect_button)),
                    "connect timeout clears busy and reports failure");
    }


    gw_ui_show(GW_SCREEN_LOCK);
    if (save_current(outdir, "lock", &report) < 0) return 1;
    check_primary(&report, "lock", g_lock_screen, 0);
    geom_result(&report, "lock", "whole_screen_unlock_target",
                has_clickable(g_lock_screen),
                "the lock surface itself is the unlock target");
    {
        int tap = 0, locked = 0;
        find_label_text(g_lock_screen, GW_STR_TAP_TO_UNLOCK, &tap);
        find_label_text(g_lock_screen, GW_STR_LOCKED, &locked);
        geom_result(&report, "lock", "one_unlock_affordance",
                    tap && !locked,
                    "only Tap to unlock remains");
    }
    {
        int back = 0;
        find_label_text(g_lock_screen, "Back", &back);
        geom_result(&report, "lock", "no_back", !back,
                    "Lock has no Back control");
    }
    {
        int clock = 0, date = 0, message = 0;
        find_label_text(g_lock_screen, "09:41", &clock);
        find_label_text(g_lock_screen, "October", &date);
        find_label_text(g_lock_screen, "message", &message);
        geom_result(&report, "lock", "clock_and_date_visible",
                    clock && date, "clock and date are the default content");
        geom_result(&report, "lock", "message_hidden_by_default",
                    !message, "no message is shown until configured");
        geom_result(&report, "lock", "card_present",
                    g_lock_panel && lv_obj_is_visible(g_lock_panel) &&
                    lv_obj_get_width(g_lock_panel) > 0,
                    "text sits on a readable card over an optional image");
    }

    /* Kit-only preview: every visual state in one page. */
    {
        lv_obj_t *kit = lv_obj_create(NULL);
        lv_obj_t *col;
        lv_obj_t *b1, *b2;
        gw_ui_style_screen(kit);
        col = lv_obj_create(kit);
        lv_obj_set_size(col, GW_UI_CONTENT_W, 1200);
        lv_obj_align(col, LV_ALIGN_TOP_MID, 0, 48);
        gw_ui_style_plain_panel(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(col, GW_UI_SPACE_2, LV_PART_MAIN);
        gw_ui_label(col, "Kit states", &gw_font_44_bold, gw_ui_c_text());
        b1 = gw_ui_kit_button_create(col, "Primary", 300, 96,
                                     GW_UI_BUTTON_PRIMARY);
        b2 = gw_ui_kit_button_create(col, "Secondary", 300, 96,
                                     GW_UI_BUTTON_SECONDARY);
        {
            lv_obj_t *pressed = gw_ui_kit_button_create(col, "Pressed", 300, 96,
                                                        GW_UI_BUTTON_SECONDARY);
            lv_obj_add_state(pressed, LV_STATE_PRESSED);
        }
        {
            lv_obj_t *disabled = gw_ui_kit_button_create(col, "Disabled", 300,
                                                         96,
                                                         GW_UI_BUTTON_SECONDARY);
            lv_obj_add_state(disabled, LV_STATE_DISABLED);
        }
        {
            lv_obj_t *busy = gw_ui_kit_button_create(col, "Busy", 300, 96,
                                                     GW_UI_BUTTON_PRIMARY);
            gw_ui_kit_busy_set(busy, 1, "Scanning...");
        }
        {
            lv_obj_t *row = lv_obj_create(col);
            lv_obj_set_size(row, LV_SIZE_CONTENT, 96);
            gw_ui_style_plain_panel(row);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
            gw_ui_kit_checkbox_create(row, 1, NULL, NULL);
            gw_ui_kit_checkbox_create(row, 0, NULL, NULL);
            gw_ui_kit_signal_bars_create(row, 3);
        }
        (void)b1;
        (void)b2;
        lv_scr_load(kit);
        if (save_current(outdir, "kit", &report) < 0) return 1;
        gw_ui_show(GW_SCREEN_HOME);
        lv_obj_del(kit);
    }

    if (report.json) {
        fprintf(report.json, "\n  ],\n  \"total\": %d,\n  \"failed\": %d\n}\n",
                report.total, report.failed);
        fclose(report.json);
    }

    printf("geometry checks: %d total, %d failed\n", report.total, report.failed);
    printf("screenshots: %s/*.ppm\n", outdir);
    return report.failed ? 1 : 0;
}

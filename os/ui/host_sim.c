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
#define GW_UI_HOST_SIM 1
#define main gw_shell_unused_main
#include "gw-shell.c"
#undef main

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

static void geom_walk(geom_report_t *r, const char *screen, lv_obj_t *obj,
                      lv_obj_t *root) {
    uint32_t i;
    if (!obj) return;
    if (obj != root && lv_obj_is_visible(obj)) {
        lv_area_t a;
        lv_obj_get_coords(obj, &a);
        {
            int pass = a.x1 >= 0 && a.y1 >= 0 &&
                       a.x2 < GW_UI_PANEL_W && a.y2 < GW_UI_PANEL_H;
            char detail[160];
            snprintf(detail, sizeof(detail), "coords=(%d,%d)-(%d,%d)",
                     a.x1, a.y1, a.x2, a.y2);
            geom_result(r, screen, "inside_screen", pass, detail);
        }
        /* Check real controls.  Plain LVGL containers are clickable by
         * default but are not touch targets. */
        if (!lv_obj_check_type(obj, &lv_dropdownlist_class) && (lv_obj_check_type(obj, &lv_btn_class) ||
            lv_obj_check_type(obj, &lv_slider_class) ||
            lv_obj_check_type(obj, &lv_textarea_class) ||
            lv_obj_check_type(obj, &lv_dropdown_class) ||
            lv_obj_check_type(obj, &lv_keyboard_class))) {
            lv_coord_t w = lv_obj_get_width(obj);
            lv_coord_t h = lv_obj_get_height(obj);
            int pass = w >= GW_UI_TOUCH_MIN && h >= GW_UI_TOUCH_MIN;
            char detail[160];
            snprintf(detail, sizeof(detail), "touch=%d x %d class=%s", w, h, obj_class_name(obj));
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

static void check_home_geometry(geom_report_t *r) {
    lv_area_t hero, date, reading, calendar, notes, quick0, quick3;
    char detail[192];
    lv_obj_get_coords(g_hero_title, &hero);
    lv_obj_get_coords(g_date_label, &date);
    lv_obj_get_coords(g_home_reading_card, &reading);
    lv_obj_get_coords(g_home_calendar_card, &calendar);
    lv_obj_get_coords(g_home_notes_card, &notes);
    lv_obj_get_coords(g_quick_tiles[0], &quick0);
    lv_obj_get_coords(g_quick_tiles[3], &quick3);

    geom_result(r, "home", "status_below_hero",
                GW_UI_STATUS_BAR_H <= hero.y1, "status bar ends before hero");
    snprintf(detail, sizeof(detail), "hero_y=%d..%d date_y=%d..%d", hero.y1, hero.y2, date.y1, date.y2);
    geom_result(r, "home", "hero_before_date", hero.y2 < date.y1, detail);
    geom_result(r, "home", "date_before_reading", date.y2 < reading.y1, "date vs continue");
    geom_result(r, "home", "reading_before_widgets", reading.y2 < calendar.y1, "continue vs widgets");
    check_pair(r, "home", "widgets_side_by_side", g_home_calendar_card,
               g_home_notes_card, 0);
    geom_result(r, "home", "widgets_same_height",
                calendar.y1 == notes.y1 && calendar.y2 == notes.y2,
                "equal widget band");
    geom_result(r, "home", "widgets_before_quick",
                calendar.y2 < quick0.y1 && notes.y2 < quick0.y1, "widgets vs quick");
    geom_result(r, "home", "quick_before_nav",
                quick0.y2 < GW_UI_PANEL_H - GW_UI_NAV_BAR_H,
                "quick access vs bottom nav");
    check_margin(r, "home", "reading_full_margin", g_home_reading_card,
                 GW_UI_MARGIN, GW_UI_PANEL_W - GW_UI_MARGIN - 1);
    check_margin(r, "home", "calendar_left_margin", g_home_calendar_card,
                 GW_UI_MARGIN, -1);
    check_margin(r, "home", "notes_right_margin", g_home_notes_card,
                 -1, GW_UI_PANEL_W - GW_UI_MARGIN - 1);
    check_margin(r, "home", "quick_first_margin", g_quick_tiles[0],
                 GW_UI_MARGIN, -1);
    check_margin(r, "home", "quick_last_margin", g_quick_tiles[3],
                 -1, GW_UI_PANEL_W - GW_UI_MARGIN - 1);
    geom_result(r, "home", "widgets_equal_width",
                lv_obj_get_width(g_home_calendar_card) ==
                lv_obj_get_width(g_home_notes_card), "calendar/notes width");
    geom_result(r, "home", "quick_equal_width",
                lv_obj_get_width(g_quick_tiles[0]) ==
                lv_obj_get_width(g_quick_tiles[3]), "quick tile width");
    geom_result(r, "home", "quick_gap",
                g_quick_tiles[1] && lv_obj_get_x(g_quick_tiles[1]) -
                (lv_obj_get_x(g_quick_tiles[0]) +
                 lv_obj_get_width(g_quick_tiles[0])) == GW_UI_GAP,
                "24 px gap between shortcuts");
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
        if (a.y1 >= GW_UI_PANEL_H - GW_UI_NAV_BAR_H) continue;  /* bottom nav */
        geom_result(r, name, "content_below_appbar",
                    a.y1 >= GW_UI_APP_BAR_H, "content starts below app bar");
        geom_result(r, name, "content_above_nav",
                    a.y2 <= GW_UI_PANEL_H - GW_UI_NAV_BAR_H,
                    "content ends above bottom nav");
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

int main(int argc, char **argv) {
    const char *outdir = argc > 1 ? argv[1] : "/tmp/gw-ui-preview";
    char json_path[512];
    geom_report_t report;
    mkdir(outdir, 0755);

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
    if (report.json) fprintf(report.json, "{\n  \"checks\": [");

    /* TODO store state transitions, with the mock list restored afterward. */
    {
        gw_todo_t saved[GW_TODO_MAX];
        int saved_count = g_todo_count;
        int saved_next = g_todo_next_id;
        int saved_history = g_todo_history;
        int before = g_todo_count;
        int rc, id;
        memcpy(saved, g_todos, sizeof(saved));
        rc = todo_add("Host test task");
        id = (rc == 0) ? g_todos[g_todo_count - 1].id : -1;
        geom_result(&report, "todos", "store_add",
                    rc == 0 && g_todo_count == before + 1 && id > 0,
                    "new task becomes active");
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
        g_todo_history = 1;
        todo_rebuild_list();
        geom_result(&report, "todos", "history_view",
                    lv_obj_get_child_cnt(g_todo_list) > 0,
                    "history view builds completed/deleted rows");
        memcpy(g_todos, saved, sizeof(saved));
        g_todo_count = saved_count;
        g_todo_next_id = saved_next;
        g_todo_history = saved_history;
        todo_rebuild_list();
        todo_preview_refresh();
    }

    /* Navigation state machine. */
    check_state(&report, GW_SCREEN_HOME, g_home_screen);
    check_state(&report, GW_SCREEN_SETTINGS, g_settings_screen);
    check_state(&report, GW_SCREEN_CALENDAR, g_calendar_screen);
    check_state(&report, GW_SCREEN_NOTES, g_notes_screen);
    check_state(&report, GW_SCREEN_NETWORK, g_network_screen);
    check_state(&report, GW_SCREEN_APPS, g_apps_screen);
    check_state(&report, GW_SCREEN_UPDATE, g_update_screen);
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
    check_callback(&report, "update_open_cb", update_open_cb,
                   GW_SCREEN_UPDATE, g_update_screen);
    check_callback(&report, "nav_home_cb", nav_home_cb,
                   GW_SCREEN_HOME, g_home_screen);
    reader_launch(NULL);
    geom_result(&report, "home", "reader_callback_host_safe",
                g_exit_for_reader == 0, "host preview does not exit on launch");

    /* Keyboard typing policy: no delayed password reveal, no blinking
     * cursor, no dark pressed-state inversion. */
    geom_result(&report, "network", "password_no_delayed_reveal",
                lv_textarea_get_password_show_time(g_wifi_password) == 0,
                "password characters hide immediately");
    geom_result(&report, "network", "password_cursor_no_blink",
                lv_obj_get_style_anim_time(g_wifi_password, LV_PART_CURSOR) == 0,
                "cursor anim_time is zero");
    {
        lv_style_value_t v;
        unsigned pressed = 0, wanted = lv_color_to8(gw_ui_c_text());
        int found = lv_obj_get_local_style_prop(
                        g_wifi_keyboard, LV_STYLE_BG_COLOR, &v,
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
                        g_wifi_keyboard, LV_STYLE_COLOR_FILTER_OPA, &v,
                        LV_PART_ITEMS | LV_STATE_PRESSED) == LV_STYLE_RES_FOUND;
        geom_result(&report, "network", "keyboard_press_no_filter",
                    found && v.num == LV_OPA_TRANSP,
                    "theme darkening filter disabled");
    }
    {
        lv_font_glyph_dsc_t d;
        int symbols = lv_font_get_glyph_dsc(&gw_font_28, &d, 0xF55A, 0) &&
                      lv_font_get_glyph_dsc(&gw_font_28, &d, 0xF8A2, 0) &&
                      lv_font_get_glyph_dsc(&gw_font_28, &d, 0xF00C, 0) &&
                      lv_font_get_glyph_dsc(&gw_font_28, &d, 0xF11C, 0);
        geom_result(&report, "network", "keyboard_symbol_glyphs", symbols,
                    "backspace/enter/check/keyboard glyphs present");
    }
    geom_result(&report, "todos", "todos_cursor_no_blink",
                lv_obj_get_style_anim_time(g_notes_area, LV_PART_CURSOR) == 0,
                "cursor anim_time is zero");
#if GW_UI_EINK_FAST_MODE
    geom_result(&report, "network", "typing_fast_mode_enabled", 1,
                "text input uses DU with deferred GC16 cleanup");
#endif

    /* Empty / loading / populated states.  These call the same label update
     * path used on the device, not a separate mock renderer. */
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
        geom_result(&report, "home", "empty_reading_text",
                    !strcmp(lv_label_get_text(
                                lv_obj_get_child(g_reading_empty, 0)),
                            "No book open"),
                    "explicit no-book message");
        /* Render the empty card too so its button/labels pass the same
         * clipping and touch-target checks as the populated state. */
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
                    lv_obj_has_flag(g_reading_empty, LV_OBJ_FLAG_HIDDEN) &&
                    !lv_obj_has_flag(g_reading_body, LV_OBJ_FLAG_HIDDEN),
                    "reading body restored");
    }
    {
        char saved[sizeof(g_model.note_preview)];
        snprintf(saved, sizeof(saved), "%s", g_model.note_preview);
        g_model.note_preview[0] = '\0';
        apply_model();
        geom_result(&report, "home", "empty_notes_state",
                    !strcmp(lv_label_get_text(g_widget_note_label), "No tasks yet."),
                    "explicit no-tasks message");
        snprintf(g_model.note_preview, sizeof(g_model.note_preview), "%s", saved);
        apply_model();
    }
    {
        g_wifi_scan_pending = 1;
        status_update(NULL);
        geom_result(&report, "network", "scan_loading_result",
                    strstr(lv_dropdown_get_options(g_wifi_list), "Home Wi-Fi") != NULL,
                    "mock scan result populated dropdown");
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

    /* Screenshots and object-tree checks. */
    gw_ui_show(GW_SCREEN_HOME);
    if (dump_screen(outdir, "home", GW_SCREEN_HOME, &report, 1) < 0) return 1;
    gw_ui_show(GW_SCREEN_SETTINGS);
    if (dump_screen(outdir, "settings", GW_SCREEN_SETTINGS, &report, 1) < 0) return 1;
    gw_ui_show(GW_SCREEN_CALENDAR);
    if (dump_screen(outdir, "calendar", GW_SCREEN_CALENDAR, &report, 1) < 0) return 1;
    gw_ui_show(GW_SCREEN_NOTES);
    if (dump_screen(outdir, "todos", GW_SCREEN_NOTES, &report, 1) < 0) return 1;
    g_todo_history = 1;
    todo_filter_style();
    todo_rebuild_list();
    {
        char path[512];
        paint_now(lv_scr_act(), 0);
        snprintf(path, sizeof(path), "%s/todos-history.ppm", outdir);
        if (gw_host_fb_save_ppm(path) < 0) return 1;
    }
    g_todo_history = 0;
    todo_filter_style();
    todo_rebuild_list();
    gw_ui_show(GW_SCREEN_NETWORK);
    if (dump_screen(outdir, "network", GW_SCREEN_NETWORK, &report, 1) < 0) return 1;
    gw_ui_show(GW_SCREEN_APPS);
    if (dump_screen(outdir, "apps", GW_SCREEN_APPS, &report, 1) < 0) return 1;
    {
        int rows = g_apps_list ? lv_obj_get_child_cnt(g_apps_list) : 0;
        geom_result(&report, "apps", "long_list_rows",
                    g_app_count == 8 && rows >= 8,
                    "eight app rows are built");
        if (rows > 0) {
            lv_obj_t *last = lv_obj_get_child(g_apps_list, rows - 1);
            lv_obj_scroll_to_view(last, LV_ANIM_OFF);
            paint_now(lv_scr_act(), 0);
            {
                char path[512];
                snprintf(path, sizeof(path), "%s/apps-long.ppm", outdir);
                if (gw_host_fb_save_ppm(path) < 0) return 1;
            }
            geom_result(&report, "apps", "long_list_last_row_reachable",
                        last != NULL, "last row is scrollable into view");
        }
    }
    gw_ui_show(GW_SCREEN_UPDATE);
    if (dump_screen(outdir, "update", GW_SCREEN_UPDATE, &report, 1) < 0) return 1;
    /* Render the dropdown popup path separately; it is not in the normal
     * five-screen acceptance set. */
    lv_dropdown_open(g_wifi_list);
    paint_now(lv_scr_act(), 0);
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/network-popup.ppm", outdir);
        if (gw_host_fb_save_ppm(path) < 0) return 1;
    }
    geom_walk(&report, "network-popup", lv_scr_act(), lv_scr_act());
    lv_dropdown_close(g_wifi_list);
    gw_ui_show(GW_SCREEN_HOME);

    if (report.json) {
        fprintf(report.json, "\n  ],\n  \"total\": %d,\n  \"failed\": %d\n}\n",
                report.total, report.failed);
        fclose(report.json);
    }

    printf("geometry checks: %d total, %d failed\n", report.total, report.failed);
    printf("screenshots: %s/{home,settings,calendar,todos,network,apps,update}.ppm\n",
           outdir);
    return report.failed ? 1 : 0;
}

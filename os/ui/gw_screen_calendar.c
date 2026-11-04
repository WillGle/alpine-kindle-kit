/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_shell_internal.h"

/* ── Calendar screen ────────────────────────────────────────────────────── */

GW_SHELL_INTERNAL void calendar_render(void);
GW_SHELL_INTERNAL void calendar_detail_render(void);

GW_SHELL_INTERNAL void calendar_select_day(int day) {
    g_calendar_selected_day = day;
    calendar_render();
    calendar_detail_render();
}

GW_SHELL_INTERNAL void calendar_day_cb(lv_event_t *event) {
    int day = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    if (day >= 1 && day <= 31) calendar_select_day(day);
}

GW_SHELL_INTERNAL void calendar_month_shift(int delta) {
    int year = g_calendar_year, month = g_calendar_month;
    struct tm t;
    month += delta;
    while (month < 0) { month += 12; year--; }
    while (month > 11) { month -= 12; year++; }
    g_calendar_year = year;
    g_calendar_month = month;
    gw_ui_now(&t);
    if (year == t.tm_year + 1900 && month == t.tm_mon)
        g_calendar_selected_day = t.tm_mday;
    if (!(year == t.tm_year + 1900 && month == t.tm_mon)) {
        int dim = days_in_month(year, month);
        if (g_calendar_selected_day > dim) g_calendar_selected_day = 1;
        if (g_calendar_selected_day < 1) g_calendar_selected_day = 1;
    }
    calendar_render();
    calendar_detail_render();
    /* A month change may cause one full-screen GC16; day selection is a
     * partial update. */
    gw_ui_request_full(NULL);
}

GW_SHELL_INTERNAL void calendar_prev_cb(lv_event_t *event) { (void)event; calendar_month_shift(-1); }
GW_SHELL_INTERNAL void calendar_next_cb(lv_event_t *event) { (void)event; calendar_month_shift(1); }
GW_SHELL_INTERNAL void calendar_today_cb(lv_event_t *event) {
    struct tm t;
    (void)event;
    gw_ui_now(&t);
    g_calendar_year = t.tm_year + 1900;
    g_calendar_month = t.tm_mon;
    g_calendar_selected_day = t.tm_mday;
    calendar_render();
    calendar_detail_render();
    gw_eink_request_scene();
}

GW_SHELL_INTERNAL void calendar_open_todos_cb(lv_event_t *event) {
    (void)event;
    gw_ui_show(GW_SCREEN_NOTES);
}

GW_SHELL_INTERNAL void calendar_render(void) {
    struct tm now;
    int first_wday, dim, cell, i;
    char title[64];
    gw_ui_now(&now);
    format_month_title(g_calendar_year, g_calendar_month, title, sizeof(title));
    label_set_if_changed(g_calendar_title_label, title);
    dim = days_in_month(g_calendar_year, g_calendar_month);
    /* Sunday-first, matching struct tm.tm_wday and the Sun..Sat header. */
    {
        struct tm first;
        memset(&first, 0, sizeof(first));
        first.tm_year = g_calendar_year - 1900;
        first.tm_mon = g_calendar_month;
        first.tm_mday = 1;
        first.tm_hour = 12;
        mktime(&first);
        first_wday = first.tm_wday;
    }
    cell = 1 - first_wday;
    for (i = 0; i < GW_CAL_CELLS; i++, cell++) {
        lv_obj_t *obj = g_calendar_cells[i];
        lv_obj_t *label = g_calendar_day_labels[i];
        lv_obj_t *dot = g_calendar_dots[i];
        int in_month = cell >= 1 && cell <= dim;
        int is_today = in_month && g_calendar_year == now.tm_year + 1900 &&
                       g_calendar_month == now.tm_mon && cell == now.tm_mday;
        int selected = in_month && cell == g_calendar_selected_day;
        char text[16];
        if (in_month) snprintf(text, sizeof(text), "%d", cell);
        else snprintf(text, sizeof(text), " ");
        label_set_if_changed(label, text);
        lv_obj_set_style_text_font(label,
                                   is_today ? &gw_font_32_bold : &gw_font_32,
                                   LV_PART_MAIN);
        lv_obj_set_style_text_color(label,
                                    in_month ? gw_ui_c_text() : gw_ui_c_muted(),
                                    LV_PART_MAIN);
        if (selected) {
            lv_obj_set_style_border_width(obj, 3, LV_PART_MAIN);
            lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL, LV_PART_MAIN);
            lv_obj_set_style_border_color(obj, gw_ui_c_text(), LV_PART_MAIN);
        } else if (is_today) {
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN);
            lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM,
                                         LV_PART_MAIN);
            lv_obj_set_style_border_color(obj, gw_ui_c_text(), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
        }
        if (in_month) set_clickable(obj);
        else lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(obj, (void *)(intptr_t)(in_month ? cell : 0));
        /* Event dots are created lazily so empty months allocate none. */
        if (in_month && todo_due_on(g_calendar_year, g_calendar_month, cell)) {
            if (!dot) {
                dot = lv_obj_create(obj);
                lv_obj_set_size(dot, 8, 8);
                lv_obj_set_style_bg_color(dot, gw_ui_c_text(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(dot, 4, LV_PART_MAIN);
                lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, -8, 8);
                g_calendar_dots[i] = dot;
            }
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
        } else if (dot) {
            lv_obj_del(dot);
            g_calendar_dots[i] = NULL;
        }
    }
}

GW_SHELL_INTERNAL void calendar_detail_ensure(int index) {
    lv_obj_t *detail, *title, *due;
    if (index < 0 || index >= 5 || g_calendar_detail_rows[index]) return;
    detail = lv_btn_create(g_calendar_detail_empty
                               ? lv_obj_get_parent(g_calendar_detail_empty)
                               : g_calendar_screen);
    lv_obj_set_width(detail, LV_PCT(100));
    lv_obj_set_height(detail, 96);
    gw_ui_style_flat(detail);
    lv_obj_set_flex_flow(detail, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(detail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(detail, GW_UI_SPACE_2, LV_PART_MAIN);
    title = gw_ui_label(detail, "", &gw_font_32, gw_ui_c_text());
    due = gw_ui_label(detail, "", &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_add_event_cb(detail, calendar_open_todos_cb, LV_EVENT_CLICKED, NULL);
    if (g_calendar_detail_empty)
        lv_obj_move_to_index(detail,
                             lv_obj_get_index(g_calendar_detail_empty));
    (void)due;
    g_calendar_detail_rows[index] = detail;
}

GW_SHELL_INTERNAL void calendar_detail_render(void) {
    char iso[11];
    int i, shown = 0, total = 0;
    struct tm t;
    gw_ui_now(&t);
    format_iso_date(g_calendar_year, g_calendar_month, g_calendar_selected_day,
                    iso, sizeof(iso));
    for (i = 0; i < g_todo_count; i++) {
        gw_todo_t *todo = &g_todos[i];
        if (todo->status == GW_TODO_ACTIVE && !strcmp(todo->due, iso)) total++;
    }
    {
        /* E01: a non-today selection is not "UPCOMING"; name the day. */
        char heading[48];
        if (g_calendar_year == t.tm_year + 1900 &&
            g_calendar_month == t.tm_mon &&
            g_calendar_selected_day == t.tm_mday) {
            snprintf(heading, sizeof(heading), "%s", GW_STR_DUE_TODAY);
        } else {
            snprintf(heading, sizeof(heading), "DUE %d %s",
                     g_calendar_selected_day,
                     gw_str_month_short[g_calendar_month]);
        }
        label_set_if_changed(g_calendar_detail_title, heading);
    }
    for (i = 0; i < g_todo_count && shown < 5; i++) {
        gw_todo_t *todo = &g_todos[i];
        if (todo->status != GW_TODO_ACTIVE || strcmp(todo->due, iso)) continue;
        calendar_detail_ensure(shown);
        {
            lv_obj_t *row = g_calendar_detail_rows[shown];
            lv_obj_t *title = lv_obj_get_child(row, 0);
            lv_obj_t *due = lv_obj_get_child(row, 1);
            char due_text[32];
            char title_text[GW_TODO_TEXT_MAX + 8];
            container_set_hidden(row, 0);
            snprintf(title_text, sizeof(title_text), "-  %s", todo->text);
            label_set_if_changed(title, title_text);
            /* The selected day is already the heading; do not repeat
             * "Today" on every row. */
            due_text[0] = '\0';
            label_set_if_changed(due, due_text);
            lv_obj_set_user_data(row, (void *)(intptr_t)todo->id);
        }
        shown++;
    }
    for (i = shown; i < 5; i++)
        if (g_calendar_detail_rows[i])
            container_set_hidden(g_calendar_detail_rows[i], 1);
    /* E02/F9: show five rows and report any remaining hidden items. */
    if (g_calendar_detail_more) {
        if (total > shown) {
            char more[32];
            snprintf(more, sizeof(more), GW_STR_MORE_TASKS_FMT, total - shown);
            {
                lv_obj_t *label = lv_obj_get_child(g_calendar_detail_more, 0);
                if (label) label_set_if_changed(label, more);
            }
            container_set_hidden(g_calendar_detail_more, 0);
        } else {
            container_set_hidden(g_calendar_detail_more, 1);
        }
    }
    container_set_hidden(g_calendar_detail_empty, total > 0);
    if (total == 0) {
        /* The existing single system event is shown when it matches today. */
        if (g_calendar_year == t.tm_year + 1900 &&
            g_calendar_month == t.tm_mon && g_calendar_selected_day == t.tm_mday &&
            g_model.event_title[0]) {
            label_set_if_changed(g_calendar_detail_empty, g_model.event_title);
        } else {
            label_set_if_changed(g_calendar_detail_empty, GW_STR_NO_ITEMS_DAY);
        }
    }
}

GW_SHELL_INTERNAL void build_calendar_screen(void) {
    lv_obj_t *content, *row, *line;
    struct tm t;
    int i;
    gw_ui_now(&t);
    g_calendar_year = t.tm_year + 1900;
    g_calendar_month = t.tm_mon;
    g_calendar_selected_day = t.tm_mday;

    g_calendar_screen = make_screen();
    gw_ui_kit_app_bar_create(g_calendar_screen, GW_STR_CALENDAR, gw_back_cb);
    content = gw_ui_kit_scroll_content_create(g_calendar_screen,
                                              GW_UI_APP_BAR_H + GW_UI_GAP,
                                              GW_UI_GAP);
    lv_obj_set_style_pad_row(content, GW_UI_SPACE_1, LV_PART_MAIN);
    attach_scroll_events(content);

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *prev = gw_ui_kit_button_icon_create(row, GW_ICON_CHEVRON_LEFT,
                                                       NULL, 96, 96,
                                                       GW_UI_BUTTON_SECONDARY);
        g_calendar_title_label = gw_ui_label(row, "October 2026",
                                             &gw_font_44_bold, gw_ui_c_text());
        lv_obj_t *next = gw_ui_kit_button_icon_create(row, GW_ICON_CHEVRON_RIGHT,
                                                       NULL, 96, 96,
                                                       GW_UI_BUTTON_SECONDARY);
        lv_obj_t *today = make_button(row, GW_STR_TODAY, 150, 96, 1);
        lv_obj_add_event_cb(prev, calendar_prev_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(next, calendar_next_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(today, calendar_today_cb, LV_EVENT_CLICKED, NULL);
    }

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 48);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    for (i = 0; i < 7; i++) {
        lv_obj_t *label = gw_ui_label(row, gw_str_weekday_short[i], &gw_font_28,
                                      gw_ui_c_muted());
        g_calendar_weekday_labels[i] = label;
        lv_obj_set_flex_grow(label, 1);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
    line = make_hairline(content, GW_UI_CONTENT_W);
    (void)line;

    for (i = 0; i < GW_CAL_ROWS; i++) {
        int d;
        lv_obj_t *grid_row = lv_obj_create(content);
        lv_obj_set_width(grid_row, LV_PCT(100));
        lv_obj_set_height(grid_row, GW_UI_TOUCH_MIN);
        gw_ui_style_plain_panel(grid_row);
        lv_obj_set_flex_flow(grid_row, LV_FLEX_FLOW_ROW);
        for (d = 0; d < GW_CAL_COLS; d++) {
            int index = i * GW_CAL_COLS + d;
            lv_obj_t *cell = lv_btn_create(grid_row);
            lv_obj_t *label;
            lv_obj_set_flex_grow(cell, 1);
            lv_obj_set_height(cell, GW_UI_TOUCH_MIN);
            gw_ui_style_flat(cell);
            label = gw_ui_label(cell, "", &gw_font_32, gw_ui_c_text());
            lv_obj_align(label, LV_ALIGN_CENTER, 0, -6);
            g_calendar_cells[index] = cell;
            g_calendar_day_labels[index] = label;
            g_calendar_dots[index] = NULL;
            lv_obj_add_event_cb(cell, calendar_day_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    line = make_hairline(content, GW_UI_CONTENT_W);
    (void)line;
    g_calendar_detail_title = gw_ui_label(content, GW_STR_UPCOMING,
                                           &gw_font_28, gw_ui_c_muted());
    lv_obj_set_width(g_calendar_detail_title, LV_PCT(100));
    for (i = 0; i < 5; i++) g_calendar_detail_rows[i] = NULL;
    g_calendar_detail_empty = gw_ui_label(content, GW_STR_NO_ITEMS_DAY,
                                           &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(g_calendar_detail_empty, LV_PCT(100));
    lv_obj_set_style_pad_top(g_calendar_detail_empty, GW_UI_SPACE_1,
                             LV_PART_MAIN);
    g_calendar_detail_more = make_button(content, "+0 more", 220,
                                         GW_UI_TOUCH_MIN, 1);
    lv_obj_add_event_cb(g_calendar_detail_more, calendar_open_todos_cb,
                        LV_EVENT_CLICKED, NULL);
    container_set_hidden(g_calendar_detail_more, 1);

    calendar_render();
    calendar_detail_render();
}


/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_shell_internal.h"

/* ── Home: status bar ───────────────────────────────────────────────────── */

GW_SHELL_INTERNAL lv_obj_t *status_target_create(lv_obj_t *bar, const char *icon,
                                      int x, int width, lv_event_cb_t cb) {
    lv_obj_t *button = lv_btn_create(bar);
    lv_obj_set_size(button, width, GW_UI_STATUS_BAR_H);
    lv_obj_align(button, LV_ALIGN_RIGHT_MID, x, 0);
    gw_ui_style_flat(button);
    gw_ui_style_button_feedback(button, 0);
    if (icon && icon[0]) {
        lv_obj_t *icon_label = gw_ui_label(button, icon, &gw_font_28,
                                           gw_ui_c_text());
        lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 12, 0);
    }
    if (cb) lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    return button;
}

GW_SHELL_INTERNAL void build_status_bar(lv_obj_t *scr) {
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_t *line;
    lv_obj_set_size(bar, GW_UI_PANEL_W, GW_UI_STATUS_BAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    gw_ui_style_plain_panel(bar);

    g_clock_label = gw_ui_label(bar, "00:00", &gw_font_36_bold, gw_ui_c_text());
    lv_obj_align(g_clock_label, LV_ALIGN_LEFT_MID, GW_UI_MARGIN, 0);

    /* USB data-link indicator: visible only while the host port reports a
     * data-capable connection (host-class), not for a dumb charger. */
    g_usb_label = gw_ui_label(bar, LV_SYMBOL_USB " USB", &gw_font_28,
                              gw_ui_c_text());
    lv_obj_align(g_usb_label, LV_ALIGN_LEFT_MID, 250, 0);
    container_set_hidden(g_usb_label, 1);

    /* D04/D06: a system tray with consistent icon targets and an 8 px gap
     * between them.  The SSID stays in Network/Settings, not in the tray. */
    g_battery_button = status_target_create(bar, NULL, -48, 112,
                                            nav_settings_cb);
    g_battery_label = gw_ui_label(g_battery_button, "--", &gw_font_28,
                                  gw_ui_c_text());
    lv_obj_center(g_battery_label);

    g_lock_button = status_target_create(bar, GW_ICON_LOCK, -168,
                                         GW_UI_TOUCH_MIN, lock_screen_cb);
    g_light_button = status_target_create(bar, NULL, -280, 128, light_open_cb);
    g_light_status_label = gw_ui_label(g_light_button, "", &gw_font_28,
                                       gw_ui_c_text());
    lv_obj_center(g_light_status_label);
    /* One network indicator only: signal bars, no separate Wi-Fi glyph. */
    g_wifi_button = status_target_create(bar, NULL, -416, GW_UI_TOUCH_MIN,
                                         network_open_cb);
    g_wifi_bars = gw_ui_kit_signal_bars_create(g_wifi_button, 0);
    lv_obj_center(g_wifi_bars);

    line = make_hairline(scr, GW_UI_PANEL_W);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, GW_UI_STATUS_BAR_H - GW_UI_HAIRLINE);
}

/* ── Home: Continue Reading ─────────────────────────────────────────────── */

GW_SHELL_INTERNAL lv_obj_t *home_card(lv_obj_t *parent, lv_event_cb_t cb) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    gw_ui_style_card(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, GW_UI_SPACE_2, LV_PART_MAIN);
    gw_ui_style_card_feedback(card);
    set_clickable(card);
    lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, NULL);
    return card;
}

GW_SHELL_INTERNAL void build_continue_card(lv_obj_t *parent) {
    lv_obj_t *card = home_card(parent, reader_launch);
    lv_obj_t *header, *eyebrow, *action;
    /* Populated card keeps the reading hierarchy; the empty state collapses
     * to GW_UI_HOME_EMPTY_CARD_H in apply_model(). */
    lv_obj_set_height(card, GW_UI_HOME_READING_CARD_H);
    g_home_reading_card = card;

    header = lv_obj_create(card);
    g_reading_header = header;
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, GW_UI_SPACE_6);
    gw_ui_style_plain_panel(header);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    eyebrow = gw_ui_label(header, GW_STR_CONTINUE_READING, &gw_font_28,
                          gw_ui_c_muted());
    lv_obj_set_flex_grow(eyebrow, 1);
    action = gw_ui_label(header, GW_STR_TAP_TO_RESUME, &gw_font_28,
                         gw_ui_c_muted());
    lv_obj_set_style_text_align(action, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    g_reading_body = lv_obj_create(card);
    lv_obj_set_width(g_reading_body, LV_PCT(100));
    lv_obj_set_height(g_reading_body, LV_SIZE_CONTENT);
    gw_ui_style_plain_panel(g_reading_body);
    lv_obj_set_flex_flow(g_reading_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_reading_body, GW_UI_SPACE_1, LV_PART_MAIN);

    g_book_title_label = gw_ui_label(g_reading_body, GW_STR_NO_BOOK_OPEN,
                                     &gw_font_44_bold, gw_ui_c_text());
    lv_obj_set_width(g_book_title_label, LV_PCT(100));
    lv_label_set_long_mode(g_book_title_label, LV_LABEL_LONG_DOT);

    g_book_author_label = gw_ui_label(g_reading_body, "", &gw_font_32,
                                      gw_ui_c_secondary());
    lv_obj_set_width(g_book_author_label, LV_PCT(100));
    lv_label_set_long_mode(g_book_author_label, LV_LABEL_LONG_DOT);

    {
        lv_obj_t *row = lv_obj_create(g_reading_body);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 40);
        gw_ui_style_plain_panel(row);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);

        g_book_bar = lv_bar_create(row);
        lv_obj_set_height(g_book_bar, 12);
        lv_obj_set_flex_grow(g_book_bar, 1);
        lv_bar_set_range(g_book_bar, 0, 100);
        lv_bar_set_value(g_book_bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(g_book_bar, gw_ui_c_white(), LV_PART_MAIN);
        lv_obj_set_style_border_width(g_book_bar, GW_UI_BORDER_W, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_book_bar, gw_ui_c_text(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_book_bar, gw_ui_c_text(), LV_PART_INDICATOR);
        lv_obj_set_style_radius(g_book_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(g_book_bar, 0, LV_PART_INDICATOR);

        g_book_progress_label = gw_ui_label(row, "", &gw_font_28,
                                            gw_ui_c_secondary());
        lv_obj_set_width(g_book_progress_label, 190);
        lv_obj_set_style_text_align(g_book_progress_label, LV_TEXT_ALIGN_RIGHT,
                                    LV_PART_MAIN);
    }

    g_reading_empty = NULL;
}

/* ── Home: Calendar and To-dos cards ───────────────────────────────────── */

GW_SHELL_INTERNAL void build_calendar_widget(lv_obj_t *parent) {
    lv_obj_t *card = home_card(parent, calendar_open_cb);
    lv_obj_t *row, *col, *header;
    int i;
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, GW_UI_HOME_CALENDAR_MIN_H, LV_PART_MAIN);
    g_home_calendar_card = card;

    header = lv_obj_create(card);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, GW_UI_SPACE_6);
    gw_ui_style_plain_panel(header);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *eyebrow = gw_ui_label(header, GW_STR_CALENDAR_EYEBROW,
                                        &gw_font_28, gw_ui_c_muted());
        lv_obj_set_flex_grow(eyebrow, 1);
        gw_ui_label(header, GW_STR_VIEW_CALENDAR, &gw_font_28,
                    gw_ui_c_muted());
    }

    row = lv_obj_create(card);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 118);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_SPACE_3, LV_PART_MAIN);

    g_widget_day_label = gw_ui_label(row, "31", &gw_font_72_bold, gw_ui_c_text());
    col = lv_obj_create(row);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, 100);
    gw_ui_style_plain_panel(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, LV_PART_MAIN);
    g_widget_weekday_label = gw_ui_label(col, "Saturday", &gw_font_32,
                                        gw_ui_c_text());
    lv_obj_set_width(g_widget_weekday_label, LV_PCT(100));
    g_widget_month_label = gw_ui_label(col, "October 2026", &gw_font_28,
                                       gw_ui_c_secondary());
    lv_obj_set_width(g_widget_month_label, LV_PCT(100));

    {
        lv_obj_t *week = lv_obj_create(card);
        lv_obj_set_width(week, LV_PCT(100));
        lv_obj_set_height(week, 48);
        gw_ui_style_plain_panel(week);
        lv_obj_set_flex_flow(week, LV_FLEX_FLOW_ROW);
        for (i = 0; i < 7; i++) {
            g_widget_week_labels[i] = gw_ui_label(week, "", &gw_font_28,
                                                  gw_ui_c_secondary());
            lv_obj_set_flex_grow(g_widget_week_labels[i], 1);
            lv_obj_set_style_text_align(g_widget_week_labels[i],
                                        LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        }
    }

    for (i = 0; i < 2; i++) {
        lv_obj_t *up_row = lv_obj_create(card);
        lv_obj_set_width(up_row, LV_PCT(100));
        lv_obj_set_height(up_row, 72);
        gw_ui_style_plain_panel(up_row);
        lv_obj_set_flex_flow(up_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(up_row, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        g_widget_event_labels[i] = gw_ui_label(up_row, "", &gw_font_32,
                                               gw_ui_c_secondary());
        lv_obj_set_flex_grow(g_widget_event_labels[i], 1);
        lv_obj_set_height(g_widget_event_labels[i], 48);
        lv_label_set_long_mode(g_widget_event_labels[i], LV_LABEL_LONG_DOT);
        g_widget_event_rows[i] = up_row;
        container_set_hidden(up_row, i > 0);
    }

}

GW_SHELL_INTERNAL void build_notes_widget(lv_obj_t *parent) {
    lv_obj_t *card = home_card(parent, notes_open_cb);
    lv_obj_t *header;
    int i;
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, GW_UI_HOME_NOTES_MIN_H, LV_PART_MAIN);
    g_home_notes_card = card;

    header = lv_obj_create(card);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, GW_UI_SPACE_6);
    gw_ui_style_plain_panel(header);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *eyebrow = gw_ui_label(header, GW_STR_TODO_EYEBROW,
                                        &gw_font_28, gw_ui_c_muted());
        lv_obj_set_flex_grow(eyebrow, 1);
        gw_ui_label(header, GW_STR_VIEW_ALL, &gw_font_28, gw_ui_c_muted());
    }
    g_widget_todo_count = gw_ui_label(card, GW_STR_NO_TASKS_YET, &gw_font_28,
                                      gw_ui_c_secondary());
    lv_obj_set_width(g_widget_todo_count, LV_PCT(100));

    for (i = 0; i < 5; i++) g_widget_todo_rows[i] = NULL;
}

GW_SHELL_INTERNAL void build_home_screen(void) {
    lv_obj_t *content, *widgets;
    g_home_screen = lv_scr_act();
    gw_ui_style_screen(g_home_screen);

    build_status_bar(g_home_screen);

    content = make_content(g_home_screen, GW_UI_STATUS_BAR_H + GW_UI_GAP);
    lv_obj_set_style_pad_row(content, GW_UI_SPACE_3, LV_PART_MAIN);

    g_date_label = gw_ui_label(content, GW_STR_DATE_UNAVAILABLE, &gw_font_36,
                               gw_ui_c_secondary());
    lv_obj_set_width(g_date_label, LV_PCT(100));

    build_continue_card(content);

    widgets = lv_obj_create(content);
    lv_obj_set_width(widgets, LV_PCT(100));
    lv_obj_set_height(widgets, LV_SIZE_CONTENT);
    gw_ui_style_plain_panel(widgets);
    lv_obj_set_flex_flow(widgets, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(widgets, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(widgets, GW_UI_GAP, LV_PART_MAIN);
    {
        lv_obj_t *left = lv_obj_create(widgets);
        lv_obj_t *right = lv_obj_create(widgets);
        lv_obj_set_flex_grow(left, 1);
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_height(left, LV_SIZE_CONTENT);
        lv_obj_set_height(right, LV_SIZE_CONTENT);
        gw_ui_style_plain_panel(left);
        gw_ui_style_plain_panel(right);
        build_calendar_widget(left);
        build_notes_widget(right);
    }

    make_bottom_nav(g_home_screen, 0);

}

GW_SHELL_INTERNAL void recovery_set(const char *text) {
    /* Errors and status messages are toasts on the top layer, so they appear
     * on the current screen and disappear without a full repaint. */
    gw_ui_kit_toast_create(text);
}


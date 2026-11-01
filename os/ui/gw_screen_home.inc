
GW_SHELL_INTERNAL void build_calendar_widget(lv_obj_t *parent) {
    lv_obj_t *card = home_card(parent, calendar_open_cb);
    lv_obj_t *row, *col;
    int i;
    lv_obj_set_height(card, LV_PCT(100));
    g_home_calendar_card = card;

    {
        lv_obj_t *eyebrow = gw_ui_label(card, GW_STR_CALENDAR_EYEBROW,
                                        &gw_font_28, gw_ui_c_muted());
        lv_obj_set_width(eyebrow, LV_PCT(100));
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

    {
        lv_obj_t *spacer = lv_obj_create(card);
        lv_obj_set_width(spacer, LV_PCT(100));
        lv_obj_set_flex_grow(spacer, 1);
        gw_ui_style_plain_panel(spacer);
    }
    {
        lv_obj_t *footer = gw_ui_label(card, GW_STR_VIEW_CALENDAR, &gw_font_28,
                                       gw_ui_c_muted());
        lv_obj_set_width(footer, LV_PCT(100));
    }
}

GW_SHELL_INTERNAL void build_notes_widget(lv_obj_t *parent) {
    lv_obj_t *card = home_card(parent, notes_open_cb);
    int i;
    lv_obj_set_height(card, LV_PCT(100));
    g_home_notes_card = card;

    {
        lv_obj_t *eyebrow = gw_ui_label(card, GW_STR_TODO_EYEBROW, &gw_font_28,
                                        gw_ui_c_muted());
        lv_obj_set_width(eyebrow, LV_PCT(100));
    }
    g_widget_todo_count = gw_ui_label(card, GW_STR_NO_TASKS_YET, &gw_font_28,
                                      gw_ui_c_secondary());
    lv_obj_set_width(g_widget_todo_count, LV_PCT(100));

    for (i = 0; i < 5; i++) g_widget_todo_rows[i] = NULL;
    {
        lv_obj_t *footer = gw_ui_label(card, GW_STR_VIEW_ALL, &gw_font_28,
                                       gw_ui_c_muted());
        lv_obj_set_width(footer, LV_PCT(100));
    }
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
    lv_obj_set_flex_grow(widgets, 1);
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
        lv_obj_set_height(left, LV_PCT(100));
        lv_obj_set_height(right, LV_PCT(100));
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

/* ── Settings ───────────────────────────────────────────────────────────── */

GW_SHELL_INTERNAL lv_obj_t *settings_section(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = gw_ui_kit_section_label_create(parent, text);
    lv_obj_set_style_pad_top(label, GW_UI_SPACE_2, LV_PART_MAIN);
    return label;
}

GW_SHELL_INTERNAL lv_obj_t *settings_row(lv_obj_t *parent, const char *title,
                              const char *subtitle, const char *button_text,
                              lv_event_cb_t cb, lv_obj_t **value_out) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *col;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 120);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);

    col = lv_obj_create(row);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, 100);
    gw_ui_style_plain_panel(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, LV_PART_MAIN);
    {
        lv_obj_t *label = gw_ui_label(col, title, &gw_font_36_bold,
                                      gw_ui_c_text());
        lv_obj_set_width(label, LV_PCT(100));
    }
    if (subtitle && subtitle[0]) {
        lv_obj_t *sub = gw_ui_label(col, subtitle, &gw_font_28,
                                    gw_ui_c_secondary());
        lv_obj_set_width(sub, LV_PCT(100));
        lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
        if (value_out) *value_out = sub;
    }

    if (button_text) {
        lv_obj_t *button = make_button(row, button_text, 170, GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    }
    return row;
}

GW_SHELL_INTERNAL lv_obj_t *settings_info_row(lv_obj_t *parent, const char *title,
                                   int height, lv_obj_t **value_out) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *label, *value;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, height);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    label = gw_ui_label(row, title, &gw_font_32, gw_ui_c_text());
    lv_obj_set_flex_grow(label, 1);
    value = gw_ui_label(row, "", &gw_font_32, gw_ui_c_secondary());
    if (value_out) *value_out = value;
    return row;
}

GW_SHELL_INTERNAL void format_size(double bytes, char *buf, size_t size) {
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const double mib = 1024.0 * 1024.0;
    if (bytes >= gib)
        snprintf(buf, size, "%.1f GB", bytes / gib);
    else if (bytes >= mib)
        snprintf(buf, size, "%.0f MB", bytes / mib);
    else
        snprintf(buf, size, "%.0f KB", bytes / 1024.0);
}

GW_SHELL_INTERNAL void format_storage_value(double free_bytes, double total_bytes,
                                 char *buf, size_t size) {
    char free_text[24], total_text[24];
    format_size(free_bytes, free_text, sizeof(free_text));
    format_size(total_bytes, total_text, sizeof(total_text));
    snprintf(buf, size, "%s free of %s", free_text, total_text);
}

GW_SHELL_INTERNAL void settings_storage_value(char *buf, size_t size) {
#if GW_UI_HOST_SIM
    snprintf(buf, size, "20.1 GB free of 32.0 GB");
#else
    /* Settings' "Storage" means user storage, not the read-only OS image.
     * /mnt/us holds documents/app data and is much larger than the rootfs. */
    static const char *const paths[] = { "/mnt/us", "/var/lib/gleworks", "/" };
    size_t i;
    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        struct statvfs vfs;
        double free_bytes, total_bytes;
        if (statvfs(paths[i], &vfs) != 0) continue;
        free_bytes = (double)vfs.f_bavail * (double)vfs.f_frsize;
        total_bytes = (double)vfs.f_blocks * (double)vfs.f_frsize;
        format_storage_value(free_bytes, total_bytes, buf, size);
        return;
    }
    buf[0] = '\0';
#endif
}

GW_SHELL_INTERNAL void settings_uptime_value(char *buf, size_t size) {
#if GW_UI_HOST_SIM
    snprintf(buf, size, "2d 4h");
#else
    FILE *file = fopen("/proc/uptime", "r");
    double seconds = 0;
    if (!file || fscanf(file, "%lf", &seconds) != 1) {
        if (file) fclose(file);
        buf[0] = '\0';
        return;
    }
    fclose(file);
    snprintf(buf, size, "%dd %dh", (int)(seconds / 86400),
             (int)((seconds - (int)(seconds / 86400) * 86400) / 3600));
#endif
}

GW_SHELL_INTERNAL void settings_facts_update(void) {
    char text[64];
    struct tm t;
    settings_storage_value(text, sizeof(text));
    if (text[0]) label_set_if_changed(g_settings_storage_label, text);
    container_set_hidden(g_settings_storage_row, !text[0]);
    settings_uptime_value(text, sizeof(text));
    if (text[0]) label_set_if_changed(g_settings_uptime_label, text);
    container_set_hidden(g_settings_uptime_row, !text[0]);
    label_set_if_changed(g_settings_version_label, GW_STR_OS_VERSION);
    gw_ui_now(&t);
    {
        char date[96];
        snprintf(date, sizeof(date), "%s, %d %s %d", gw_str_weekday[t.tm_wday],
                 t.tm_mday, gw_str_month[t.tm_mon], t.tm_year + 1900);
        label_set_if_changed(g_settings_date_label, date);
    }
}

GW_SHELL_INTERNAL void build_settings_screen(void) {
    lv_obj_t *content, *row;
    g_settings_screen = make_screen();
    gw_ui_kit_title_bar_create(g_settings_screen, GW_STR_SETTINGS);
    make_bottom_nav(g_settings_screen, 4);
    content = gw_ui_kit_scroll_content_create(g_settings_screen,
                                              GW_UI_APP_BAR_H + GW_UI_GAP,
                                              GW_UI_NAV_BAR_H + GW_UI_GAP);
    g_settings_content = content;
    lv_obj_set_style_pad_bottom(content, GW_UI_SPACE_6 + GW_UI_SPACE_1,
                                LV_PART_MAIN);
    lv_obj_set_style_pad_right(content, 24, LV_PART_MAIN);
    lv_obj_set_style_width(content, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(content, gw_ui_c_border(), LV_PART_SCROLLBAR);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    attach_scroll_events(content);

    settings_section(content, GW_STR_READING);
    settings_row(content, GW_STR_LIBRARY_ROW, GW_STR_READER_SUB, GW_STR_OPEN,
                 reader_launch, NULL);

    settings_section(content, GW_STR_DISPLAY);
    row = lv_obj_create(content);
    g_settings_light_row = row;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 120);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_SPACE_2, LV_PART_MAIN);
    {
        lv_obj_t *col = lv_obj_create(row);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_height(col, 100);
        gw_ui_style_plain_panel(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(col, 2, LV_PART_MAIN);
        {
            lv_obj_t *label = gw_ui_label(col, GW_STR_FRONTLIGHT,
                                          &gw_font_36_bold, gw_ui_c_text());
            lv_obj_set_width(label, LV_PCT(100));
        }
        {
            lv_obj_t *label = gw_ui_label(col, GW_STR_BRIGHTNESS, &gw_font_28,
                                          gw_ui_c_secondary());
            lv_obj_set_width(label, LV_PCT(100));
        }
    }
    {
        lv_obj_t *minus = gw_ui_kit_button_icon_create(row, GW_ICON_MINUS, NULL,
                                                       96, GW_UI_TOUCH_MIN,
                                                       GW_UI_BUTTON_SECONDARY);
        lv_obj_t *track = lv_obj_create(row);
        lv_obj_t *plus;
        lv_obj_set_size(track, 200, GW_UI_TOUCH_MIN);
        gw_ui_style_plain_panel(track);
        /* D08: real slider with a visible knob; value is committed on
         * release, not on every drag step. */
        g_light_bar = lv_slider_create(track);
        lv_obj_set_size(g_light_bar, 190, 40);
        lv_obj_set_ext_click_area(g_light_bar, 28);
        lv_obj_center(g_light_bar);
        lv_slider_set_range(g_light_bar, 0, 100);
        lv_slider_set_value(g_light_bar, g_light_value, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(g_light_bar, gw_ui_c_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_light_bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_light_bar, GW_UI_BORDER_W, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_light_bar, gw_ui_c_text(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_light_bar, gw_ui_c_text(), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(g_light_bar, gw_ui_c_white(), LV_PART_KNOB);
        lv_obj_set_style_border_width(g_light_bar, GW_UI_BORDER_W, LV_PART_KNOB);
        lv_obj_set_style_border_color(g_light_bar, gw_ui_c_text(), LV_PART_KNOB);
        lv_obj_set_style_pad_all(g_light_bar, 8, LV_PART_KNOB);
        lv_obj_set_style_radius(g_light_bar, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(g_light_bar, 2, LV_PART_INDICATOR);
        lv_obj_set_style_radius(g_light_bar, 0, LV_PART_KNOB);
        lv_obj_add_event_cb(g_light_bar, light_slider_pressed_cb,
                            LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_light_bar, light_slider_released_cb,
                            LV_EVENT_RELEASED, NULL);
        plus = gw_ui_kit_button_icon_create(row, GW_ICON_PLUS, NULL, 96,
                                            GW_UI_TOUCH_MIN,
                                            GW_UI_BUTTON_SECONDARY);
        g_light_label = gw_ui_label(row, "37%", &gw_font_32, gw_ui_c_text());
        lv_obj_set_width(g_light_label, 86);
        lv_obj_set_style_text_align(g_light_label, LV_TEXT_ALIGN_RIGHT,
                                    LV_PART_MAIN);
        lv_obj_add_event_cb(minus, light_minus_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(minus, light_minus_cb, LV_EVENT_LONG_PRESSED_REPEAT,
                            NULL);
        lv_obj_add_event_cb(plus, light_plus_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(plus, light_plus_cb, LV_EVENT_LONG_PRESSED_REPEAT,
                            NULL);
        light_apply_ui();
    }

    settings_section(content, GW_STR_CONNECTIVITY);
    settings_row(content, GW_STR_WIFI, GW_STR_NETWORKS_CONNECTION, GW_STR_OPEN,
                 network_open_cb, &g_settings_wifi_label);

    settings_section(content, GW_STR_SOFTWARE);
    settings_row(content, GW_STR_APPLICATIONS, GW_STR_APPLICATIONS_SUB,
                 GW_STR_OPEN, apps_open_cb, NULL);
    settings_row(content, GW_STR_APP_UPDATES, GW_STR_APP_UPDATES_SUB,
                 GW_STR_OPEN, update_open_cb, NULL);

    settings_section(content, GW_STR_DEVICE);
    g_settings_battery_row = settings_info_row(content, GW_STR_BATTERY, 72,
                                               &g_settings_battery_label);
    g_settings_storage_row = settings_info_row(content, GW_STR_STORAGE, 72,
                                               &g_settings_storage_label);
    g_settings_uptime_row = settings_info_row(content, GW_STR_UPTIME, 72,
                                              &g_settings_uptime_label);
    settings_info_row(content, GW_STR_VERSION, 72, &g_settings_version_label);
    settings_info_row(content, GW_STR_DATE, 72, &g_settings_date_label);

    settings_section(content, GW_STR_ACTIONS);
    {
        lv_obj_t *lock_button = make_button(content, GW_STR_LOCK_NOW,
                                            GW_UI_CONTENT_W, GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(lock_button, lock_screen_cb, LV_EVENT_CLICKED, NULL);
    }

    settings_facts_update();
}

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


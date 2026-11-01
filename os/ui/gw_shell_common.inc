}
GW_SHELL_INTERNAL void update_open_cb(lv_event_t *event) {
    (void)event;
    gw_open_secondary(GW_SCREEN_UPDATE, g_active_screen);
}
GW_SHELL_INTERNAL void apps_update_open_cb(lv_event_t *event) {
    (void)event;
    gw_open_secondary(GW_SCREEN_UPDATE, GW_SCREEN_APPS);
}

/* ── Common widget builders ─────────────────────────────────────────────── */

GW_SHELL_INTERNAL lv_obj_t *make_screen(void) {
    return gw_ui_kit_page_create();
}

GW_SHELL_INTERNAL lv_obj_t *make_hairline(lv_obj_t *parent, int width) {
    return gw_ui_kit_hairline_create(parent, width);
}

GW_SHELL_INTERNAL lv_obj_t *make_button(lv_obj_t *parent, const char *text, int width,
                             int height, int secondary) {
    return gw_ui_kit_button_create(parent, text, width, height,
                                   secondary ? GW_UI_BUTTON_SECONDARY
                                             : GW_UI_BUTTON_PRIMARY);
}

GW_SHELL_INTERNAL lv_obj_t *make_bottom_nav(lv_obj_t *scr, int active) {
    gw_ui_nav_cbs_t cbs = {
        nav_home_cb, nav_library_cb, nav_calendar_cb, nav_todos_cb,
        nav_settings_cb
    };
    gw_ui_kit_bottom_nav_create(scr, active, &cbs);
    return NULL;
}

GW_SHELL_INTERNAL lv_obj_t *make_content(lv_obj_t *scr, int top) {
    return gw_ui_kit_content_create(scr, top);
}

/* Modal and month-change repaints: one whole-panel GC16_FAST partial.  A GC16
 * here would invert the panel black/white on every confirmation tap. */
GW_SHELL_INTERNAL void gw_ui_request_full(void *user_data) {
    (void)user_data;
    g_full_paint_requests++;
    gw_eink_request_scene();
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
    g_force_full_update = 0;
#endif
}

GW_SHELL_INTERNAL void scroll_activity_cb(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    /* On-release: dragging only marks the intent; the EPDC port paints once
     * when the finger lifts.  RELEASED also covers a tap that scrolled. */
    if (code == LV_EVENT_RELEASED) {
        /* A plain tap inside a scrollable container also emits RELEASED.
         * Only a real drag may request a scroll paint/cleanup; otherwise a
         * brightness tap would schedule a full-screen GC16. */
        if (gw_eink_scroll_was_moved()) {
            lv_obj_t *target = lv_event_get_target(event);
            /* Text-heavy profiles snap to a page boundary: no intermediate
             * glyph positions are ever shown, so there is nothing to smear. */
            if (g_paint_page_snap) {
                int32_t view = lv_obj_get_height(target);
                int32_t y = lv_obj_get_scroll_y(target);
                if (view > 0 && y > 0) {
                    int32_t snap = ((y + view / 2) / view) * view;
                    lv_obj_scroll_to_y(target, snap, LV_ANIM_OFF);
                }
            }
            /* With no live previews there may be no dirty rectangle at all;
             * force the final viewport to be flushed once. */
            if (gw_eink_scroll_preview_max() == 0)
                lv_obj_invalidate(target);
            gw_eink_request_scroll(400);
        }
    } else {
        gw_eink_request_scroll(1500);
    }
}

GW_SHELL_INTERNAL void attach_scroll_events(lv_obj_t *obj) {
    lv_obj_add_event_cb(obj, scroll_activity_cb, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(obj, scroll_activity_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(obj, scroll_activity_cb, LV_EVENT_RELEASED, NULL);
}

GW_SHELL_INTERNAL void set_clickable(lv_obj_t *obj) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

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
    lv_obj_t *eyebrow;
    /* D01: content-based height instead of a fixed 440 px block with dead
     * space below the book metadata. */
    lv_obj_set_height(card, 380);
    g_home_reading_card = card;

    eyebrow = gw_ui_label(card, GW_STR_CONTINUE_READING, &gw_font_28,
                          gw_ui_c_muted());
    lv_obj_set_width(eyebrow, LV_PCT(100));

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

    {
        lv_obj_t *footer = gw_ui_label(g_reading_body, GW_STR_TAP_TO_RESUME,
                                       &gw_font_28, gw_ui_c_muted());
        lv_obj_set_width(footer, LV_PCT(100));
    }

    g_reading_empty = NULL;
}

/* ── Home: Calendar and To-dos cards ───────────────────────────────────── */

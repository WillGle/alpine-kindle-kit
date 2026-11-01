/* ── Lock screen ────────────────────────────────────────────────────────── */

static void lock_refresh_time(void) {
    struct tm t;
    char clock_text[16], date[96];
    gw_ui_now(&t);
    snprintf(clock_text, sizeof(clock_text), "%02d:%02d", t.tm_hour, t.tm_min);
    label_set_if_changed(g_lock_time_label, clock_text);
    label_set_if_changed(g_lock_label, GW_STR_LOCKED);
    snprintf(date, sizeof(date), "%s, %d %s %d", gw_str_weekday[t.tm_wday],
             t.tm_mday, gw_str_month[t.tm_mon], t.tm_year + 1900);
    label_set_if_changed(g_lock_date_label, date);
}

static void build_lock_screen(void) {
    lv_obj_t *button, *hint;
    g_lock_screen = make_screen();
    g_lock_time_label = gw_ui_label(g_lock_screen, "00:00", &gw_font_72_bold,
                                    gw_ui_c_text());
    lv_obj_align(g_lock_time_label, LV_ALIGN_CENTER, 0, -260);
    g_lock_date_label = gw_ui_label(g_lock_screen, "", &gw_font_36,
                                    gw_ui_c_secondary());
    lv_obj_align(g_lock_date_label, LV_ALIGN_CENTER, 0, -170);
    g_lock_label = gw_ui_label(g_lock_screen, GW_STR_LOCKED, &gw_font_44_bold,
                               gw_ui_c_text());
    lv_obj_align(g_lock_label, LV_ALIGN_CENTER, 0, -90);
    /* E06: the whole lock surface is the unlock target, not just the button. */
    gw_ui_style_screen(g_lock_screen);
    lv_obj_add_flag(g_lock_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_lock_screen, unlock_screen_cb, LV_EVENT_CLICKED, NULL);
    hint = gw_ui_label(g_lock_screen, GW_STR_TAP_TO_UNLOCK, &gw_font_32,
                       gw_ui_c_secondary());
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, -10);
    button = make_button(g_lock_screen, GW_STR_UNLOCK, 300, GW_UI_TOUCH_MIN, 0);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 120);
    lv_obj_add_event_cb(button, unlock_screen_cb, LV_EVENT_CLICKED, NULL);
    lock_refresh_time();
}

/* ── Model application and screen switching ─────────────────────────────── */

static void home_todo_ensure_row(int index) {
    lv_obj_t *row;
    if (g_widget_todo_rows[index] || !g_home_notes_card) return;
    row = lv_obj_create(g_home_notes_card);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 72);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_SPACE_2, LV_PART_MAIN);
    g_widget_todo_rows[index] = gw_ui_label(row, "", &gw_font_32,
                                            gw_ui_c_text());
    lv_obj_set_flex_grow(g_widget_todo_rows[index], 1);
    lv_obj_set_height(g_widget_todo_rows[index], 44);
    lv_label_set_long_mode(g_widget_todo_rows[index], LV_LABEL_LONG_DOT);
    /* Rows are appended after the count label and before the footer. */
    lv_obj_move_to_index(row, 2 + index);
}

static void home_todo_remove_row(int index) {
    lv_obj_t *label = g_widget_todo_rows[index];
    if (!label) return;
    lv_obj_del(lv_obj_get_parent(label));
    g_widget_todo_rows[index] = NULL;
}

static void home_empty_ensure(void) {
    lv_obj_t *icon, *title, *hint;
    if (g_reading_empty || !g_home_reading_card) return;
    g_reading_empty = lv_obj_create(g_home_reading_card);
    lv_obj_set_width(g_reading_empty, LV_PCT(100));
    lv_obj_set_flex_grow(g_reading_empty, 1);
    gw_ui_style_plain_panel(g_reading_empty);
    lv_obj_set_flex_flow(g_reading_empty, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_reading_empty, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(g_reading_empty, GW_UI_SPACE_1, LV_PART_MAIN);
    icon = gw_ui_label(g_reading_empty, GW_ICON_BOOK, &gw_font_44_bold,
                       gw_ui_c_muted());
    lv_obj_set_width(icon, LV_PCT(100));
    title = gw_ui_label(g_reading_empty, GW_STR_NO_BOOK_OPEN, &gw_font_44_bold,
                        gw_ui_c_text());
    lv_obj_set_width(title, LV_PCT(100));
    hint = gw_ui_label(g_reading_empty, GW_STR_OPEN_LIBRARY_HINT, &gw_font_32,
                       gw_ui_c_secondary());
    lv_obj_set_width(hint, LV_PCT(100));
}

static void apply_model(void) {
    int has_book = g_model.book_title[0] != '\0';
    struct tm t;
    char text[256];
    int i, shown = 0, active = 0;
    gw_ui_now(&t);

    if (!has_book) home_empty_ensure();
    if (g_reading_body) container_set_hidden(g_reading_body, !has_book);
    if (g_reading_empty) {
        if (has_book) {
            lv_obj_del(g_reading_empty);
            g_reading_empty = NULL;
        } else {
            container_set_hidden(g_reading_empty, 0);
        }
    }
    if (has_book) {
        label_set_if_changed(g_book_title_label, g_model.book_title);
        label_set_if_changed(g_book_author_label,
                             g_model.book_author[0] ? g_model.book_author
                                                    : GW_STR_UNKNOWN_AUTHOR);
        if (g_model.book_progress >= 0 && g_model.book_progress <= 100) {
            snprintf(text, sizeof(text), GW_STR_PROGRESS_FMT,
                     g_model.book_progress);
            label_set_if_changed(g_book_progress_label, text);
            container_set_hidden(g_book_bar, 0);
            lv_bar_set_value(g_book_bar, g_model.book_progress, LV_ANIM_OFF);
        } else {
            label_set_if_changed(g_book_progress_label,
                                 GW_STR_PROGRESS_UNAVAILABLE);
            container_set_hidden(g_book_bar, 1);
        }
    }

    /* Home To-dos card: five active rows plus an overflow line. */
    for (i = 0; i < g_todo_count; i++)
        if (g_todos[i].status == GW_TODO_ACTIVE) active++;
    for (i = 0; i < g_todo_count && shown < 5; i++) {
        lv_obj_t *label;
        if (g_todos[i].status != GW_TODO_ACTIVE) continue;
        home_todo_ensure_row(shown);
        label = g_widget_todo_rows[shown];
        if (label) {
            if (g_todos[i].due[0]) {
                char due[32];
                format_due_label(g_todos[i].due, due, sizeof(due));
                snprintf(text, sizeof(text), "-  %s  |  %s",
                         g_todos[i].text, due);
            } else {
                snprintf(text, sizeof(text), "-  %s", g_todos[i].text);
            }
            label_set_if_changed(label, text);
            container_set_hidden(lv_obj_get_parent(label), 0);
        }
        shown++;
    }
    for (i = shown; i < 5; i++) home_todo_remove_row(i);
    if (active > 0) {
        if (active > shown)
            snprintf(text, sizeof(text), "%d active  |  " GW_STR_MORE_FMT,
                     active, active - shown);
        else
            snprintf(text, sizeof(text), GW_STR_ACTIVE_FMT, active);
        label_set_if_changed(g_widget_todo_count, text);
    } else {
        label_set_if_changed(g_widget_todo_count, GW_STR_NO_TASKS_YET);

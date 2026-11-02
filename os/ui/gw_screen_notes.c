/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_shell_internal.h"

/* ── To-dos ─────────────────────────────────────────────────────────────── */

GW_SHELL_INTERNAL void todo_due_style(void) {
    int i;
    for (i = 0; i < 5; i++) {
        lv_obj_t *button = g_todo_due_buttons[i];
        lv_obj_t *label;
        if (!button) continue;
        label = lv_obj_get_child(button, 0);
        if (!label) continue;
        lv_obj_set_style_text_font(label, &gw_font_28, LV_PART_MAIN);
        lv_obj_set_style_text_decor(label,
                                    i == g_todo_due_index
                                        ? LV_TEXT_DECOR_UNDERLINE
                                        : LV_TEXT_DECOR_NONE,
                                    LV_PART_MAIN);
    }
}

GW_SHELL_INTERNAL void todo_due_cb(lv_event_t *event) {
    g_todo_due_index = (int)(intptr_t)lv_event_get_user_data(event);
    todo_due_style();
}

GW_SHELL_INTERNAL void todo_filter_style(void) {
    lv_obj_t *active = g_todo_filter_active;
    lv_obj_t *history = g_todo_filter_history;
    lv_obj_t *label;
    if (!active || !history) return;
    label = lv_obj_get_child(active, 0);
    if (label)
        lv_obj_set_style_text_font(label,
                                   g_todo_history ? &gw_font_32 : &gw_font_32_bold,
                                   LV_PART_MAIN);
    label = lv_obj_get_child(history, 0);
    if (label)
        lv_obj_set_style_text_font(label,
                                   g_todo_history ? &gw_font_32_bold : &gw_font_32,
                                   LV_PART_MAIN);
    label = lv_obj_get_child(active, 0);
    if (label)
        lv_obj_set_style_text_decor(label,
                                    g_todo_history ? LV_TEXT_DECOR_NONE
                                                   : LV_TEXT_DECOR_UNDERLINE,
                                    LV_PART_MAIN);
    label = lv_obj_get_child(history, 0);
    if (label)
        lv_obj_set_style_text_decor(label,
                                    g_todo_history ? LV_TEXT_DECOR_UNDERLINE
                                                   : LV_TEXT_DECOR_NONE,
                                    LV_PART_MAIN);
}

GW_SHELL_INTERNAL void todo_add_from_input(void) {
    const char *text;
    char due[11] = "";
    /* C09: more than No date/Today/Tomorrow. */
    static const int offsets[5] = { 0, 0, 1, 7, 14 };
    int rc;
    if (!g_notes_area) return;
    if (g_todo_due_index > 0)
        iso_relative_days(offsets[g_todo_due_index], due, sizeof(due));
    text = lv_textarea_get_text(g_notes_area);
    rc = todo_add(text, due);
    if (rc == 0) {
        lv_textarea_set_text(g_notes_area, "");
        g_todo_due_index = 0;
        todo_due_style();
        label_set_if_changed(g_notes_status, GW_STR_TASK_ADDED);
        todo_commit();
    } else if (rc == -2) {
        label_set_if_changed(g_notes_status, GW_STR_TASK_LIMIT);
    } else {
        label_set_if_changed(g_notes_status, GW_STR_TYPE_TASK_FIRST);
    }
}

GW_SHELL_INTERNAL void todo_add_cb(lv_event_t *event) { (void)event; todo_add_from_input(); }

GW_SHELL_INTERNAL void todo_check_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    int completed;
    todo_toggle(id);
    completed = todo_find(id) && todo_find(id)->status == GW_TODO_DONE;
    todo_commit();
    /* C08: completing a task offers Undo, matching delete. */
    if (completed)
        gw_ui_kit_undo_create(GW_STR_TASK_COMPLETED, GW_STR_UNDO,
                              todo_restore_action, (void *)(intptr_t)id);
}

GW_SHELL_INTERNAL void todo_restore_action(void *user_data) {
    int id = (int)(intptr_t)user_data;
    todo_restore(id);
    todo_commit();
}

GW_SHELL_INTERNAL void todo_delete_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    todo_delete(id);
    todo_commit();
    gw_ui_kit_undo_create(GW_STR_TASK_DELETED, GW_STR_UNDO,
                          todo_restore_action, (void *)(intptr_t)id);
}

GW_SHELL_INTERNAL void todo_restore_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    todo_restore(id);
    todo_commit();
}

GW_SHELL_INTERNAL void todo_filter_cb(lv_event_t *event) {
    g_todo_history = (int)(intptr_t)lv_event_get_user_data(event) ? 1 : 0;
    todo_filter_style();
    todo_rebuild_list();
}

GW_SHELL_INTERNAL void todo_delete_button(lv_obj_t *row, int id) {
    lv_obj_t *button = gw_ui_kit_button_icon_create(row, GW_ICON_TRASH, NULL,
                                                    96, 96,
                                                    GW_UI_BUTTON_SECONDARY);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, todo_delete_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)id);
}

GW_SHELL_INTERNAL void todo_row_make(int id, int history) {
    gw_todo_row_t *slot = &g_todo_rows[g_todo_row_count++];
    lv_obj_t *row = gw_ui_kit_row_create(g_todo_list, 112);
    slot->id = id;
    slot->row = row;
    slot->check = NULL;
    slot->due = NULL;
    slot->status = NULL;
    if (history) {
        slot->status = gw_ui_label(row, "", &gw_font_28, gw_ui_c_muted());
        lv_obj_set_width(slot->status, 132);
        slot->text = gw_ui_label(row, "", &gw_font_32, gw_ui_c_text());
        lv_obj_set_flex_grow(slot->text, 1);
        lv_label_set_long_mode(slot->text, LV_LABEL_LONG_DOT);
        {
            lv_obj_t *restore = make_button(row, GW_STR_RESTORE, 150,
                                            GW_UI_TOUCH_MIN, 1);
            lv_obj_add_event_cb(restore, todo_restore_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)id);
        }
    } else {
        slot->check = gw_ui_kit_checkbox_create(row, 0, todo_check_cb,
                                                (void *)(intptr_t)id);
        slot->text = gw_ui_label(row, "", &gw_font_32, gw_ui_c_text());
        lv_obj_set_flex_grow(slot->text, 1);
        lv_label_set_long_mode(slot->text, LV_LABEL_LONG_DOT);
        slot->due = gw_ui_label(row, "", &gw_font_28, gw_ui_c_secondary());
        lv_obj_set_width(slot->due, 150);
        todo_delete_button(row, id);
    }
}

GW_SHELL_INTERNAL void todo_row_update(int slot, const gw_todo_t *t, int history) {
    gw_todo_row_t *row = &g_todo_rows[slot];
    if (history) {
        label_set_if_changed(row->status, t->status == GW_TODO_DONE
                                              ? GW_STR_DONE : GW_STR_DELETED);
        label_set_if_changed(row->text, t->text);
    } else {
        char due[32];
        gw_ui_kit_checkbox_set_checked(row->check,
                                       t->status == GW_TODO_DONE);
        label_set_if_changed(row->text, t->text);
        if (t->due[0]) {
            format_due_label(t->due, due, sizeof(due));
            label_set_if_changed(row->due, due);
            container_set_hidden(row->due, 0);
        } else {
            label_set_if_changed(row->due, "");
            container_set_hidden(row->due, 1);
        }
    }
}

GW_SHELL_INTERNAL void todo_empty_action_cb(lv_event_t *event) {
    (void)event;
    if (g_todo_history) {
        g_todo_history = 0;
        todo_filter_style();
        todo_rebuild_list();
        return;
    }
    if (!g_notes_area || !g_notes_keyboard) return;
    g_text_input_active = 1;
    gw_eink_set_text_input(1);
    lv_obj_clear_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_notes_keyboard);
    lv_keyboard_set_textarea(g_notes_keyboard, g_notes_area);
    lv_obj_add_state(g_notes_area, LV_STATE_FOCUSED);
    if (g_todo_list)
        lv_obj_set_style_pad_bottom(g_todo_list, 480, LV_PART_MAIN);
}

/* Diff by task id (A7): toggle/restore updates one row; add appends one row;
 * delete removes one row.  The list is never lv_obj_clean()ed after build. */
GW_SHELL_INTERNAL void todo_rebuild_list(void) {
    int i, shown = 0, active = 0, done = 0, deleted = 0;
    int desired[GW_TODO_MAX];
    int desired_count = 0;
    int used[GW_TODO_MAX] = {0};
    gw_todo_row_t ordered[GW_TODO_MAX];
    if (!g_todo_list) return;

    for (i = 0; i < g_todo_count; i++) {
        if (g_todos[i].status == GW_TODO_ACTIVE) active++;
        else if (g_todos[i].status == GW_TODO_DONE) done++;
        else if (g_todos[i].status == GW_TODO_DELETED) deleted++;
    }

    if (!g_todo_history) {
        for (i = 0; i < g_todo_count && desired_count < GW_TODO_MAX; i++)
            if (g_todos[i].status == GW_TODO_ACTIVE)
                desired[desired_count++] = g_todos[i].id;
    } else {
        for (i = g_todo_count - 1; i >= 0 && desired_count < GW_TODO_MAX; i--)
            if (g_todos[i].status == GW_TODO_DONE ||
                g_todos[i].status == GW_TODO_DELETED)
                desired[desired_count++] = g_todos[i].id;
    }

    for (i = 0; i < desired_count; i++) {
        gw_todo_t *t = todo_find(desired[i]);
        int j, slot = -1;
        if (!t) continue;
        for (j = 0; j < g_todo_row_count; j++) {
            if (!used[j] && g_todo_rows[j].id == desired[i]) {
                slot = j;
                break;
            }
        }
        if (slot < 0) {
            if (g_todo_row_count >= GW_TODO_MAX) continue;
            slot = g_todo_row_count;
            todo_row_make(desired[i], g_todo_history);
        }
        used[slot] = 1;
        todo_row_update(slot, t, g_todo_history);
        ordered[shown] = g_todo_rows[slot];
        /* Keep widget order equal to model order without rebuilding. */
        lv_obj_move_to_index(ordered[shown].row, shown);
        shown++;
    }

    for (i = g_todo_row_count - 1; i >= 0; i--) {
        if (!used[i] && g_todo_rows[i].row)
            lv_obj_del(g_todo_rows[i].row);
    }
    for (i = 0; i < shown; i++) g_todo_rows[i] = ordered[i];
    g_todo_row_count = shown;
    for (i = shown; i < GW_TODO_MAX; i++) {
        g_todo_rows[i].id = 0;
        g_todo_rows[i].row = NULL;
        g_todo_rows[i].check = NULL;
        g_todo_rows[i].text = NULL;
        g_todo_rows[i].due = NULL;
        g_todo_rows[i].status = NULL;
    }

    if (shown == 0 && !g_todo_empty) {
        g_todo_empty = gw_ui_kit_empty_state_create(
            g_todo_list, GW_ICON_CHECK_SQUARE,
            g_todo_history ? GW_STR_NO_HISTORY_TASKS
                           : GW_STR_NO_ACTIVE_TASKS,
            "", g_todo_history ? GW_STR_ACTIVE : GW_STR_ADD_TASK,
            todo_empty_action_cb, 0);
        lv_obj_set_height(g_todo_empty, LV_PCT(100));
    } else if (shown > 0 && g_todo_empty) {
        lv_obj_del(g_todo_empty);
        g_todo_empty = NULL;
    }

    {
        char summary[128];
        snprintf(summary, sizeof(summary), GW_STR_TODO_SUMMARY_FMT, active, done,
                 deleted);
        label_set_if_changed(g_notes_status, summary);
    }
}

/* Single reset path for both keyboards.  Every close/defocus/navigation path
 * calls this so no screen can leave a hidden keyboard, stale padding or the
 * EPDC input mode enabled. */
GW_SHELL_INTERNAL void keyboard_dismiss(void) {
    int active = g_text_input_active;
    if (g_notes_keyboard &&
        !lv_obj_has_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN)) active = 1;
    if (g_net_keyboard &&
        !lv_obj_has_flag(g_net_keyboard, LV_OBJ_FLAG_HIDDEN)) active = 1;
    if (g_notes_keyboard) lv_obj_add_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (g_net_keyboard) lv_obj_add_flag(g_net_keyboard, LV_OBJ_FLAG_HIDDEN);
    g_text_input_active = 0;
    gw_eink_set_text_input(0);
    if (g_todo_list)
        lv_obj_set_style_pad_bottom(g_todo_list, 0, LV_PART_MAIN);
    if (!active) return;
    g_settle_refresh_pending = 1;
}

GW_SHELL_INTERNAL void notes_keyboard_event(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_CANCEL) {
        keyboard_dismiss();
    } else if (code == LV_EVENT_READY) {
        todo_add_from_input();
    }
}

GW_SHELL_INTERNAL void notes_focus_event(lv_event_t *event) {
    if (lv_event_get_code(event) == LV_EVENT_FOCUSED && g_notes_keyboard) {
        g_text_input_active = 1;
        gw_eink_set_text_input(1);
        lv_obj_clear_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_notes_keyboard);
        lv_keyboard_set_textarea(g_notes_keyboard, g_notes_area);
        if (g_todo_list)
            lv_obj_set_style_pad_bottom(g_todo_list, 480, LV_PART_MAIN);
    } else if (lv_event_get_code(event) == LV_EVENT_DEFOCUSED && g_notes_keyboard) {
        keyboard_dismiss();
    }
}

GW_SHELL_INTERNAL void build_notes_screen(void) {
    lv_obj_t *content, *input_row, *due_row, *filter_row;
    int i;
    static const char *const due_names[5] = {
        GW_STR_NO_DATE, GW_STR_TODAY, GW_STR_TOMORROW, GW_STR_NEXT_WEEK,
        GW_STR_IN_TWO_WEEKS
    };

    g_notes_screen = make_screen();
    gw_ui_kit_title_bar_create(g_notes_screen, GW_STR_TODOS);
    make_bottom_nav(g_notes_screen, 3);
    content = make_content(g_notes_screen, GW_UI_APP_BAR_H + GW_UI_SPACE_2);
    lv_obj_set_style_pad_row(content, GW_UI_SPACE_2, LV_PART_MAIN);

    input_row = lv_obj_create(content);
    lv_obj_set_width(input_row, LV_PCT(100));
    lv_obj_set_height(input_row, 112);
    gw_ui_style_plain_panel(input_row);
    lv_obj_set_flex_flow(input_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(input_row, GW_UI_GAP, LV_PART_MAIN);

    g_notes_area = lv_textarea_create(input_row);
    lv_textarea_set_one_line(g_notes_area, true);
    lv_textarea_set_max_length(g_notes_area, 120);
    lv_textarea_set_placeholder_text(g_notes_area, GW_STR_NEW_TASK);
    lv_obj_set_flex_grow(g_notes_area, 1);
    lv_obj_set_height(g_notes_area, GW_UI_TOUCH_MIN);
    gw_ui_style_textarea(g_notes_area);
    lv_obj_set_style_text_font(g_notes_area, &gw_font_32, LV_PART_MAIN);
    lv_obj_set_style_anim_time(g_notes_area, 0, LV_PART_CURSOR);
    lv_obj_add_event_cb(g_notes_area, notes_focus_event, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(g_notes_area, notes_focus_event, LV_EVENT_DEFOCUSED, NULL);
    {
        lv_obj_t *add = make_button(input_row, GW_STR_ADD, GW_UI_ACTION_W_ADD,
                                    GW_UI_TOUCH_MIN, 0);
        lv_obj_add_event_cb(add, todo_add_cb, LV_EVENT_CLICKED, NULL);
    }

    due_row = lv_obj_create(content);
    lv_obj_set_width(due_row, LV_PCT(100));
    lv_obj_set_height(due_row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(due_row);
    lv_obj_set_flex_flow(due_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(due_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(due_row, GW_UI_SPACE_1, LV_PART_MAIN);
    lv_obj_set_scroll_dir(due_row, LV_DIR_HOR);
    lv_obj_add_flag(due_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(due_row, LV_SCROLLBAR_MODE_OFF);
    {
        lv_obj_t *label = gw_ui_label(due_row, GW_STR_DUE, &gw_font_28,
                                      gw_ui_c_secondary());
        lv_obj_set_width(label, GW_UI_DUE_LABEL_W);
    }
    g_todo_due_index = 0;
    for (i = 0; i < 5; i++) {
        lv_obj_t *button = make_button(due_row, due_names[i],
                                       GW_UI_DUE_CHIP_W, GW_UI_TOUCH_MIN, 1);
        lv_obj_t *label = lv_obj_get_child(button, 0);
        if (label) {
            lv_style_t *small = gw_ui_label_style(&gw_font_28, gw_ui_c_text());
            if (small) lv_obj_add_style(label, small, LV_PART_MAIN);
        }
        lv_obj_add_event_cb(button, todo_due_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        g_todo_due_buttons[i] = button;
    }
    todo_due_style();

    filter_row = lv_obj_create(content);
    lv_obj_set_width(filter_row, LV_PCT(100));
    lv_obj_set_height(filter_row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(filter_row);
    lv_obj_set_flex_flow(filter_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(filter_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(filter_row, GW_UI_SPACE_2, LV_PART_MAIN);
    g_todo_filter_active = make_button(filter_row, GW_STR_ACTIVE,
                                       GW_UI_ACTION_W_FILTER,
                                       GW_UI_TOUCH_MIN, 1);
    g_todo_filter_history = make_button(filter_row, GW_STR_HISTORY,
                                        GW_UI_ACTION_W_FILTER,
                                        GW_UI_TOUCH_MIN, 1);
    lv_obj_add_event_cb(g_todo_filter_active, todo_filter_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)0);
    lv_obj_add_event_cb(g_todo_filter_history, todo_filter_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)1);
    g_notes_status = gw_ui_label(filter_row, "", &gw_font_28,
                                 gw_ui_c_secondary());
    lv_obj_set_flex_grow(g_notes_status, 1);
    lv_obj_set_style_text_align(g_notes_status, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN);
    lv_label_set_long_mode(g_notes_status, LV_LABEL_LONG_DOT);
    todo_filter_style();

    g_todo_list = lv_obj_create(content);
    lv_obj_set_width(g_todo_list, LV_PCT(100));
    lv_obj_set_flex_grow(g_todo_list, 1);
    gw_ui_style_plain_panel(g_todo_list);
    lv_obj_set_flex_flow(g_todo_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_todo_list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(g_todo_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(g_todo_list, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_row(g_todo_list, 0, LV_PART_MAIN);
    lv_obj_set_style_width(g_todo_list, 4, LV_PART_SCROLLBAR);
    lv_obj_set_scroll_dir(g_todo_list, LV_DIR_VER);
    lv_obj_add_flag(g_todo_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_todo_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_todo_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(g_todo_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    attach_scroll_events(g_todo_list);
    todo_rebuild_list();

    g_notes_keyboard = lv_keyboard_create(g_notes_screen);
    lv_obj_set_size(g_notes_keyboard, GW_UI_CONTENT_W, 480);
    lv_obj_align(g_notes_keyboard, LV_ALIGN_BOTTOM_MID, 0, -GW_UI_NAV_BAR_H);
    gw_ui_kit_style_keyboard(g_notes_keyboard);
    gw_eink_set_keyboard_rect(1, GW_UI_MARGIN,
                              GW_UI_PANEL_H - GW_UI_NAV_BAR_H - 480,
                              GW_UI_MARGIN + GW_UI_CONTENT_W - 1,
                              GW_UI_PANEL_H - GW_UI_NAV_BAR_H - 1);
    lv_obj_add_event_cb(g_notes_keyboard, notes_keyboard_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_notes_keyboard, notes_keyboard_event, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
}


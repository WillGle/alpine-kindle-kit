/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_shell_internal.h"

/* ── Applications and app updates ─────────────────────────────────────── */

GW_SHELL_INTERNAL void apps_row_cb(lv_event_t *event);
GW_SHELL_INTERNAL int apps_slot_for(const char *id);

GW_SHELL_INTERNAL void strip_newline(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

GW_SHELL_INTERNAL int apps_fetch(gw_app_entry_t *out, int max) {
    char response[GW_APP_LIST_MAX];
    char *line, *next;
    int used = 0, result;
    /* One request returns every row; the old APP COUNT + APP INFO N loop
     * opened one socket per app. */
    result = ipc_cmd_all("APP LIST", response, sizeof(response));
    if (result < 0) return 0;
    if (result > 0) return -1;   /* the bounded buffer did not fit the list */
    for (line = response; line && *line && used < max; line = next) {
        char id[64] = "", version[32] = "", description[128] = "";
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        if (!strncmp(line, "APP END", 7)) break;
        if (sscanf(line, "APP %63s %31s %127[^\n]", id, version,
                   description) >= 2 && id[0]) {
            snprintf(out[used].id, sizeof(out[used].id), "%s", id);
            snprintf(out[used].version, sizeof(out[used].version), "%s",
                     version);
            snprintf(out[used].description, sizeof(out[used].description),
                     "%s", description);
            used++;
        }
    }
    /* Never silently drop rows: the caller shows a list-too-large message. */
    if (line && !strncmp(line, "APP ", 4) && strncmp(line, "APP END", 7))
        return -1;
    return used;
}

GW_SHELL_INTERNAL void apps_make_row(int index, const gw_app_entry_t *entry) {
    lv_obj_t *row = lv_btn_create(g_apps_list);
    lv_obj_t *col;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_APP_ROW_H);
    gw_ui_style_flat(row);
    lv_obj_set_style_border_width(row, GW_UI_HAIRLINE, LV_PART_MAIN);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, GW_UI_SPACE_1, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_SPACE_2, LV_PART_MAIN);

    col = lv_obj_create(row);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, GW_UI_APP_ROW_H - GW_UI_SPACE_2);
    gw_ui_style_plain_panel(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 0, LV_PART_MAIN);
    /* E05: name, version and description are separate lines, not one
     * concatenated id/version/description string. */
    g_app_name_labels[index] = gw_ui_label(col, entry->id, &gw_font_32_bold,
                                           gw_ui_c_text());
    lv_obj_set_width(g_app_name_labels[index], LV_PCT(100));
    lv_label_set_long_mode(g_app_name_labels[index], LV_LABEL_LONG_DOT);
    g_app_version_labels[index] = gw_ui_label(col, entry->version, &gw_font_28,
                                              gw_ui_c_secondary());
    lv_obj_set_width(g_app_version_labels[index], LV_PCT(100));
    g_app_desc_labels[index] = gw_ui_label(col, entry->description, &gw_font_28,
                                           gw_ui_c_secondary());
    lv_obj_set_width(g_app_desc_labels[index], LV_PCT(100));
    lv_label_set_long_mode(g_app_desc_labels[index], LV_LABEL_LONG_DOT);

    /* E04: Remove lives in the app detail sheet, not on every row. */
    lv_obj_add_event_cb(row, apps_row_cb, LV_EVENT_CLICKED, NULL);
    free(g_app_row_ids[index]);
    g_app_row_ids[index] = strdup(entry->id);
    /* Row identity is an owned app ID, not a list index: deleting another app
     * shifts indices and must not retarget this row. */
    lv_obj_set_user_data(row, g_app_row_ids[index]);
    g_app_rows[index] = row;
}

GW_SHELL_INTERNAL int apps_slot_for(const char *id) {
    int i;
    for (i = 0; i < g_app_count; i++)
        if (!strcmp(g_app_entries[i].id, id)) return i;
    return -1;
}

GW_SHELL_INTERNAL void apps_delete_slot(int slot) {
    if (slot < 0 || slot >= g_app_count) return;
    if (g_app_rows[slot]) lv_obj_del(g_app_rows[slot]);
    free(g_app_row_ids[slot]);
    g_app_row_ids[slot] = NULL;
    {
        int i;
        for (i = slot; i + 1 < g_app_count; i++) {
            g_app_entries[i] = g_app_entries[i + 1];
            g_app_rows[i] = g_app_rows[i + 1];
            g_app_name_labels[i] = g_app_name_labels[i + 1];
            g_app_version_labels[i] = g_app_version_labels[i + 1];
            g_app_desc_labels[i] = g_app_desc_labels[i + 1];
            g_app_row_ids[i] = g_app_row_ids[i + 1];
        }
    }
    g_app_count--;
    g_app_rows[g_app_count] = NULL;
    g_app_row_ids[g_app_count] = NULL;
    g_app_name_labels[g_app_count] = NULL;
    g_app_version_labels[g_app_count] = NULL;
    g_app_desc_labels[g_app_count] = NULL;
}

/* Incremental list update (A7).  A remove deletes one row; an install adds one
 * row; a version/description change updates that row only.  The panel is not
 * rebuilt, so the dirty region is one row instead of the whole list. */
GW_SHELL_INTERNAL void apps_apply_entries(gw_app_entry_t *entries, int count) {
    int i, j;
    int was_empty;
    if (!g_apps_list) return;
    was_empty = g_app_count == 0;

    /* The installed-list empty state owns Refresh when the list is empty;
     * hide the sticky duplicate so there is exactly one. */
    if (g_apps_action_row)
        container_set_hidden(g_apps_action_row, count == 0);

    if (g_apps_empty_label && count > 0) {
        lv_obj_del(g_apps_empty_label);
        g_apps_empty_label = NULL;
    }

    /* Drop rows that no longer exist. */
    for (i = g_app_count - 1; i >= 0; i--) {
        int keep = 0;
        for (j = 0; j < count; j++)
            if (!strcmp(entries[j].id, g_app_entries[i].id)) { keep = 1; break; }
        if (!keep) apps_delete_slot(i);
    }

    for (i = 0; i < count; i++) {
        int slot = apps_slot_for(entries[i].id);
        if (slot < 0) {
            if (g_app_count >= GW_APP_MAX) break;
            slot = g_app_count++;
            g_app_entries[slot] = entries[i];
            apps_make_row(slot, &entries[i]);
            continue;
        }
        if (strcmp(entries[i].version, g_app_entries[slot].version) ||
            strcmp(entries[i].description, g_app_entries[slot].description)) {
            label_set_if_changed(g_app_version_labels[slot],
                                 entries[i].version);
            label_set_if_changed(g_app_desc_labels[slot],
                                 entries[i].description);
            g_apps_row_updates++;
        }
        g_app_entries[slot] = entries[i];
    }

    if (was_empty && count > 0) g_apps_full_rebuilds++;
    if (count == 0 && !g_apps_empty_label) {
        g_apps_empty_label = gw_ui_kit_empty_state_create(
            g_apps_list, GW_ICON_LIST, GW_STR_NO_APPS,
            GW_STR_APP_BUNDLES_SUB, GW_STR_REFRESH,
            apps_refresh_cb, 0);
        lv_obj_set_height(g_apps_empty_label, LV_PCT(100));
    }
}

GW_SHELL_INTERNAL void apps_message_set(const char *text) {
    if (!g_apps_message) return;
    if (text && text[0]) {
        container_set_hidden(g_apps_message, 0);
        label_set_if_changed(g_apps_message, text);
    } else {
        container_set_hidden(g_apps_message, 1);
    }
}

GW_SHELL_INTERNAL void apps_rebuild_list(void) {
    gw_app_entry_t entries[GW_APP_MAX];
    int count;
    if (!g_apps_list) return;
    count = apps_fetch(entries, GW_APP_MAX);
    if (count < 0) {
        apps_message_set(GW_STR_APP_LIST_TOO_LARGE);
        return;
    }
    apps_apply_entries(entries, count);
}

GW_SHELL_INTERNAL void apps_remove_action(void *user_data) {
    const char *id = (const char *)user_data;
    int index = id ? apps_slot_for(id) : -1;
    char command[GW_IPC_MAX_LINE], response[GW_IPC_MAX_LINE];
    if (index < 0 || !g_app_entries[index].id[0]) return;
    snprintf(command, sizeof(command), "APP REMOVE %s", g_app_entries[index].id);
    if (ipc_cmd(command, response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2))
        apps_message_set(GW_STR_APP_REMOVED);
    else {
        strip_newline(response);
        snprintf(command, sizeof(command), GW_STR_REMOVE_FAILED_FMT,
                 response[0] ? response : GW_STR_UNAVAILABLE);
        apps_message_set(command);
    }
    apps_refresh();
}

GW_SHELL_INTERNAL void apps_row_cb(lv_event_t *event) {
    lv_obj_t *row = lv_event_get_current_target(event);
    const char *id = (const char *)lv_obj_get_user_data(row);
    int index = id ? apps_slot_for(id) : -1;
    char message[256];
    static char modal_id[GW_IPC_MAX_LINE];
    if (index < 0 || index >= g_app_count) return;
    snprintf(message, sizeof(message), "%s  |  %s",
             g_app_entries[index].version,
             g_app_entries[index].description[0]
                 ? g_app_entries[index].description
                 : GW_STR_UNAVAILABLE);
    /* E04/E07: app detail sheet, inset rather than a full-screen page.
     * The modal must own its identity: the row list can be rebuilt (and the
     * row id freed) while the sheet is open. */
    snprintf(modal_id, sizeof(modal_id), "%s", g_app_entries[index].id);
    gw_ui_kit_confirm_create(g_app_entries[index].id, message, GW_STR_REMOVE,
                             apps_remove_action, modal_id);
}

GW_SHELL_INTERNAL void apps_refresh(void) {
    apps_rebuild_list();
}

GW_SHELL_INTERNAL void apps_refresh_cb(lv_event_t *event) {
    (void)event;
    apps_refresh();
    apps_message_set(GW_STR_APP_LIST_REFRESHED);
}

GW_SHELL_INTERNAL void update_selection_refresh(void) {
    int i;
    for (i = 0; i < g_update_package_count; i++) {
        lv_obj_t *row = g_update_rows[i];
        int width = i == g_update_selected ? 3 : 0;
        if (!row) continue;
        lv_obj_set_style_border_width(row, width, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, gw_ui_c_text(), LV_PART_MAIN);
    }
    if (g_update_install_button)
        gw_ui_kit_primary_set_enabled(g_update_install_button,
                                      g_update_selected >= 0);
}

GW_SHELL_INTERNAL void update_package_cb(lv_event_t *event) {
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    if (index < 0 || index >= g_update_package_count) return;
    g_update_selected = index;
    update_selection_refresh();
}

static void update_action_layout(void) {
    int empty = g_update_package_count == 0;
    if (g_update_scan_button)
        container_set_hidden(g_update_scan_button, empty);
    if (g_update_install_button)
        lv_obj_set_width(g_update_install_button,
                         empty ? GW_UI_CONTENT_W
                               : (GW_UI_CONTENT_W - GW_UI_GAP) / 2);
}

static void update_show_empty_state(void) {
    lv_obj_t *empty;
    if (!g_update_list || g_update_package_count > 0) return;
    empty = gw_ui_kit_empty_state_create(
        g_update_list, GW_ICON_LIST, GW_STR_NO_PACKAGES, "",
        GW_STR_SCAN_USB, update_scan_cb, 1);
    lv_obj_set_height(empty, LV_PCT(100));
}

GW_SHELL_INTERNAL void update_scan(void) {
    char response[GW_IPC_MAX_LINE];
    int count = 0, i;
    if (!g_update_list) return;
    lv_obj_clean(g_update_list);
    for (i = 0; i < 6; i++) {
        g_update_rows[i] = NULL;
        g_update_packages[i][0] = '\0';
    }
    g_update_package_count = 0;
    g_update_selected = -1;
    update_selection_refresh();
    label_set_if_changed(g_update_status, GW_STR_SCANNING_USB);
    if (ipc_cmd("APP SCAN", response, sizeof(response)) == 0)
        sscanf(response, "APP USB %d", &count);
    for (i = 0; i < count && i < 6; i++) {
        char command[64];
        char *name;
        lv_obj_t *row;
        snprintf(command, sizeof(command), "APP USB %d", i);
        if (ipc_cmd(command, response, sizeof(response)) != 0) break;
        if (strncmp(response, "APP USBFILE ", 12)) continue;
        name = response + 12;
        strip_newline(name);
        if (!name[0]) continue;
        snprintf(g_update_packages[g_update_package_count],
                 sizeof(g_update_packages[0]), "%s", name);
        row = lv_btn_create(g_update_list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, GW_UI_TOUCH_MIN);
        gw_ui_style_flat(row);
        lv_obj_set_style_border_width(row, GW_UI_HAIRLINE, LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, gw_ui_c_border(), LV_PART_MAIN);
        {
            lv_obj_t *label = gw_ui_label(row, name, &gw_font_32,
                                          gw_ui_c_text());
            lv_obj_set_width(label, LV_PCT(100));
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            lv_obj_center(label);
        }
        lv_obj_add_event_cb(row, update_package_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)g_update_package_count);
        g_update_rows[g_update_package_count] = row;
        g_update_package_count++;
    }
    if (g_update_package_count == 0) {
        update_show_empty_state();
        label_set_if_changed(g_update_status, GW_STR_NO_PACKAGES);
    } else {
        char text[64];
        snprintf(text, sizeof(text), GW_STR_PACKAGE_FMT,
                 g_update_package_count);
        label_set_if_changed(g_update_status, text);
    }
    update_action_layout();
    update_selection_refresh();
}

GW_SHELL_INTERNAL void update_scan_cb(lv_event_t *event) {
    (void)event;
    update_scan();
}

GW_SHELL_INTERNAL void update_install_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE], message[192];
    lv_obj_t *button = lv_event_get_target(event);
    if (gw_ui_kit_button_is_busy(button)) return;
    /* C07: Install requires an explicitly selected package. */
    if (g_update_selected < 0 ||
        g_update_selected >= g_update_package_count) {
        label_set_if_changed(g_update_status, GW_STR_SELECT_PACKAGE);
        return;
    }
    {
        char command[64];
        snprintf(command, sizeof(command), "APP INSTALL USB %d",
                 g_update_selected);
        gw_ui_kit_busy_set(button, 1, "...");
        label_set_if_changed(g_update_status, GW_STR_INSTALL_UPDATE);
        if (ipc_cmd(command, response, sizeof(response)) == 0 &&
            !strncmp(response, "OK", 2)) {
            strip_newline(response);
            snprintf(message, sizeof(message), GW_STR_UPDATE_APPLIED_FMT,
                     g_update_packages[g_update_selected]);
        } else {
            strip_newline(response);
            snprintf(message, sizeof(message), GW_STR_UPDATE_REJECTED_FMT,
                     response[0] ? response : GW_STR_UNAVAILABLE);
        }
        gw_ui_kit_busy_set(button, 0, NULL);
        /* C06: the result is its own label; the package list is not overwritten. */
        label_set_if_changed(g_update_status, message);
    }
}

GW_SHELL_INTERNAL void build_apps_screen(void) {
    lv_obj_t *content, *row, *label;

    g_apps_screen = make_screen();
    gw_ui_kit_app_bar_create(g_apps_screen, GW_STR_APP_MANAGER, gw_back_cb);
    content = make_content(g_apps_screen, GW_UI_APP_BAR_H + GW_UI_GAP);
    /* This screen has no bottom nav, so reclaim the reserved nav band. */
    lv_obj_set_height(content, GW_UI_PANEL_H - (GW_UI_APP_BAR_H +
                                                GW_UI_GAP) - GW_UI_GAP);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, GW_UI_SPACE_2, LV_PART_MAIN);

    settings_section(content, GW_STR_INSTALLED);
    g_apps_list = lv_obj_create(content);
    lv_obj_set_width(g_apps_list, LV_PCT(100));
    lv_obj_set_flex_grow(g_apps_list, 1);
    lv_obj_set_style_min_height(g_apps_list, GW_UI_APP_LIST_MIN_H,
                                LV_PART_MAIN);
    gw_ui_style_plain_panel(g_apps_list);
    lv_obj_set_style_pad_right(g_apps_list, GW_UI_LIST_SCROLL_PAD,
                                LV_PART_MAIN);
    lv_obj_set_flex_flow(g_apps_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_apps_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_apps_list, LV_DIR_VER);
    lv_obj_add_flag(g_apps_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_apps_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_apps_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(g_apps_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_style_width(g_apps_list, GW_UI_LIST_SCROLLBAR_W,
                            LV_PART_SCROLLBAR);
    attach_scroll_events(g_apps_list);

    /* Refresh is the only row action; USB install lives next to the package
     * list so there is exactly one install path. */
    row = lv_obj_create(content);
    g_apps_action_row = row;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_COMPACT_ACTION_H);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    {
        g_apps_refresh_button = make_button(row, GW_STR_REFRESH,
                                            GW_UI_ACTION_W_REFRESH,
                                            GW_UI_COMPACT_ACTION_H, 1);
        lv_obj_add_event_cb(g_apps_refresh_button, apps_refresh_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    g_apps_message = gw_ui_label(content, "", &gw_font_28,
                                 gw_ui_c_secondary());
    lv_obj_set_width(g_apps_message, LV_PCT(100));
    lv_label_set_long_mode(g_apps_message, LV_LABEL_LONG_DOT);
    container_set_hidden(g_apps_message, 1);

    /* App updates are a section here, not a second destination. */
    settings_section(content, GW_STR_USB_PACKAGES);
    label = gw_ui_label(content, GW_STR_USB_PACKAGES_SUB, &gw_font_28,
                        gw_ui_c_secondary());
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);

    g_update_list = lv_obj_create(content);
    lv_obj_set_width(g_update_list, LV_PCT(100));
    lv_obj_set_height(g_update_list, GW_UI_APP_UPDATE_LIST_H);
    gw_ui_style_plain_panel(g_update_list);
    lv_obj_set_style_pad_right(g_update_list, GW_UI_LIST_SCROLL_PAD,
                                LV_PART_MAIN);
    lv_obj_set_style_width(g_update_list, GW_UI_LIST_SCROLLBAR_W,
                            LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(g_update_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_update_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_update_list, LV_DIR_VER);
    lv_obj_add_flag(g_update_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_update_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_update_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(g_update_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    attach_scroll_events(g_update_list);

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_COMPACT_ACTION_H);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    {
        g_update_scan_button = make_button(row, GW_STR_SCAN_USB,
                                           (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                           GW_UI_COMPACT_ACTION_H, 1);
        /* Selected-package model: the only install action. */
        g_update_install_button = make_button(
            row, GW_STR_INSTALL_SELECTED,
            (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
            GW_UI_COMPACT_ACTION_H, 0);
        lv_obj_add_event_cb(g_update_scan_button, update_scan_cb,
                            LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(g_update_install_button, update_install_cb,
                            LV_EVENT_CLICKED, NULL);
        gw_ui_kit_primary_set_enabled(g_update_install_button, 0);
    }
    update_show_empty_state();
    update_action_layout();
    g_update_status = gw_ui_label(content, "", &gw_font_28,
                                  gw_ui_c_secondary());
    lv_obj_set_width(g_update_status, LV_PCT(100));
    lv_label_set_long_mode(g_update_status, LV_LABEL_LONG_DOT);
    g_update_package_count = 0;
    g_update_selected = -1;

    settings_section(content, GW_STR_BUILT_IN);
    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    {
        lv_obj_t *col = lv_obj_create(row);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_height(col, 100);
        gw_ui_style_plain_panel(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        {
            lv_obj_t *name = gw_ui_label(col, GW_STR_READER, &gw_font_36_bold,
                                         gw_ui_c_text());
            lv_obj_set_width(name, LV_PCT(100));
        }
        {
            lv_obj_t *sub = gw_ui_label(col, GW_STR_READER_SUB, &gw_font_28,
                                        gw_ui_c_secondary());
            lv_obj_set_width(sub, LV_PCT(100));
            lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
        }
        {
            lv_obj_t *launch = make_button(row, GW_ICON_CHEVRON_RIGHT, 96,
                                           GW_UI_TOUCH_MIN, 1);
            lv_obj_add_event_cb(launch, reader_launch, LV_EVENT_CLICKED, NULL);
        }
    }
    apps_refresh();
}


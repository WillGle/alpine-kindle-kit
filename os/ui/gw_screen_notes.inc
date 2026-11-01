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
        lv_obj_t *add = make_button(input_row, GW_STR_ADD, 160,
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
    lv_obj_set_style_pad_column(due_row, 4, LV_PART_MAIN);
    {
        lv_obj_t *label = gw_ui_label(due_row, GW_STR_DUE, &gw_font_28,
                                      gw_ui_c_secondary());
        lv_obj_set_width(label, 70);
    }
    g_todo_due_index = 0;
    for (i = 0; i < 5; i++) {
        lv_obj_t *button = make_button(due_row, due_names[i], 176,
                                       GW_UI_TOUCH_MIN, 1);
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
    g_todo_filter_active = make_button(filter_row, GW_STR_ACTIVE, 170,
                                       GW_UI_TOUCH_MIN, 1);
    g_todo_filter_history = make_button(filter_row, GW_STR_HISTORY, 170,
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

/* ── Network ────────────────────────────────────────────────────────────── */

static void network_select_cb(lv_event_t *event);

static void network_row_update(int slot, const gw_net_ap_t *ap) {
    char text[80];
    if (slot < 0 || slot >= GW_NET_MAX || !g_net_rows[slot]) return;
    label_set_if_changed(g_net_ssid_labels[slot], ap->ssid);
    snprintf(text, sizeof(text), "%d dBm", ap->signal);
    label_set_if_changed(g_net_dbm_labels[slot], text);
    /* B06: the lock is its own widget, so it cannot wrap under the dBm text
     * or push the signal bars. */
    label_set_if_changed(g_net_lock_labels[slot],
                         ap->secure ? GW_ICON_LOCK : "");
    if (g_net_signal_widgets[slot]) {
        lv_obj_set_user_data(g_net_signal_widgets[slot],
                             (void *)(intptr_t)signal_bars_for(ap->signal));
        lv_obj_invalidate(g_net_signal_widgets[slot]);
    }
    if (g_net_check_labels[slot]) {
        int connected = g_net_state == 1 && !strcmp(ap->ssid, g_net_ssid);
        label_set_if_changed(g_net_check_labels[slot],
                             connected ? LV_SYMBOL_OK : "");
    }
}

static void network_make_row(int slot, const gw_net_ap_t *ap) {
    lv_obj_t *row = lv_btn_create(g_net_list);
    lv_obj_t *ssid;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 104);
    gw_ui_style_flat(row);
    lv_obj_set_style_border_color(row, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, GW_UI_SPACE_1, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_SPACE_2, LV_PART_MAIN);
    ssid = gw_ui_label(row, ap->ssid, &gw_font_32, gw_ui_c_text());
    lv_obj_set_flex_grow(ssid, 1);
    lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
    g_net_ssid_labels[slot] = ssid;
    g_net_signal_widgets[slot] = gw_ui_kit_signal_bars_create(
        row, signal_bars_for(ap->signal));
    g_net_dbm_labels[slot] = gw_ui_label(row, "", &gw_font_28,
                                         gw_ui_c_secondary());
    lv_obj_set_width(g_net_dbm_labels[slot], 130);
    lv_obj_set_style_text_align(g_net_dbm_labels[slot], LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN);
    g_net_lock_labels[slot] = gw_ui_label(row, "", &gw_font_28,
                                          gw_ui_c_secondary());
    lv_obj_set_width(g_net_lock_labels[slot], 36);
    g_net_check_labels[slot] = gw_ui_label(row, "", &gw_font_28,
                                           gw_ui_c_text());
    lv_obj_set_width(g_net_check_labels[slot], 36);
    memcpy(g_net_row_ssid[slot], ap->ssid, sizeof(g_net_row_ssid[slot]));
    g_net_row_ssid[slot][sizeof(g_net_row_ssid[slot]) - 1] = '\0';
    lv_obj_add_event_cb(row, network_select_cb, LV_EVENT_CLICKED, NULL);
    /* Selection is by SSID, not by row index, so a scan that adds/removes a
     * row cannot make a stale index select the wrong network. */
    lv_obj_set_user_data(row, g_net_row_ssid[slot]);
    g_net_rows[slot] = row;
    network_row_update(slot, ap);
}

static void network_delete_slot(int slot) {
    int i;
    if (slot < 0 || slot >= g_net_row_count) return;
    if (g_net_rows[slot]) lv_obj_del(g_net_rows[slot]);
    for (i = slot; i + 1 < g_net_row_count; i++) {
        g_net_rows[i] = g_net_rows[i + 1];
        memcpy(g_net_row_ssid[i], g_net_row_ssid[i + 1],
               sizeof(g_net_row_ssid[i]));
        g_net_ssid_labels[i] = g_net_ssid_labels[i + 1];
        g_net_dbm_labels[i] = g_net_dbm_labels[i + 1];
        g_net_lock_labels[i] = g_net_lock_labels[i + 1];
        g_net_check_labels[i] = g_net_check_labels[i + 1];
        g_net_signal_widgets[i] = g_net_signal_widgets[i + 1];
    }
    g_net_row_count--;
    g_net_rows[g_net_row_count] = NULL;
    g_net_ssid_labels[g_net_row_count] = NULL;
    g_net_dbm_labels[g_net_row_count] = NULL;
    g_net_lock_labels[g_net_row_count] = NULL;
    g_net_check_labels[g_net_row_count] = NULL;
    g_net_signal_widgets[g_net_row_count] = NULL;
}

/* Incremental network list (A7): a rescan updates rows whose SSID already
 * exists, adds new SSIDs and removes vanished ones.  It never cleans the
 * whole list, so the dirty region stays row-sized. */
static void network_build_rows(void) {
    int i;
    if (!g_net_list) return;
    if (g_net_empty_label && g_net_ap_count > 0) {
        lv_obj_del(g_net_empty_label);
        g_net_empty_label = NULL;
    }
    for (i = g_net_row_count - 1; i >= 0; i--) {
        int keep = 0, j;
        for (j = 0; j < g_net_ap_count; j++)
            if (!strcmp(g_net_row_ssid[i], g_net_aps[j].ssid)) { keep = 1; break; }
        if (!keep) network_delete_slot(i);
    }
    for (i = 0; i < g_net_ap_count; i++) {
        int slot = -1, j;
        for (j = 0; j < g_net_row_count; j++)
            if (!strcmp(g_net_row_ssid[j], g_net_aps[i].ssid)) { slot = j; break; }
        if (slot < 0 && g_net_row_count < GW_NET_MAX) {
            slot = g_net_row_count++;
            network_make_row(slot, &g_net_aps[i]);
        } else if (slot >= 0) {
            network_row_update(slot, &g_net_aps[i]);
        }
    }
    /* Keep row order equal to the scan order so row-index users (selection,
     * host tests) cannot mismatch signal/SSID after a rescan. */
    if (g_net_row_count > 0) {
        lv_obj_t *rows[GW_NET_MAX];
        char ssids[GW_NET_MAX][65];
        lv_obj_t *ssid_labels[GW_NET_MAX];
        lv_obj_t *dbm_labels[GW_NET_MAX];
        lv_obj_t *lock_labels[GW_NET_MAX];
        lv_obj_t *check_labels[GW_NET_MAX];
        lv_obj_t *signals[GW_NET_MAX];
        int used[GW_NET_MAX] = {0};
        int n = 0, j;
        for (i = 0; i < g_net_ap_count; i++) {
            int slot = -1;
            for (j = 0; j < g_net_row_count; j++)
                if (!used[j] && !strcmp(g_net_row_ssid[j], g_net_aps[i].ssid)) {
                    slot = j;
                    break;
                }
            if (slot < 0) continue;
            used[slot] = 1;
            lv_obj_move_to_index(g_net_rows[slot], n);
            rows[n] = g_net_rows[slot];
            memcpy(ssids[n], g_net_row_ssid[slot], sizeof(ssids[n]));
            ssid_labels[n] = g_net_ssid_labels[slot];
            dbm_labels[n] = g_net_dbm_labels[slot];
            lock_labels[n] = g_net_lock_labels[slot];
            check_labels[n] = g_net_check_labels[slot];
            signals[n] = g_net_signal_widgets[slot];
            n++;
        }
        for (i = 0; i < n; i++) {
            g_net_rows[i] = rows[i];
            memcpy(g_net_row_ssid[i], ssids[i], sizeof(g_net_row_ssid[i]));
            g_net_ssid_labels[i] = ssid_labels[i];
            g_net_dbm_labels[i] = dbm_labels[i];
            g_net_lock_labels[i] = lock_labels[i];
            g_net_check_labels[i] = check_labels[i];
            g_net_signal_widgets[i] = signals[i];
        }
        g_net_row_count = n;
    }
    if (g_net_ap_count == 0 && !g_net_empty_label) {
        g_net_empty_label = gw_ui_label(g_net_list, GW_STR_NO_NETWORKS,
                                        &gw_font_28, gw_ui_c_muted());
        lv_obj_set_width(g_net_empty_label, LV_PCT(100));
        lv_obj_set_style_pad_all(g_net_empty_label, GW_UI_GAP, LV_PART_MAIN);
    }
}

static void network_select_cb(lv_event_t *event) {
    const char *ssid = (const char *)lv_obj_get_user_data(
        lv_event_get_target(event));
    int index = -1, i;
    if (!ssid) return;
    for (i = 0; i < g_net_ap_count; i++)
        if (!strcmp(g_net_aps[i].ssid, ssid)) { index = i; break; }
    if (index < 0) return;
    snprintf(g_net_selected_ssid, sizeof(g_net_selected_ssid), "%s", ssid);
    {
        char message[96];
        snprintf(message, sizeof(message), GW_STR_SELECTED_FMT, ssid);
        label_set_if_changed(g_net_message, message);
    }
    network_update_ui();
}

static void network_update_ui(void) {
    int i;
    const char *selected_ssid = NULL;
    if (!g_net_list) return;
    if (g_net_selected_ssid[0]) selected_ssid = g_net_selected_ssid;
    for (i = 0; i < g_net_row_count && i < GW_NET_MAX; i++) {
        lv_obj_t *row = g_net_rows[i];
        int selected = selected_ssid &&
                       !strcmp(g_net_row_ssid[i], selected_ssid);
        int width = selected ? 3 : 0;
        if (!row) continue;
        if (lv_obj_get_style_border_width(row, LV_PART_MAIN) != width) {
            lv_obj_set_style_border_width(row, width, LV_PART_MAIN);
            lv_obj_set_style_border_color(row, gw_ui_c_text(), LV_PART_MAIN);
        }
        if (g_net_check_labels[i])
            label_set_if_changed(
                g_net_check_labels[i],
                (g_net_state == 1 &&
                 !strcmp(g_net_row_ssid[i], g_net_ssid)) ? LV_SYMBOL_OK : "");
    }
    if (g_net_signal_bars && g_net_state == 1) {
        int level = signal_bars_for(g_net_signal);
        uint32_t n = lv_obj_get_child_cnt(g_net_signal_bars);
        uint32_t k;
        for (k = 0; k < n; k++) {
            lv_obj_t *bar = lv_obj_get_child(g_net_signal_bars, (int32_t)k);
            lv_color_t colour = (int)k < level ? gw_ui_c_text()
                                               : gw_ui_c_white();
            if (lv_color_to8(lv_obj_get_style_bg_color(bar, LV_PART_MAIN)) !=
                lv_color_to8(colour))
                lv_obj_set_style_bg_color(bar, colour, LV_PART_MAIN);
        }
    }
    if (g_net_disconnect_button)
        container_set_hidden(g_net_disconnect_button, g_net_state != 1);
    if (g_net_forget_button)
        container_set_hidden(g_net_forget_button, g_net_state != 1);
    if (g_net_connection_label) {
        char text[160];
        if (g_net_state == 1 && g_net_ssid[0])
            snprintf(text, sizeof(text), "%s | %s | %s", GW_STR_CONNECTED,
                     g_net_ssid, g_net_ip[0] ? g_net_ip : GW_STR_CONNECTED);
        else if (g_net_state == 0)
            snprintf(text, sizeof(text), "%s", GW_STR_NOT_CONNECTED);
        else
            snprintf(text, sizeof(text), "%s", GW_STR_UNKNOWN);
        label_set_if_changed(g_net_connection_label, text);
    }
    {
        int selected_secure = 0;
        for (i = 0; i < g_net_ap_count; i++)
            if (!strcmp(g_net_aps[i].ssid, g_net_selected_ssid)) {
                selected_secure = g_net_aps[i].secure;
                break;
            }
        int selected_connected = g_net_selected_ssid[0] &&
                                 g_net_state == 1 &&
                                 !strcmp(g_net_selected_ssid, g_net_ssid);
        container_set_hidden(g_net_password_row,
                             !(selected_secure && !selected_connected));
        /* C05: Connect is meaningful only for a selected, disconnected
         * network.  Hide it when the selection is already connected. */
        if (g_net_connect_button) {
            container_set_hidden(g_net_connect_button, selected_connected);
            if (selected_connected)
                lv_obj_add_state(g_net_connect_button, LV_STATE_DISABLED);
            else
                lv_obj_clear_state(g_net_connect_button, LV_STATE_DISABLED);
        }
    }
}

static void wifi_scan(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE];
    (void)event;
    if (ipc_cmd("NET SCAN", response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        g_wifi_scan_pending = 1;
        g_wifi_scan_started_ms = gw_eink_now_ms();
        if (g_net_scan_button)
            gw_ui_kit_busy_set(g_net_scan_button, 1, GW_STR_SCANNING);
        label_set_if_changed(g_net_message, GW_STR_SCANNING);
    } else {
        label_set_if_changed(g_net_message, GW_STR_SCAN_FAILED);
    }
}

static void wifi_connect(lv_event_t *event) {
    static const char hex[] = "0123456789abcdef";
    const char *ssid;
    const char *password;
    char command[GW_IPC_MAX_LINE], response[GW_IPC_MAX_LINE];
    size_t ssid_len, password_len, i, n;
    int selected = -1;
    (void)event;
    for (i = 0; i < (size_t)g_net_ap_count; i++)
        if (!strcmp(g_net_aps[i].ssid, g_net_selected_ssid)) {
            selected = (int)i;
            break;
        }
    if (selected < 0) {
        label_set_if_changed(g_net_message, GW_STR_SELECT_NETWORK);
        return;
    }
    if (gw_ui_kit_button_is_busy(g_net_connect_button)) return;
    ssid = g_net_aps[selected].ssid;
    password = g_net_password ? lv_textarea_get_text(g_net_password) : "";
    ssid_len = strlen(ssid);
    password_len = strlen(password);
    if (g_net_aps[selected].secure &&
        (password_len < 8 || password_len > 63)) {
        label_set_if_changed(g_net_message, GW_STR_PASSWORD_RULE);
        return;
    }
    if (ssid_len < 1 || ssid_len > 32) {
        label_set_if_changed(g_net_message, GW_STR_CONNECT_FAILED);
        return;
    }
    n = (size_t)snprintf(command, sizeof(command), GW_CMD_NET_CONNECT " ");
    if (n >= sizeof(command)) return;
    for (i = 0; i < ssid_len; i++) {
        command[n++] = hex[(unsigned char)ssid[i] >> 4];
        command[n++] = hex[(unsigned char)ssid[i] & 15];
    }
    command[n++] = ' ';
    if (n + password_len * 2 + 1 >= sizeof(command)) return;
    for (i = 0; i < password_len; i++) {
        command[n++] = hex[(unsigned char)password[i] >> 4];
        command[n++] = hex[(unsigned char)password[i] & 15];
    }
    command[n] = '\0';
    if (ipc_cmd(command, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        label_set_if_changed(g_net_message, GW_STR_CONNECTING);
        gw_ui_kit_busy_set(g_net_connect_button, 1, GW_STR_CONNECTING);
        if (g_net_password) lv_textarea_set_text(g_net_password, "");
        g_net_poll_deadline_ms = gw_eink_now_ms() + 15000;
        g_net_poll_next_ms = gw_eink_now_ms() + 2000;
    } else {
        label_set_if_changed(g_net_message, GW_STR_CONNECT_FAILED);
    }
    memset(command, 0, sizeof(command));
}

static void network_disconnect_action(void *user_data) {
    char response[GW_IPC_MAX_LINE];
    (void)user_data;
    if (g_net_disconnect_button)
        gw_ui_kit_busy_set(g_net_disconnect_button, 1, GW_STR_DISCONNECTING);
    label_set_if_changed(g_net_message, GW_STR_DISCONNECTING);
    if (ipc_cmd(GW_CMD_NET_DISCO, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        label_set_if_changed(g_net_message, GW_STR_NOT_CONNECTED);
        g_net_state = 0;
        g_net_ssid[0] = '\0';
        g_net_ip[0] = '\0';
    } else {
        label_set_if_changed(g_net_message, GW_STR_DISCONNECT_FAILED);
    }
    if (g_net_disconnect_button)
        gw_ui_kit_busy_set(g_net_disconnect_button, 0, NULL);
    net_status_fetch();
    network_update_ui();
}

static void network_forget_action(void *user_data) {
    char response[GW_IPC_MAX_LINE];
    (void)user_data;
    if (ipc_cmd(GW_CMD_NET_FORGET, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        label_set_if_changed(g_net_message, GW_STR_NOT_CONNECTED);
        g_net_state = 0;
        g_net_ssid[0] = '\0';
        g_net_ip[0] = '\0';
    } else {
        label_set_if_changed(g_net_message, GW_STR_NET_FORGET_FAILED);
    }
    network_update_ui();
}

static void wifi_disconnect(lv_event_t *event) {
    (void)event;
    gw_ui_kit_confirm_create(GW_STR_DISCONNECT_TITLE,
                             GW_STR_DISCONNECT_MESSAGE, GW_STR_DISCONNECT,
                             network_disconnect_action, NULL);
}

static void wifi_forget(lv_event_t *event) {
    (void)event;
    gw_ui_kit_confirm_create(GW_STR_FORGET_TITLE, GW_STR_FORGET_MESSAGE,
                             GW_STR_FORGET, network_forget_action, NULL);
}

static void wifi_eye_cb(lv_event_t *event) {
    lv_obj_t *label;
    (void)event;
    if (!g_net_password) return;
    g_net_password_visible = !g_net_password_visible;
    lv_textarea_set_password_mode(g_net_password, !g_net_password_visible);
    label = lv_obj_get_child(g_net_eye_button, 0);
    if (label)
        lv_label_set_text(label, g_net_password_visible ? GW_ICON_EYE_SLASH
                                                        : GW_ICON_EYE);
}

static void wifi_focus(lv_event_t *event) {
    if (lv_event_get_code(event) == LV_EVENT_FOCUSED && g_net_keyboard) {
        g_text_input_active = 1;
        gw_eink_set_text_input(1);
        lv_obj_clear_flag(g_net_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_net_keyboard);
        lv_keyboard_set_textarea(g_net_keyboard, g_net_password);
    } else if (lv_event_get_code(event) == LV_EVENT_DEFOCUSED && g_net_keyboard) {
        keyboard_dismiss();
    }
}

static void wifi_keyboard_event(lv_event_t *event) {
    keyboard_dismiss();
    (void)event;
}

static void build_network_screen(void) {
    lv_obj_t *content, *row, *card, *line;

    g_network_screen = make_screen();
    gw_ui_kit_app_bar_create(g_network_screen, GW_STR_NETWORK, gw_back_cb);
    content = make_content(g_network_screen, GW_UI_APP_BAR_H + GW_UI_GAP);
    lv_obj_set_style_pad_row(content, GW_UI_SPACE_2, LV_PART_MAIN);

    card = lv_obj_create(content);
    g_net_card = card;
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 220);
    gw_ui_style_card(card);
    lv_obj_set_style_pad_all(card, GW_UI_SPACE_1, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, GW_UI_SPACE_1, LV_PART_MAIN);
    {
        lv_obj_t *label = gw_ui_label(card, GW_STR_CONNECTION, &gw_font_28,
                                      gw_ui_c_muted());
        lv_obj_set_width(label, LV_PCT(100));
    }
    g_net_connection_label = gw_ui_label(card, GW_STR_UNKNOWN, &gw_font_36_bold,
                                          gw_ui_c_text());
    lv_obj_set_width(g_net_connection_label, LV_PCT(100));
    lv_label_set_long_mode(g_net_connection_label, LV_LABEL_LONG_DOT);
    row = lv_obj_create(card);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_SPACE_2, LV_PART_MAIN);
    g_net_signal_bars = gw_ui_kit_signal_bars_create(row, 4);
    {
        lv_obj_t *spacer = lv_obj_create(row);
        lv_obj_set_flex_grow(spacer, 1);
        lv_obj_set_height(spacer, GW_UI_TOUCH_MIN);
        gw_ui_style_plain_panel(spacer);
    }
    g_net_disconnect_button = make_button(row, GW_STR_DISCONNECT, 220,
                                          GW_UI_TOUCH_MIN, 1);
    g_net_forget_button = make_button(row, GW_STR_FORGET, 160,
                                      GW_UI_TOUCH_MIN, 1);
    lv_obj_add_event_cb(g_net_disconnect_button, wifi_disconnect,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_net_forget_button, wifi_forget, LV_EVENT_CLICKED, NULL);

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *label = gw_ui_label(row, GW_STR_NETWORKS, &gw_font_28,
                                      gw_ui_c_muted());
        lv_obj_set_width(label, 200);
    }
    g_net_scan_button = make_button(row, GW_STR_SCAN, 160, GW_UI_TOUCH_MIN, 1);
    lv_obj_add_event_cb(g_net_scan_button, wifi_scan, LV_EVENT_CLICKED, NULL);

    g_net_list = lv_obj_create(content);
    lv_obj_set_width(g_net_list, LV_PCT(100));
    lv_obj_set_height(g_net_list, 416);
    gw_ui_style_plain_panel(g_net_list);
    lv_obj_set_style_pad_right(g_net_list, 20, LV_PART_MAIN);
    lv_obj_set_style_width(g_net_list, 4, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(g_net_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_net_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_net_list, LV_DIR_VER);
    lv_obj_add_flag(g_net_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_net_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_net_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(g_net_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    attach_scroll_events(g_net_list);

    g_net_password_row = lv_obj_create(content);
    lv_obj_set_width(g_net_password_row, LV_PCT(100));
    lv_obj_set_height(g_net_password_row, 112);
    gw_ui_style_plain_panel(g_net_password_row);
    lv_obj_set_flex_flow(g_net_password_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_net_password_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g_net_password_row, GW_UI_SPACE_2,
                                LV_PART_MAIN);
    g_net_password = lv_textarea_create(g_net_password_row);
    lv_textarea_set_one_line(g_net_password, true);
    lv_textarea_set_password_mode(g_net_password, true);
    lv_textarea_set_password_show_time(g_net_password, 0);
    lv_textarea_set_max_length(g_net_password, 63);
    lv_textarea_set_placeholder_text(g_net_password, GW_STR_PASSWORD);
    lv_obj_set_flex_grow(g_net_password, 1);
    lv_obj_set_height(g_net_password, GW_UI_TOUCH_MIN);
    gw_ui_style_textarea(g_net_password);
    lv_obj_set_style_text_font(g_net_password, &gw_font_32, LV_PART_MAIN);
    lv_obj_set_style_anim_time(g_net_password, 0, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(g_net_password, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_border_width(g_net_password, 0, LV_PART_CURSOR);
    lv_obj_add_event_cb(g_net_password, wifi_focus, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(g_net_password, wifi_focus, LV_EVENT_DEFOCUSED, NULL);
    g_net_eye_button = gw_ui_kit_button_icon_create(g_net_password_row,
                                                    GW_ICON_EYE, NULL, 96, 96,
                                                    GW_UI_BUTTON_SECONDARY);
    lv_obj_add_event_cb(g_net_eye_button, wifi_eye_cb, LV_EVENT_CLICKED, NULL);
    container_set_hidden(g_net_password_row, 1);

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    g_net_connect_button = make_button(row, GW_STR_CONNECT, 240,
                                       GW_UI_TOUCH_MIN, 0);
    lv_obj_add_event_cb(g_net_connect_button, wifi_connect, LV_EVENT_CLICKED,
                        NULL);

    line = make_hairline(content, GW_UI_CONTENT_W);
    (void)line;
    g_net_message = gw_ui_label(content, GW_STR_SELECT_NETWORK, &gw_font_28,
                                gw_ui_c_secondary());
    lv_obj_set_width(g_net_message, LV_PCT(100));
    lv_label_set_long_mode(g_net_message, LV_LABEL_LONG_DOT);

    g_net_keyboard = lv_keyboard_create(g_network_screen);
    lv_obj_set_size(g_net_keyboard, GW_UI_CONTENT_W, 480);
    lv_obj_align(g_net_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    gw_ui_kit_style_keyboard(g_net_keyboard);
    gw_eink_set_keyboard_rect(1, GW_UI_MARGIN, GW_UI_PANEL_H - 480,
                              GW_UI_MARGIN + GW_UI_CONTENT_W - 1,
                              GW_UI_PANEL_H - 1);
    lv_obj_add_event_cb(g_net_keyboard, wifi_keyboard_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_net_keyboard, wifi_keyboard_event, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(g_net_keyboard, LV_OBJ_FLAG_HIDDEN);

    g_net_ap_count = 2;
    snprintf(g_net_aps[0].ssid, sizeof(g_net_aps[0].ssid), "Home Wi-Fi");

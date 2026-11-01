    unsigned i;
    for (i = 0; i < 50 && found_count < GW_NET_MAX; i++) {
        gw_net_ap_t ap;
        snprintf(cmd, sizeof(cmd), "NET RESULT %u", i);
        if (ipc_cmd(cmd, response, sizeof(response)) != 0) break;
        if (net_result_parse(response, &ap) != 0) break;
        found[found_count++] = ap;
    }
    changed = found_count != g_net_ap_count;
    if (!changed) {
        for (i = 0; i < (unsigned)found_count; i++) {
            if (strcmp(found[i].ssid, g_net_aps[i].ssid) ||
                found[i].signal != g_net_aps[i].signal ||
                found[i].secure != g_net_aps[i].secure) {
                changed = 1;
                break;
            }
        }
    }
    if (changed) {
        memcpy(g_net_aps, found, sizeof(g_net_aps[0]) * (size_t)found_count);
        g_net_ap_count = found_count;
        /* Selecting by SSID keeps the target valid across reordering; clear it
         * only when the selected network vanished from the scan. */
        {
            int still_there = 0, k;
            for (k = 0; k < g_net_ap_count; k++)
                if (!strcmp(g_net_aps[k].ssid, g_net_selected_ssid)) still_there = 1;
            if (!still_there) g_net_selected_ssid[0] = '\0';
        }
        network_build_rows();
    }
    g_wifi_scan_pending = 0;
    if (g_net_scan_button) gw_ui_kit_busy_set(g_net_scan_button, 0, NULL);
    if (g_net_ap_count == 0)
        label_set_if_changed(g_net_message, GW_STR_NO_NETWORKS);
    else
        label_set_if_changed(g_net_message, GW_STR_SELECT_NETWORK);
    network_update_ui();
}

static void status_apply(const char *status) {
    char text[128];
    char power[24] = "unknown", net[24] = "unknown", usb[24] = "unknown";
    int battery = -1, light = -1;
    char *field;

    field = strstr(status, " battery=");
    if (field) battery = atoi(field + strlen(" battery="));
    field = strstr(status, " light=");
    if (field) light = atoi(field + strlen(" light="));
    field = strstr(status, " power=");
    if (field) sscanf(field, " power=%23s", power);
    field = strstr(status, " net=");
    if (field) sscanf(field, " net=%23s", net);
    field = strstr(status, " usb=");
    if (field) sscanf(field, " usb=%23s", usb);

    lock_apply_status(status);
    if (g_usb_label) {
        int data_link = !strcmp(usb, "host-class");
        label_set_if_changed(g_usb_label, data_link ? LV_SYMBOL_USB " USB" : "");
        container_set_hidden(g_usb_label, !data_link);
    }

    if (battery >= 0 && battery <= 100) {
        char value[16];
        snprintf(value, sizeof(value), "%d%%", battery);
        snprintf(text, sizeof(text), "%s %s",
                 battery_icon(battery, !strcmp(power, "charging")), value);
        label_set_if_changed(g_battery_label, text);
        label_set_if_changed(g_settings_battery_label, value);
    } else {
        snprintf(text, sizeof(text), "%s",
                 !strcmp(power, "charging") ? "Charging" : "--");
        label_set_if_changed(g_battery_label, text);
        label_set_if_changed(g_settings_battery_label, text);
    }
    container_set_hidden(g_settings_battery_row, battery < 0 && strcmp(power, "charging"));

    if (!strcmp(net, "connected")) {
        if (net_status_fetch() == 0 && g_net_ssid[0]) {
            snprintf(text, sizeof(text), "%s | %s", g_net_ssid,
                     g_net_ip[0] ? g_net_ip : GW_STR_CONNECTED);
        } else {
            snprintf(text, sizeof(text), "%s", GW_STR_CONNECTED);
        }
        status_wifi_bars(4);
        label_set_if_changed(g_settings_wifi_label, text);
        label_set_if_changed(g_net_connection_label, GW_STR_CONNECTED);
    } else if (!strcmp(net, "disconnected")) {
        status_wifi_bars(0);
        label_set_if_changed(g_settings_wifi_label, GW_STR_NOT_CONNECTED);
        label_set_if_changed(g_net_connection_label, GW_STR_NOT_CONNECTED);
        g_net_state = 0;
        g_net_ssid[0] = '\0';
        g_net_ip[0] = '\0';
    } else {
        status_wifi_bars(0);
        label_set_if_changed(g_settings_wifi_label, GW_STR_UNKNOWN);
        label_set_if_changed(g_net_connection_label, GW_STR_UNKNOWN);
    }

    if (light >= 0 && light <= 100) {
        char light_text[24];
        g_light_value = light;
        snprintf(light_text, sizeof(light_text), "%d%%", light);
        label_set_if_changed(g_light_label, light_text);
        if (g_light_bar) lv_bar_set_value(g_light_bar, light, LV_ANIM_OFF);
        g_light_last = light;
    }
    network_update_ui();
}

static void status_refresh_now(void) {
    char status[GW_IPC_MAX_LINE];
    if (ipc_cmd(GW_CMD_STATUS, status, sizeof(status)) == 0)
        status_apply(status);
}

static void status_update(lv_timer_t *timer) {
    char status[GW_IPC_MAX_LINE];
    (void)timer;
    if (!g_clock_label) return;

    /* Clock and date update only when the displayed day/minute changes, so a
     * static Home stays electrically quiet. */
    {
        struct tm tm;
        static int last_minute = -1, last_yday = -1;
        int current_minute;
        char clk[16];
        gw_ui_now(&tm);
        snprintf(clk, sizeof(clk), "%02d:%02d", tm.tm_hour, tm.tm_min);
        current_minute = tm.tm_hour * 60 + tm.tm_min;
        if (tm.tm_yday != last_yday) {
            char date_text[96];
            format_hero_date(date_text, sizeof(date_text));
            label_set_if_changed(g_date_label, date_text);
            label_set_if_changed(g_settings_date_label, date_text);
            last_yday = tm.tm_yday;
        }
        if (current_minute != last_minute) {
            label_set_if_changed(g_clock_label, clk);
            lock_refresh_time();
            last_minute = current_minute;
        }
    }

    if (g_lock_screen && lv_scr_act() == g_lock_screen) return;

    /* Status IPC is deliberately bounded: once every 30 s in production, or
     * immediately while a Wi-Fi scan result is pending. */
    {
        static uint32_t last_status_ms = 0;
        uint32_t now_ms = gw_eink_now_ms();
        if (!g_wifi_scan_pending && last_status_ms != 0 &&
            now_ms - last_status_ms < 30000)
            return;
        last_status_ms = now_ms;
    }

    if (ipc_cmd(GW_CMD_STATUS, status, sizeof(status)) == 0) {
        status_apply(status);
    } else {
        label_set_if_changed(g_battery_label, "--");
        status_wifi_bars(0);
        label_set_if_changed(g_net_connection_label, GW_STR_STATUS_UNAVAILABLE);
        return;
    }

    if (g_wifi_scan_pending && gw_eink_now_ms() - g_wifi_scan_started_ms >= 800)
        network_scan_apply();
}

/* One-second tick for the bounded scan result and post-connect status poll. */
static void ui_tick_cb(lv_timer_t *timer) {
    uint32_t now = gw_eink_now_ms();
    (void)timer;
    if (g_wifi_scan_pending && now - g_wifi_scan_started_ms >= 15000) {
        g_wifi_scan_pending = 0;
        if (g_net_scan_button) gw_ui_kit_busy_set(g_net_scan_button, 0, NULL);
        label_set_if_changed(g_net_message, GW_STR_SCAN_FAILED);
    } else if (g_wifi_scan_pending && now - g_wifi_scan_started_ms >= 800) {
        char status[GW_IPC_MAX_LINE];
        if (ipc_cmd(GW_CMD_STATUS, status, sizeof(status)) == 0) {
            char *error = strstr(status, " wifi-error=");
            if (!error || atoi(error + strlen(" wifi-error=")) == 0)
                network_scan_apply();
            else {
                g_wifi_scan_pending = 0;
                if (g_net_scan_button) gw_ui_kit_busy_set(g_net_scan_button, 0, NULL);
                label_set_if_changed(g_net_message, GW_STR_SCAN_FAILED);
            }
        }
    }
    if (g_net_poll_deadline_ms && (int32_t)(now - g_net_poll_deadline_ms) >= 0) {
        g_net_poll_deadline_ms = 0;
        g_net_poll_next_ms = 0;
        if (g_net_connect_button)
            gw_ui_kit_busy_set(g_net_connect_button, 0, NULL);
        if (g_net_state != 1)
            label_set_if_changed(g_net_message, GW_STR_CONNECT_FAILED);
    }
    if (g_net_poll_deadline_ms && (int32_t)(now - g_net_poll_next_ms) >= 0) {
        g_net_poll_next_ms = now + 2000;
        net_status_fetch();
        network_update_ui();
        if (g_net_state == 1) {
            g_net_poll_deadline_ms = 0;
            if (g_net_connect_button)
                gw_ui_kit_busy_set(g_net_connect_button, 0, NULL);
            label_set_if_changed(g_net_message, GW_STR_CONNECTED);
        }
    }
}

/* ── App launch, lock and light actions ─────────────────────────────────── */

static void reader_launch(lv_event_t *event) {
    (void)event;
#if GW_UI_PERF
    GW_PERF_LOG("GW_UI: ui event reader t=%lld us\n", gw_now_us());
#endif
#if GW_UI_HOST_SIM
    return;
#elif GW_UI_LIFECYCLE_SPIKE
    char response[GW_IPC_MAX_LINE];
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
    if (gw_epdc_finish() < 0) {
        recovery_set("Display error");
        return;
    }
#endif
    if (ipc_cmd(GW_CMD_APP_LAUNCH " koreader", response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        g_exit_for_reader = 1;
    } else {
        recovery_set("Reader unavailable");
    }
#else
    recovery_set("Reader unavailable");
#endif
}

static void lock_screen_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE];
    (void)event;
    if (ipc_cmd(GW_CMD_LOCK, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        /* U3: lock is allowed to do one full GC16 cleanup. */
        gw_scene_show(GW_SCREEN_LOCK, 1);
    } else {
        recovery_set("Lock unavailable");
    }
}

static void unlock_screen_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE];
    (void)event;
    if (ipc_cmd(GW_CMD_UNLOCK, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        if (g_lock_label) lv_label_set_text(g_lock_label, GW_STR_UNLOCKED);
        g_ignore_lock_until_ms = gw_eink_now_ms() + 2500;
        /* Unlock is a normal scene: GC16_FAST partial, no white flash. */
        gw_scene_show(GW_SCREEN_HOME, 0);
    } else if (g_lock_label) {
        lv_label_set_text(g_lock_label, "Unlock unavailable");
    }
}

static void light_apply_ui(void) {
    char text[16];
    snprintf(text, sizeof(text), "%d%%", g_light_value);
    label_set_if_changed(g_light_label, text);
    if (g_light_status_label) {
        char tray[32];
        snprintf(tray, sizeof(tray), "%s %s", GW_ICON_EYE, text);
        label_set_if_changed(g_light_status_label, tray);
    }
    if (g_light_bar) lv_bar_set_value(g_light_bar, g_light_value, LV_ANIM_OFF);
}

static void light_step(int delta) {
    char response[GW_IPC_MAX_LINE], cmd[32];
    g_light_value += delta;
    if (g_light_value < 0) g_light_value = 0;
    if (g_light_value > 100) g_light_value = 100;
    /* Value text is optimistic; one LIGHT SET per step. */
    light_apply_ui();
    snprintf(cmd, sizeof(cmd), GW_CMD_LIGHT_SET " %d", g_light_value);
    ipc_cmd(cmd, response, sizeof(response));
    g_light_last = g_light_value;
}

static void light_minus_cb(lv_event_t *event) { (void)event; light_step(-1); }
static void light_plus_cb(lv_event_t *event) { (void)event; light_step(1); }

static void light_set_abs(int value) {
    char response[GW_IPC_MAX_LINE], cmd[32];
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    g_light_value = value;
    light_apply_ui();
    g_light_last = value;
    snprintf(cmd, sizeof(cmd), GW_CMD_LIGHT_SET " %d", value);
    ipc_cmd(cmd, response, sizeof(response));
}

/* D08: a real slider.  Its value is applied on release so a drag does not
 * emit one EPDC update per finger movement. */
static void light_slider_pressed_cb(lv_event_t *event) {
    (void)event;
    gw_eink_request_scroll(1500);
}

static void light_slider_released_cb(lv_event_t *event) {
    (void)event;
    if (g_light_bar) light_set_abs((int)lv_slider_get_value(g_light_bar));
    gw_eink_request_scroll(300);
}

/* D09: the status-bar Light target opens Settings scrolled to Frontlight. */
static void light_open_cb(lv_event_t *event) {
    (void)event;
    gw_ui_show(GW_SCREEN_SETTINGS);
    if (g_settings_light_row && g_settings_content)
        lv_obj_scroll_to_view(g_settings_light_row, LV_ANIM_OFF);
}

/* ── Navigation callbacks ───────────────────────────────────────────────── */

static void nav_home_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_HOME); }
static void nav_library_cb(lv_event_t *event) { reader_launch(event); }
static void nav_calendar_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_CALENDAR); }
static void nav_todos_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_NOTES); }
static void nav_settings_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_SETTINGS); }
static void calendar_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_CALENDAR); }
static void notes_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_NOTES); }

/* B05/C03: secondary screens remember which screen opened them. */
static void gw_open_secondary(gw_screen_t screen, gw_screen_t back) {
    g_back_screen = back;
    gw_ui_show(screen);
}

static void gw_back_cb(lv_event_t *event) {
    gw_screen_t back = g_back_screen;
    (void)event;
    g_back_screen = GW_SCREEN_HOME;
    gw_ui_show(back);
}

static void network_open_cb(lv_event_t *event) {
    (void)event;
    gw_open_secondary(GW_SCREEN_NETWORK, g_active_screen);
}
static void apps_open_cb(lv_event_t *event) {
    (void)event;
    gw_open_secondary(GW_SCREEN_APPS, g_active_screen);

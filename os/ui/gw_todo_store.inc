    int i, n = 0;
    format_iso_date(year, month, day, iso, sizeof(iso));
    for (i = 0; i < g_todo_count; i++)
        if (g_todos[i].status == GW_TODO_ACTIVE && !strcmp(g_todos[i].due, iso))
            n++;
    return n;
}

static int todo_add(const char *text, const char *due) {
    char clean[GW_TODO_TEXT_MAX];
    gw_todo_t *t;
    if (!text) return -1;
    todo_sanitize(clean, sizeof(clean), text);
    if (!clean[0]) return -1;
    if (g_todo_count >= GW_TODO_MAX) return -2;
    t = &g_todos[g_todo_count++];
    t->id = g_todo_next_id++;
    t->status = GW_TODO_ACTIVE;
    t->created = t->updated = (long)time(NULL);
    todo_copy_due(t->due, sizeof(t->due), due);
    snprintf(t->text, sizeof(t->text), "%s", clean);
    return 0;
}

static void todo_toggle(int id) {
    gw_todo_t *t = todo_find(id);
    if (!t || t->status == GW_TODO_DELETED) return;
    t->status = t->status == GW_TODO_ACTIVE ? GW_TODO_DONE : GW_TODO_ACTIVE;
    t->updated = (long)time(NULL);
}

static void todo_delete(int id) {
    gw_todo_t *t = todo_find(id);
    if (!t) return;
    t->status = GW_TODO_DELETED;
    t->updated = (long)time(NULL);
    g_todo_last_deleted = id;
}

static void todo_restore(int id) {
    gw_todo_t *t = todo_find(id);
    if (!t) return;
    t->status = GW_TODO_ACTIVE;
    t->updated = (long)time(NULL);
}

static int todo_commit(void) {
    int save_rc = todo_store_save();
    todo_rebuild_list();
    apply_model();
    if (save_rc < 0)
        label_set_if_changed(g_notes_status, GW_STR_TODO_SAVE_FAILED);
    return save_rc;
}

/* ── Status refresh ─────────────────────────────────────────────────────── */

static void network_update_ui(void);
static void network_scan_apply(void);
static void network_build_rows(void);
static void settings_facts_update(void);

static void lock_apply_status(const char *status) {
    const char *field;
    if (!status) return;
    field = strstr(status, " lock=");
    if (!field) return;
    field += strlen(" lock=");
    if (!strncmp(field, "locked", 6)) {
        if (g_ignore_lock_until_ms &&
            (int32_t)(gw_eink_now_ms() - g_ignore_lock_until_ms) < 0)
            return;
        if (g_lock_screen && lv_scr_act() != g_lock_screen) {
            gw_scene_show(GW_SCREEN_LOCK, 1);
            dprintf(2, "GW_SHELL: startup/status lock engaged\n");
        }
    } else if (!strncmp(field, "unlocked", 8)) {
        if (g_lock_screen && lv_scr_act() == g_lock_screen)
            gw_scene_show(GW_SCREEN_HOME, 0);
    }
}

/* Startup path: the service may already be locked (e.g. a power press while
 * KOReader was foreground).  The normal 30-second status throttle must not
 * hide that state from a freshly started shell. */
static void lock_sync_now(void) {
    char status[GW_IPC_MAX_LINE];
    if (ipc_cmd(GW_CMD_STATUS, status, sizeof(status)) == 0)
        lock_apply_status(status);
}

static int hex_decode(const char *hex, char *out, size_t size) {
    size_t i, n;
    if (!hex || !out || size == 0) return -1;
    n = strlen(hex);
    if (!n || (n & 1) || n / 2 >= size) return -1;
    for (i = 0; i < n; i++)
        if (!((hex[i] >= '0' && hex[i] <= '9') ||
              (hex[i] >= 'a' && hex[i] <= 'f') ||
              (hex[i] >= 'A' && hex[i] <= 'F')))
            return -1;
    for (i = 0; i < n; i += 2) {
        unsigned value;
        if (sscanf(hex + i, "%2x", &value) != 1) return -1;
        out[i / 2] = (char)value;
    }
    out[n / 2] = '\0';
    return 0;
}

/* Accept the old one-field plain SSID and the new three-field hex form.
 * Malformed hex in the new form is rejected rather than shown as text. */
static int net_result_parse(const char *response, gw_net_ap_t *ap) {
    char line[GW_IPC_MAX_LINE];
    char *first, *rest, *sig, *secure;
    size_t n;
    if (!response || strncmp(response, "NET SSID ", 9)) return -1;
    snprintf(line, sizeof(line), "%s", response + 9);
    n = strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    first = line;
    rest = strchr(first, ' ');
    if (rest) *rest++ = '\0';
    if (!first[0]) return -1;
    if (!rest || !rest[0]) {
        /* Old service: plain SSID. */
        snprintf(ap->ssid, sizeof(ap->ssid), "%s", first);
        ap->signal = 0;
        ap->secure = 0;
        return 0;
    }
    sig = rest;
    secure = strchr(sig, ' ');
    if (!secure) return -1;
    *secure++ = '\0';
    if (hex_decode(first, ap->ssid, sizeof(ap->ssid))) return -1;
    ap->signal = atoi(sig);
    ap->secure = atoi(secure) ? 1 : 0;
    return 0;
}

static int signal_bars_for(int dbm) {
    if (dbm == 0) return 0;
    if (dbm >= -55) return 4;
    if (dbm >= -65) return 3;
    if (dbm >= -75) return 2;
    return 1;
}

static const char *battery_icon(int percent, int charging) {
    if (charging) return LV_SYMBOL_CHARGE;
    if (percent < 0) return LV_SYMBOL_BATTERY_EMPTY;
    if (percent <= 10) return LV_SYMBOL_BATTERY_EMPTY;
    if (percent <= 40) return LV_SYMBOL_BATTERY_1;
    if (percent <= 70) return LV_SYMBOL_BATTERY_2;
    return LV_SYMBOL_BATTERY_3;
}

static void status_wifi_bars(int level) {
    if (!g_wifi_bars) return;
    lv_obj_set_user_data(g_wifi_bars, (void *)(intptr_t)level);
    lv_obj_invalidate(g_wifi_bars);
}

/* Fetch NET STATUS into g_net_ssid/g_net_ip/g_net_state. */
static int net_status_fetch(void) {
    char response[GW_IPC_MAX_LINE];
    char *token, *save = NULL;
    int state = -1;
    if (ipc_cmd(GW_CMD_NET_STATUS, response, sizeof(response)) != 0)
        return -1;
    token = strtok_r(response, " \r\n", &save);
    if (!token || strcmp(token, "NET")) return -1;
    token = strtok_r(NULL, " \r\n", &save);
    if (!token) return -1;
    if (!strcmp(token, "connected")) state = 1;
    else if (!strcmp(token, "disconnected")) state = 0;
    else state = -1;
    g_net_state = state;
    g_net_ssid[0] = '\0';
    g_net_ip[0] = '\0';
    if (state == 1) {
        token = strtok_r(NULL, " \r\n", &save);
        if (token) {
            if (hex_decode(token, g_net_ssid, sizeof(g_net_ssid)) != 0)
                snprintf(g_net_ssid, sizeof(g_net_ssid), "%s", token);
        }
        token = strtok_r(NULL, " \r\n", &save);
        if (token) snprintf(g_net_ip, sizeof(g_net_ip), "%s", token);
    }
    return 0;
}

static void network_scan_apply(void) {
    char cmd[32], response[GW_IPC_MAX_LINE];
    gw_net_ap_t found[GW_NET_MAX];
    int found_count = 0;
    int changed;

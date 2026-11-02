#include "gw_shell_internal.h"

#ifndef GW_UI_HOST_SIM
#define GW_UI_HOST_SIM 0
#endif
#ifndef GW_UI_LIFECYCLE_SPIKE
#define GW_UI_LIFECYCLE_SPIKE 0
#endif
#ifndef GW_UI_CALENDAR_SPIKE
#define GW_UI_CALENDAR_SPIKE 0
#endif

#define GW_TODO_PATH "/var/lib/gleworks/todos.tsv"

/* Paint policy.  `smooth` is the default: scene/scroll/content use the
 * 16-level GC16_FAST partial waveform, and GC16 full is reserved for explicit
 * deep cleanup (lock/unlock or the idle safety budget).  `quality` keeps the
 * old GC16-per-scene behaviour for A/B testing; balanced/fast are accepted as
 * aliases of smooth so an old policy file cannot reintroduce taps that flash.
 * Select at runtime with GW_UI_PAINT_POLICY, /etc/gleworks/paint-policy,
 * GW_UI_SCENE_WAVEFORM or /etc/gleworks/scene-waveform. */

GW_SHELL_INTERNAL uint32_t gw_waveform_from_name(const char *value) {
    if (!value || !value[0]) return 0;
    if (!strcmp(value, "gc16_fast") || !strcmp(value, "smooth"))
        return WAVEFORM_MODE_GC16_FAST;
    if (!strcmp(value, "gl16")) return WAVEFORM_MODE_GL16;
    if (!strcmp(value, "du4")) return WAVEFORM_MODE_DU4;
    if (!strcmp(value, "reagl")) return WAVEFORM_MODE_REAGL;
    if (!strcmp(value, "gc16") || !strcmp(value, "quality"))
        return WAVEFORM_MODE_GC16;
    return 0;
}

GW_SHELL_INTERNAL int gw_read_trimmed(const char *path, char *buffer, size_t size) {
    FILE *file;
    if (!path || !buffer || size == 0) return 0;
    file = fopen(path, "r");
    if (!file) return 0;
    if (fgets(buffer, (int)size, file)) {
        char *end = buffer + strlen(buffer);
        while (end > buffer && (end[-1] == '\n' || end[-1] == '\r' ||
                                end[-1] == ' ' || end[-1] == '\t'))
            *--end = '\0';
    } else {
        buffer[0] = '\0';
    }
    fclose(file);
    return buffer[0] != '\0';
}

static uint32_t paint_waveform(gw_paint_wave_t wave) {
    switch (wave) {
    case GW_WAVE_GC16_FAST: return WAVEFORM_MODE_GC16_FAST;
    case GW_WAVE_GL16: return WAVEFORM_MODE_GL16;
    case GW_WAVE_REAGL: return WAVEFORM_MODE_REAGL;
    case GW_WAVE_GC16: return WAVEFORM_MODE_GC16;
    case GW_WAVE_DU4: return WAVEFORM_MODE_DU4;
    default: return 0;
    }
}

static gw_paint_profile_id_t paint_profile_for_screen(gw_screen_t screen) {
    switch (screen) {
    case GW_SCREEN_SETTINGS: return GW_PAINT_SETTINGS;
    case GW_SCREEN_NETWORK: return GW_PAINT_NETWORK;
    case GW_SCREEN_APPS: return GW_PAINT_APPS;
    case GW_SCREEN_UPDATE: return GW_PAINT_UPDATE;
    case GW_SCREEN_NOTES: return GW_PAINT_NOTES;
    case GW_SCREEN_CALENDAR: return GW_PAINT_CALENDAR;
    case GW_SCREEN_HUB: return GW_PAINT_HUB;
    case GW_SCREEN_LOCK: return GW_PAINT_LOCK;
    default: return GW_PAINT_HOME;
    }
}

/* One universal EPDC engine, one ordered rule set selects the profile. */
static int g_paint_page_snap;

#if GW_UI_DIAG
static int diag_int_file(const char *path, int minimum, int maximum) {
    FILE *file = fopen(path, "r");
    int value = -1;
    if (!file) return -1;
    if (fscanf(file, "%d", &value) != 1) value = -1;
    fclose(file);
    if (value < minimum || value > maximum) return -1;
    return value;
}
#endif

GW_SHELL_INTERNAL void gw_shell_apply_profile(gw_screen_t screen) {
    gw_paint_profile_id_t id = paint_profile_for_screen(screen);
    const gw_paint_profile_t *base = gw_paint_profile_get(id);
    gw_paint_decision_t decision =
        gw_paint_rules_decide(id, base->content);
    const gw_paint_profile_t *profile = decision.profile;
    g_paint_page_snap = decision.page_snap;
    gw_eink_set_scroll_preview_max((uint32_t)decision.preview_max);
    gw_eink_set_scroll_preview_ms((uint32_t)decision.preview_ms);
    gw_eink_set_scroll_waveform(paint_waveform(profile->preview_wave));
    gw_eink_set_scroll_release_waveform(paint_waveform(profile->release_wave));
    gw_eink_set_scroll_clean_delay((uint32_t)profile->cleanup_ms);
    gw_epdc_set_scroll_cleanup_area(profile->cleanup_area);
    {
        int periodic = profile->periodic_full_after;
        const char *value = getenv("GW_UI_FULL_REFRESH_AFTER");
        if (value && value[0]) {
            int n = atoi(value);
            if (n >= 0 && n <= 10000) periodic = n;
        }
        gw_epdc_set_periodic_full_after(periodic);
    }
#if GW_UI_DIAG
    {
        int v = diag_int_file("/run/gw-ui-test-scroll-wave", 1, 11);
        if (v > 0) {
            gw_eink_set_scroll_waveform((uint32_t)v);
            gw_eink_set_scroll_release_waveform((uint32_t)v);
        }
        v = diag_int_file("/run/gw-ui-test-preview-max", 0, 20);
        if (v >= 0) gw_eink_set_scroll_preview_max((uint32_t)v);
        v = diag_int_file("/run/gw-ui-test-preview-ms", 0, 2000);
        if (v >= 0) gw_eink_set_scroll_preview_ms((uint32_t)v);
        v = diag_int_file("/run/gw-ui-test-clean-ms", 0, 10000);
        if (v >= 0) gw_eink_set_scroll_clean_delay((uint32_t)v);
    }
#endif
    dprintf(2, "GW_SHELL: paint profile=%s rule=%s content=%d preview=%d/%dms snap=%d cleanup=%dms area=%d periodic=%d\n",
            profile->name, decision.rule, (int)profile->content,
            decision.preview_max, decision.preview_ms, decision.page_snap,
            profile->cleanup_ms, profile->cleanup_area,
            profile->periodic_full_after);
}

GW_SHELL_INTERNAL void gw_paint_policy_load(void) {
    const char *value;
    char buffer[32];
    /* Scene/scroll waveform selection is the only live paint configuration.
     * GC16 is opt-in via GW_UI_SCENE_WAVEFORM=gc16. */
    g_scene_waveform = WAVEFORM_MODE_GC16_FAST;

    value = getenv("GW_UI_SCENE_WAVEFORM");
    if (!value && gw_read_trimmed("/etc/gleworks/scene-waveform", buffer,
                                  sizeof(buffer)))
        value = buffer;
    {
        uint32_t selected = gw_waveform_from_name(value);
        if (selected) g_scene_waveform = selected;
    }
    gw_eink_set_scene_waveform(g_scene_waveform);

    value = getenv("GW_UI_SCROLL_WAVEFORM");
    if (!value && gw_read_trimmed("/etc/gleworks/scroll-waveform", buffer,
                                  sizeof(buffer)))
        value = buffer;
    {
        uint32_t selected = gw_waveform_from_name(value);
        if (selected) gw_eink_set_scroll_waveform(selected);
    }
    value = getenv("GW_UI_SCROLL_RELEASE_WAVEFORM");
    if (!value && gw_read_trimmed("/etc/gleworks/scroll-release-waveform",
                                  buffer, sizeof(buffer)))
        value = buffer;
    {
        uint32_t selected = gw_waveform_from_name(value);
        if (selected) gw_eink_set_scroll_release_waveform(selected);
    }
    value = getenv("GW_UI_SCROLL_PREVIEW_MS");
    if (value && value[0]) {
        int ms = atoi(value);
        if (ms >= 0 && ms <= 1000) gw_eink_set_scroll_preview_ms((uint32_t)ms);
    }
    value = getenv("GW_UI_SCROLL_PREVIEW_MAX");
    if (value && value[0]) {
        int n = atoi(value);
        if (n >= 0 && n <= 20) gw_eink_set_scroll_preview_max((uint32_t)n);
    }
    value = getenv("GW_UI_SCROLL_CLEAN_MS");
    if (value && value[0]) {
        int ms = atoi(value);
        if (ms >= 0 && ms <= 10000) gw_eink_set_scroll_clean_delay((uint32_t)ms);
    }
    value = getenv("GW_UI_SCROLL_CLEAN_MIN_MS");
    if (value && value[0]) {
        int ms = atoi(value);
        if (ms >= 0 && ms <= 60000) gw_eink_set_scroll_clean_min((uint32_t)ms);
    }
    dprintf(2, "GW_SHELL: scene=%u scroll=%u release=%u preview_ms=%u preview_max=%u\n",
            g_scene_waveform,
            gw_eink_scroll_waveform() ? gw_eink_scroll_waveform()
                                      : g_scene_waveform,
            gw_eink_scroll_release_waveform()
                ? gw_eink_scroll_release_waveform() : WAVEFORM_MODE_REAGL,
            gw_eink_scroll_preview_ms(), gw_eink_scroll_preview_max());
}




/* ── Screens and shared state ───────────────────────────────────────────── */






GW_SHELL_INTERNAL void apps_refresh(void);
GW_SHELL_INTERNAL void update_scan(void);

/* Home status bar and cards. */

/* Continue Reading. */

/* Home calendar / notes widgets. */

/* Settings facts. */


/* To-dos. */


/* Shared input state and diagnostics. */

/* Paint instrumentation for the host suite. */

GW_SHELL_INTERNAL void gw_ui_show(gw_screen_t screen);
GW_SHELL_INTERNAL void gw_scene_show(gw_screen_t screen, int deep_clean);
GW_SHELL_INTERNAL void status_refresh_now(void);
GW_SHELL_INTERNAL void lock_refresh_time(void);
GW_SHELL_INTERNAL void recovery_set(const char *text);

/* ── IPC client helper ───────────────────────────────────────────────────── */

#if GW_UI_HOST_SIM
/* Deterministic host responses — no device, no /dev/fb0, no /run socket. */

static const char *g_host_app_list_override;

GW_SHELL_INTERNAL void host_ipc_fail_next(const char *prefix) {
    g_host_ipc_fail_prefix = prefix;
}

GW_SHELL_INTERNAL void host_ipc_app_list_set(const char *response) {
    g_host_app_list_override = response;
}

GW_SHELL_INTERNAL int ipc_cmd(const char *cmd, char *resp, size_t resp_size) {
    if (!cmd || !resp || resp_size == 0) return -1;
    resp[0] = '\0';
    snprintf(g_host_last_ipc, sizeof(g_host_last_ipc), "%s", cmd);
    if (g_host_ipc_fail_prefix && !strncmp(cmd, g_host_ipc_fail_prefix,
                                           strlen(g_host_ipc_fail_prefix))) {
        g_host_ipc_fail_prefix = NULL;
        return -1;
    }
    if (!strcmp(cmd, GW_CMD_STATUS)) {
        snprintf(resp, resp_size,
                 "STATUS version=host boot=preview power=battery battery=82 "
                 "light=37 foreground=none net=connected usb=none "
                 "reader-exit=0 lock=unlocked wifi-error=0 wifi-busy=0\n");
        return 0;
    }
    if (!strcmp(cmd, "NET SCAN")) {
        snprintf(resp, resp_size, "OK\n");
        return 0;
    }
    if (!strcmp(cmd, GW_CMD_NET_STATUS)) {
        static const char ssid[] = "Home Wi-Fi";
        size_t i, used = (size_t)snprintf(resp, resp_size, "NET connected ");
        for (i = 0; ssid[i] && used + 2 < resp_size; i++)
            used += (size_t)snprintf(resp + used, resp_size - used, "%02x",
                                     (unsigned char)ssid[i]);
        snprintf(resp + used, resp_size - used, " 192.168.1.200\n");
        return 0;
    }
    if (!strncmp(cmd, "NET RESULT ", 11)) {
        static const struct { const char *ssid; int signal; int secure; } nets[] = {
            { "Home Wi-Fi", -50, 1 },
            { "Library Wi-Fi", -70, 0 },
            { "Cafe", -78, 0 }
        };
        unsigned index = (unsigned)atoi(cmd + 11);
        if (index >= sizeof(nets) / sizeof(nets[0])) return -1;
        {
            const unsigned char *s = (const unsigned char *)nets[index].ssid;
            size_t i, used = (size_t)snprintf(resp, resp_size, "NET SSID ");
            for (i = 0; s[i] && used + 2 < resp_size; i++)
                used += (size_t)snprintf(resp + used, resp_size - used, "%02x", s[i]);
            snprintf(resp + used, resp_size - used, " %d %d\n",
                     nets[index].signal, nets[index].secure);
        }
        return 0;
    }
    if (!strcmp(cmd, GW_CMD_LIGHT_GET)) {
        snprintf(resp, resp_size, "LIGHT 37\n");
        return 0;
    }
    if (!strcmp(cmd, "APP COUNT")) {
        snprintf(resp, resp_size, "APPS 8\n");
        return 0;
    }
    if (!strcmp(cmd, "APP LIST")) {
        static const char *const names[8] = {
            "notes", "dictionary", "clock", "gallery",
            "calc", "reader", "radio", "tools"
        };
        size_t used = 0;
        int i;
        if (g_host_app_list_override) {
            snprintf(resp, resp_size, "%s", g_host_app_list_override);
            return 0;
        }
        for (i = 0; i < 8; i++)
            used += (size_t)snprintf(resp + used, resp_size - used,
                                     "APP %s 1.%d.0 Host long-list fixture\n",
                                     names[i], i);
        snprintf(resp + used, resp_size - used, "APP END\n");
        return 0;
    }
    if (!strcmp(cmd, "NET SCAN RESULTS")) {
        snprintf(resp, resp_size,
                 "NET RESULTS 3\n"
                 "NET SSID 486f6d652057692d4669 -50 1\n"
                 "NET SSID 4c6962726172792057692d4669 -70 0\n"
                 "NET SSID 43616665 -78 0\n"
                 "NET END\n");
        return 0;
    }
    if (!strncmp(cmd, "APP INFO ", 9)) {
        static const char *const names[8] = {
            "notes", "dictionary", "clock", "gallery",
            "calc", "reader", "radio", "tools"
        };
        int index = atoi(cmd + 9);
        if (index < 0 || index >= 8) {
            snprintf(resp, resp_size, "APP END\n");
        } else {
            snprintf(resp, resp_size, "APP %s 1.%d.0 %s\n",
                     names[index], index, "Host long-list fixture");
        }
        return 0;
    }
    if (!strcmp(cmd, "APP SCAN")) {
        snprintf(resp, resp_size, "APP USB 1\n");
        return 0;
    }
    if (!strcmp(cmd, "APP USB 0")) {
        snprintf(resp, resp_size, "APP USBFILE notes-1.3.0.gwapp\n");
        return 0;
    }
    if (!strcmp(cmd, "APP USB 1")) {
        snprintf(resp, resp_size, "APP USB END\n");
        return 0;
    }
    if (!strncmp(cmd, "APP INSTALL USB", 15) || !strncmp(cmd, "APP REMOVE ", 11)) {
        snprintf(resp, resp_size, "OK\n");
        return 0;
    }
    snprintf(resp, resp_size, "OK\n");
    return 0;
}

GW_SHELL_INTERNAL int ipc_cmd_all(const char *cmd, char *resp, size_t resp_size) {
    /* The host fake already returns the complete multi-line response. */
    return ipc_cmd(cmd, resp, resp_size);
}
#else
/* ── Persistent IPC connection ───────────────────────────────────────────── */
static gw_ipc_conn_t g_ipc_conn = { -1 };

/* Send one command and read one line response.  gw_ipc_conn_cmd reconnects
 * once when the service closed a stale connection. */
GW_SHELL_INTERNAL int ipc_cmd(const char *cmd, char *resp, size_t resp_size) {
    return gw_ipc_conn_cmd(&g_ipc_conn, cmd, resp, resp_size);
}

/* Read every line of a bounded multi-line response.  The service keeps the
 * connection open after a batch handler, so read until the protocol
 * terminator rather than waiting for EOF or the socket timeout. */
GW_SHELL_INTERNAL int ipc_cmd_all(const char *cmd, char *resp, size_t resp_size) {
    const char *terminator = NULL;
    if (!cmd) return -1;
    if (!strcmp(cmd, "APP LIST")) terminator = "APP END";
    else if (!strcmp(cmd, "NET SCAN RESULTS")) terminator = "NET END";
    if (!terminator) return gw_ipc_conn_cmd(&g_ipc_conn, cmd, resp, resp_size);
    return gw_ipc_conn_cmd_until(&g_ipc_conn, cmd, terminator, resp, resp_size);
}
#endif

/* ── LVGL tick shim and small helpers ───────────────────────────────────── */

/* ── Service event stream (lock/wake/unlock) ────────────────────────────── */

/* A lock notification queued behind the unlock IPC must not re-engage the
 * lock screen.  Ignore lock/wake events for a short window after unlock. */

GW_SHELL_INTERNAL void evt_handle_line(const char *line) {
    if (!line || !line[0]) return;
    dprintf(2, "GW_SHELL: event %s\n", line);
    if (!strcmp(line, GW_EVT_PWR_LOCK) || !strcmp(line, GW_EVT_PWR_WAKE) ||
        !strcmp(line, GW_EVT_PWR_SUSPEND)) {
        if (g_ignore_lock_until_ms &&
            (int32_t)(gw_eink_now_ms() - g_ignore_lock_until_ms) < 0) {
            dprintf(2, "GW_SHELL: ignoring stale lock event\n");
            return;
        }
        /* The service owns the light and lock state.  This is the missing
         * half of a short press: put the lock screen on the panel *before*
         * the service suspends, instead of only turning the frontlight off.
         * Lock is the primary GC16 ghost-cleanup point (U3). */
        if (g_lock_screen && lv_scr_act() != g_lock_screen) {
            gw_scene_show(GW_SCREEN_LOCK, 1);
            dprintf(2, "GW_SHELL: lock screen engaged\n");
        }
    } else if (!strcmp(line, GW_EVT_PWR_UNLOCK)) {
        if (g_lock_screen && lv_scr_act() == g_lock_screen)
            gw_scene_show(GW_SCREEN_HOME, 0);
    } else if (!strcmp(line, GW_EVT_USB_CONNECTED) ||
               !strcmp(line, GW_EVT_USB_DISCONNECTED)) {
        /* Show/hide the USB indicator immediately, not on the 30 s poll. */
        status_refresh_now();
    }
}

GW_SHELL_INTERNAL void evt_close(void) {
    if (g_evt_fd >= 0) close(g_evt_fd);
    g_evt_fd = -1;
    g_evt_used = 0;
}

GW_SHELL_INTERNAL void evt_connect(void) {
    struct sockaddr_un addr;
    int fd;
    if (g_evt_fd >= 0) return;
    if (g_evt_retry_ms && (int32_t)(gw_eink_now_ms() - g_evt_retry_ms) < 0)
        return;
    g_evt_retry_ms = gw_eink_now_ms() + 2000;
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) return;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, GW_IPC_EVT_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 &&
        errno != EINPROGRESS) {
        close(fd);
        return;
    }
    g_evt_fd = fd;
    g_evt_used = 0;
    dprintf(2, "GW_SHELL: event socket connected\n");
}

GW_SHELL_INTERNAL void evt_poll(void) {
    for (;;) {
        ssize_t n;
        char *newline;
        if (g_evt_fd < 0) return;
        if (g_evt_used >= sizeof(g_evt_line) - 1) {
            g_evt_used = 0;   /* overlong/malformed event: drop the line */
        }
        n = read(g_evt_fd, g_evt_line + g_evt_used,
                 sizeof(g_evt_line) - 1 - g_evt_used);
        if (n > 0) {
            g_evt_used += (size_t)n;
            g_evt_line[g_evt_used] = '\0';
            while ((newline = strchr(g_evt_line, '\n')) != NULL) {
                size_t consumed = (size_t)(newline - g_evt_line) + 1;
                *newline = '\0';
                evt_handle_line(g_evt_line);
                memmove(g_evt_line, g_evt_line + consumed,
                        g_evt_used - consumed + 1);
                g_evt_used -= consumed;
            }
        } else if (n == 0) {
            evt_close();
            return;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        } else if (errno == EINTR) {
            continue;
        } else {
            evt_close();
            return;
        }
    }
}

GW_SHELL_INTERNAL uint32_t lv_tick_get_cb(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}


GW_SHELL_INTERNAL void label_set_if_changed(lv_obj_t *label, const char *text) {
    const char *current;
    if (label == NULL || text == NULL) return;
    current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
        g_ui_text_updates++;
    }
}

GW_SHELL_INTERNAL void container_set_hidden(lv_obj_t *obj, int hidden) {
    if (!obj) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

GW_SHELL_INTERNAL const char *gw_screen_name(gw_screen_t screen) {
    switch (screen) {
    case GW_SCREEN_HOME: return "home";
    case GW_SCREEN_SETTINGS: return "settings";
    case GW_SCREEN_CALENDAR: return "calendar";
    case GW_SCREEN_NOTES: return "todos";
    case GW_SCREEN_NETWORK: return "network";
    case GW_SCREEN_APPS: return "apps";
    case GW_SCREEN_UPDATE: return "update";
    case GW_SCREEN_HUB: return "hub";
    case GW_SCREEN_LOCK: return "lock";
    default: return "unknown";
    }
}

/* ── English date and calendar helpers ─────────────────────────────────── */

/* ── Status refresh ─────────────────────────────────────────────────────── */

GW_SHELL_INTERNAL void network_update_ui(void);
GW_SHELL_INTERNAL void network_scan_apply(void);
GW_SHELL_INTERNAL void network_build_rows(void);
GW_SHELL_INTERNAL void settings_facts_update(void);

GW_SHELL_INTERNAL void lock_apply_status(const char *status) {
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
GW_SHELL_INTERNAL void lock_sync_now(void) {
    char status[GW_IPC_MAX_LINE];
    if (ipc_cmd(GW_CMD_STATUS, status, sizeof(status)) == 0)
        lock_apply_status(status);
}

GW_SHELL_INTERNAL int hex_decode(const char *hex, char *out, size_t size) {
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
GW_SHELL_INTERNAL int net_result_parse(const char *response, gw_net_ap_t *ap) {
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

GW_SHELL_INTERNAL int signal_bars_for(int dbm) {
    if (dbm == 0) return 0;
    if (dbm >= -55) return 4;
    if (dbm >= -65) return 3;
    if (dbm >= -75) return 2;
    return 1;
}

GW_SHELL_INTERNAL const char *battery_icon(int percent, int charging) {
    if (charging) return LV_SYMBOL_CHARGE;
    if (percent < 0) return LV_SYMBOL_BATTERY_EMPTY;
    if (percent <= 10) return LV_SYMBOL_BATTERY_EMPTY;
    if (percent <= 40) return LV_SYMBOL_BATTERY_1;
    if (percent <= 70) return LV_SYMBOL_BATTERY_2;
    return LV_SYMBOL_BATTERY_3;
}

GW_SHELL_INTERNAL void status_wifi_bars(int level) {
    if (!g_wifi_bars) return;
    lv_obj_set_user_data(g_wifi_bars, (void *)(intptr_t)level);
    lv_obj_invalidate(g_wifi_bars);
}

/* Fetch NET STATUS into g_net_ssid/g_net_ip/g_net_state. */
GW_SHELL_INTERNAL int net_status_fetch(void) {
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

GW_SHELL_INTERNAL void network_scan_apply(void) {
    char response[GW_NET_LIST_MAX];
    char *line, *next;
    gw_net_ap_t found[GW_NET_MAX];
    int found_count = 0;
    int changed;
    unsigned i;
    if (ipc_cmd_all("NET SCAN RESULTS", response, sizeof(response)) != 0)
        return;
    for (line = response; line && *line && found_count < GW_NET_MAX;
         line = next) {
        gw_net_ap_t ap;
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        if (strncmp(line, "NET SSID ", 9)) continue;
        if (net_result_parse(line, &ap) != 0) continue;
        /* Hidden/blank SSIDs are not selectable; a whitespace-only or
         * non-printable name would otherwise render as an unexplained
         * empty row with signal bars. */
        {
            const unsigned char *c;
            int printable = 1;
            if (!ap.ssid[0] || strspn(ap.ssid, " ") == strlen(ap.ssid))
                printable = 0;
            for (c = (const unsigned char *)ap.ssid; printable && *c; c++)
                if (*c < 0x20 || *c == 0x7f) printable = 0;
            if (!printable) continue;
        }
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

GW_SHELL_INTERNAL void status_apply(const char *status) {
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

GW_SHELL_INTERNAL void status_refresh_now(void) {
    char status[GW_IPC_MAX_LINE];
    if (ipc_cmd(GW_CMD_STATUS, status, sizeof(status)) == 0)
        status_apply(status);
}

GW_SHELL_INTERNAL void status_update(lv_timer_t *timer) {
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
            gw_ui_kit_status_strip_update(
                clk, lv_label_get_text(g_battery_label));
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
        gw_ui_kit_status_strip_update(
            lv_label_get_text(g_clock_label),
            lv_label_get_text(g_battery_label));
    } else {
        label_set_if_changed(g_battery_label, "--");
        status_wifi_bars(0);
        label_set_if_changed(g_net_connection_label, GW_STR_STATUS_UNAVAILABLE);
        gw_ui_kit_status_strip_update(lv_label_get_text(g_clock_label), "--");
        return;
    }

    if (g_wifi_scan_pending && gw_eink_now_ms() - g_wifi_scan_started_ms >= 800)
        network_scan_apply();
}

/* One-second tick for the bounded scan result and post-connect status poll. */
GW_SHELL_INTERNAL void ui_tick_cb(lv_timer_t *timer) {
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
    lock_config_poll();
}

/* ── App launch, lock and light actions ─────────────────────────────────── */

GW_SHELL_INTERNAL void reader_launch(lv_event_t *event) {
    (void)event;
#if GW_UI_PERF
    GW_PERF_LOG("GW_UI: ui event reader t=%lld us\n", gw_now_us());
#endif
#if GW_UI_HOST_SIM
    return;
#elif GW_UI_LIFECYCLE_SPIKE
    char response[GW_IPC_MAX_LINE];
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
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

GW_SHELL_INTERNAL void lock_screen_cb(lv_event_t *event) {
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

GW_SHELL_INTERNAL void unlock_screen_cb(lv_event_t *event) {
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

GW_SHELL_INTERNAL void light_apply_ui(void) {
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

GW_SHELL_INTERNAL void light_step(int delta) {
    char response[GW_IPC_MAX_LINE], cmd[32];
    gw_epdc_cancel_scroll_cleanup();
    g_light_value += delta;
    if (g_light_value < 0) g_light_value = 0;
    if (g_light_value > 100) g_light_value = 100;
    /* Value text is optimistic; one LIGHT SET per step. */
    light_apply_ui();
    snprintf(cmd, sizeof(cmd), GW_CMD_LIGHT_SET " %d", g_light_value);
    ipc_cmd(cmd, response, sizeof(response));
    g_light_last = g_light_value;
}

GW_SHELL_INTERNAL void light_minus_cb(lv_event_t *event) { (void)event; light_step(-1); }
GW_SHELL_INTERNAL void light_plus_cb(lv_event_t *event) { (void)event; light_step(1); }

GW_SHELL_INTERNAL void light_set_abs(int value) {
    char response[GW_IPC_MAX_LINE], cmd[32];
    gw_epdc_cancel_scroll_cleanup();
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
GW_SHELL_INTERNAL void light_slider_pressed_cb(lv_event_t *event) {
    (void)event;
    gw_eink_request_scroll(1500);
}

GW_SHELL_INTERNAL void light_slider_released_cb(lv_event_t *event) {
    (void)event;
    if (g_light_bar) light_set_abs((int)lv_slider_get_value(g_light_bar));
    gw_eink_request_scroll(300);
}

/* D09: the status-bar Light target opens Settings scrolled to Frontlight. */
GW_SHELL_INTERNAL void light_open_cb(lv_event_t *event) {
    (void)event;
    gw_ui_show(GW_SCREEN_SETTINGS);
    if (g_settings_light_row && g_settings_content)
        lv_obj_scroll_to_view(g_settings_light_row, LV_ANIM_OFF);
}

/* ── Navigation callbacks ───────────────────────────────────────────────── */

GW_SHELL_INTERNAL void nav_home_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_HOME); }
GW_SHELL_INTERNAL void nav_library_cb(lv_event_t *event) { reader_launch(event); }
GW_SHELL_INTERNAL void nav_calendar_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_CALENDAR); }
GW_SHELL_INTERNAL void nav_todos_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_NOTES); }
GW_SHELL_INTERNAL void nav_settings_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_SETTINGS); }
GW_SHELL_INTERNAL void calendar_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_CALENDAR); }
GW_SHELL_INTERNAL void notes_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_NOTES); }

/* B05/C03: secondary screens remember which screen opened them. */
GW_SHELL_INTERNAL void gw_open_secondary(gw_screen_t screen, gw_screen_t back) {
    g_back_screen = back;
    gw_ui_show(screen);
}

GW_SHELL_INTERNAL void gw_back_cb(lv_event_t *event) {
    gw_screen_t back = g_back_screen;
    (void)event;
    g_back_screen = GW_SCREEN_HOME;
    gw_ui_show(back);
}

GW_SHELL_INTERNAL void network_open_cb(lv_event_t *event) {
    (void)event;
    gw_open_secondary(GW_SCREEN_NETWORK, g_active_screen);
}
GW_SHELL_INTERNAL void apps_open_cb(lv_event_t *event) {
    (void)event;
    gw_open_secondary(GW_SCREEN_APPS, g_active_screen);
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
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
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
                    /* Paint the resulting page in four progressive GC16
                     * bands instead of one large partial. */
                    gw_epdc_set_split_page(1);
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

/* ── Model application and screen switching ─────────────────────────────── */

GW_SHELL_INTERNAL void home_todo_ensure_row(int index) {
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

GW_SHELL_INTERNAL void home_todo_remove_row(int index) {
    lv_obj_t *label = g_widget_todo_rows[index];
    if (!label) return;
    lv_obj_del(lv_obj_get_parent(label));
    g_widget_todo_rows[index] = NULL;
}

GW_SHELL_INTERNAL void home_empty_ensure(void) {
    if (g_reading_empty || !g_home_reading_card) return;
    g_reading_empty = gw_ui_kit_empty_state_create(
        g_home_reading_card, GW_ICON_BOOK, GW_STR_NO_BOOK_OPEN,
        GW_STR_OPEN_LIBRARY_HINT, GW_STR_LIBRARY, reader_launch, 1);
}

GW_SHELL_INTERNAL void apply_model(void) {
    int has_book = g_model.book_title[0] != '\0';
    struct tm t;
    char text[256];
    int i, shown = 0, active = 0;
    gw_ui_now(&t);

    if (g_home_reading_card)
        lv_obj_set_height(g_home_reading_card,
                          has_book ? GW_UI_HOME_READING_CARD_H
                                   : GW_UI_HOME_EMPTY_CARD_H);
    if (!has_book) home_empty_ensure();
    /* With no book the eyebrow + "Tap to resume" header is redundant; the
     * compact empty state is the whole card. */
    if (g_reading_header) container_set_hidden(g_reading_header, !has_book);
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
    }

    {
        /* D02: the hero date above already shows weekday/month/year.  The
         * calendar card adds information instead of repeating it. */
        char day[8], month[48];
        int due_month = 0;
        int due_day = 0;
        char month_prefix[12];
        snprintf(day, sizeof(day), "%d", t.tm_mday);
        snprintf(month_prefix, sizeof(month_prefix), "%04d-%02d",
                 t.tm_year + 1900, t.tm_mon + 1);
        for (i = 0; i < g_todo_count; i++) {
            if (g_todos[i].status != GW_TODO_ACTIVE || !g_todos[i].due[0])
                continue;
            if (!strncmp(g_todos[i].due, month_prefix, 7)) due_month++;
            if (!strncmp(g_todos[i].due, month_prefix, 7) &&
                atoi(g_todos[i].due + 8) == t.tm_mday)
                due_day++;
        }
        if (due_month > 0)
            snprintf(month, sizeof(month), "%d due this month", due_month);
        else
            snprintf(month, sizeof(month), "No due dates this month");
        if (due_day > 0)
            snprintf(month, sizeof(month), "%d due today  |  %d this month",
                     due_day, due_month);
        label_set_if_changed(g_widget_day_label, day);
        label_set_if_changed(g_widget_month_label, month);
        container_set_hidden(g_widget_weekday_label, 1);
    }
    {
        struct tm week = t;
        int offset = (t.tm_wday + 6) % 7;
        int i;
        week.tm_mday -= offset;
        week.tm_hour = 12;
        week.tm_min = 0;
        week.tm_sec = 0;
        mktime(&week);
        for (i = 0; i < 7; i++) {
            lv_obj_t *label = g_widget_week_labels[i];
            char day[8];
            int is_today;
            if (!label) continue;
            snprintf(day, sizeof(day), "%d", week.tm_mday);
            is_today = week.tm_mday == t.tm_mday &&
                       week.tm_mon == t.tm_mon &&
                       week.tm_year == t.tm_year;
            label_set_if_changed(label, day);
            lv_obj_set_style_text_font(label,
                                       is_today ? &gw_font_32_bold
                                                : &gw_font_28,
                                       LV_PART_MAIN);
            lv_obj_set_style_text_decor(label,
                                        is_today ? LV_TEXT_DECOR_UNDERLINE
                                                 : LV_TEXT_DECOR_NONE,
                                        LV_PART_MAIN);
            lv_obj_set_style_text_color(label,
                                        is_today ? gw_ui_c_text()
                                                 : gw_ui_c_secondary(),
                                        LV_PART_MAIN);
            week.tm_mday++;
            mktime(&week);
        }
    }

    {
        int rows = 0;
        if (g_model.event_title[0]) {
            snprintf(text, sizeof(text), "%s | %s",
                     g_model.event_when[0] ? g_model.event_when : GW_STR_TODAY,
                     g_model.event_title);
            label_set_if_changed(g_widget_event_labels[0], text);
            rows = 1;
        } else {
            label_set_if_changed(g_widget_event_labels[0],
                                 GW_STR_NO_UPCOMING_EVENTS);
        }
        for (i = 0; i < g_todo_count && rows < 2; i++) {
            char due[32];
            if (g_todos[i].status != GW_TODO_ACTIVE || !g_todos[i].due[0])
                continue;
            format_due_label(g_todos[i].due, due, sizeof(due));
            snprintf(text, sizeof(text), "%s | %s", g_todos[i].text, due);
            label_set_if_changed(g_widget_event_labels[rows], text);
            rows++;
        }
        for (i = 0; i < 2; i++)
            container_set_hidden(g_widget_event_rows[i], i >= rows);
    }
    if (g_calendar_screen) calendar_detail_render();
}

GW_SHELL_INTERNAL void model_load(void) {
#if GW_UI_HOST_SIM
    snprintf(g_model.book_title, sizeof(g_model.book_title),
             "Những người khốn khổ");
    snprintf(g_model.book_author, sizeof(g_model.book_author), "Victor Hugo");
    g_model.book_progress = 42;
    snprintf(g_model.event_title, sizeof(g_model.event_title),
             "Đọc sách cùng con");
    snprintf(g_model.event_when, sizeof(g_model.event_when), "19:30");
#else
    /* No book metadata provider exists in the current IPC contract.  Keep a
     * real empty state until KOReader exposes a bounded current-book record;
     * do not invent a filesystem protocol or scrape reader internals here. */
    g_model.book_title[0] = '\0';
    g_model.book_author[0] = '\0';
    g_model.book_progress = -1;
    g_model.event_title[0] = '\0';
    g_model.event_when[0] = '\0';
#endif
    todo_store_load();
}

GW_SHELL_INTERNAL void build_all_screens(void) {
    build_home_screen();
    build_settings_screen();
    build_calendar_screen();
    build_notes_screen();
    build_network_screen();
    build_apps_screen();
    build_hub_screen();
    build_lock_screen();
}

GW_SHELL_INTERNAL void gw_scene_show(gw_screen_t screen, int deep_clean) {
    lv_obj_t *target = NULL;
    switch (screen) {
    case GW_SCREEN_HOME: target = g_home_screen; break;
    case GW_SCREEN_SETTINGS: target = g_settings_screen; break;
    case GW_SCREEN_CALENDAR: target = g_calendar_screen; break;
    case GW_SCREEN_NOTES: target = g_notes_screen; break;
    case GW_SCREEN_NETWORK: target = g_network_screen; break;
    case GW_SCREEN_APPS: target = g_apps_screen; break;
    case GW_SCREEN_UPDATE: target = g_update_screen; break;
    case GW_SCREEN_HUB: target = g_hub_screen; break;
    case GW_SCREEN_LOCK: target = g_lock_screen; break;
    default: return;
    }
    if (!target) return;
    gw_shell_apply_profile(screen);

    /* Leaving any screen dismisses both keyboards.  Otherwise a hidden
     * keyboard could remain visible when returning to Notes/Network while the
     * fast-text state was already off. */
    /* Leaving any screen dismisses both keyboards through the one reset
     * path, including EPDC input mode and list padding. */
    keyboard_dismiss();
    g_settle_refresh_pending = 0;

    /* Refresh screen-specific data before the single paint.  A second paint
     * after lv_scr_load() is a second EPDC submit and looks like tearing. */
    if (screen == GW_SCREEN_APPS) apps_refresh();
    else if (screen == GW_SCREEN_UPDATE) update_scan();
    else if (screen == GW_SCREEN_NETWORK) {
        net_status_fetch();
        network_update_ui();
    } else if (screen == GW_SCREEN_CALENDAR) {
        calendar_render();
        calendar_detail_render();
    } else if (screen == GW_SCREEN_SETTINGS) {
        settings_facts_update();
    } else if (screen == GW_SCREEN_LOCK) {
        /* Config and image are loaded at startup or by the background poll.
         * The lock transition itself must not touch /mnt/us: the service may
         * turn the display off immediately after the lock event. */
        lock_refresh_time();
    }

    if (g_active_screen == GW_SCREEN_HUB && screen != GW_SCREEN_HUB)
        gw_hub_set_view(0);

    g_active_screen = screen;
    if (screen == GW_SCREEN_HUB) gw_hub_set_view(1);
    gw_eink_request_scene();
    lv_scr_load(target);
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
    lv_refr_now(NULL);
    if (deep_clean) {
        /* U3: lock may do one full GC16 pass to clear residue.  It uses the
         * framebuffer that already contains the lock screen, so it is one
         * update, not a flash followed by a second repaint. */
        g_deep_clean_requests++;
        gw_epdc_request_full();
        gw_epdc_present();
    } else {
        gw_epdc_present();
    }
#else
    if (deep_clean) g_deep_clean_requests++;
#endif
}

GW_SHELL_INTERNAL void gw_ui_show(gw_screen_t screen) {
    gw_scene_show(screen, 0);
}

GW_SHELL_INTERNAL void gw_ui_build(void) {
#if !GW_UI_HOST_SIM
    /* One connection for the shell lifetime; ipc_cmd reconnects on error. */
    (void)gw_ipc_conn_open(&g_ipc_conn);
#endif
    model_load();
    build_all_screens();
    gw_hub_init();
    apply_model();
    /* Sync the service lock before the first paint.  A shell restarted during
     * a reader lock must show the lock screen once, not paint Home and then
     * immediately paint over it. */
    lock_sync_now();
    if (lv_scr_act() != g_lock_screen)
        gw_ui_show(GW_SCREEN_HOME);
    gw_ui_kit_set_full_paint_cb(gw_ui_request_full);
    gw_ui_kit_set_modal_close_cb(gw_ui_request_full);
    status_update(NULL);
    lv_timer_create(status_update, 30000, NULL);
    lv_timer_create(ui_tick_cb, 1000, NULL);
}

/* ── Crash diagnostics ──────────────────────────────────────────────────── */
/* Only async-signal-safe calls here: write(2) and _exit(2).  No stdio,
 * backtrace, malloc or locale functions (see signal-safety(7)). */
GW_SHELL_INTERNAL void crash_handler(int sig, siginfo_t *info, void *ctx) {
    static const char prefix[] = "GW_SHELL: fatal signal ";
    char number[12];
    char *end = number + sizeof(number);
    unsigned value;
    (void)info;
    (void)ctx;
    *--end = '\n';
    value = sig < 0 ? 0u : (unsigned)sig;
    do {
        *--end = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    (void)!write(STDERR_FILENO, prefix, sizeof(prefix) - 1);
    (void)!write(STDERR_FILENO, end, (size_t)(number + sizeof(number) - end));
    _exit(128 + sig);
}

GW_SHELL_INTERNAL void install_crash_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

#if GW_UI_DIAG
#define GW_UI_DIAG_SOCKET "/run/gw-shell.diag"

GW_SHELL_INTERNAL void gw_diag_init(void) {
    struct sockaddr_un addr;
    int fd;
    unlink(GW_UI_DIAG_SOCKET);
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, GW_UI_DIAG_SOCKET, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return; }
    if (listen(fd, 4) < 0) { close(fd); unlink(GW_UI_DIAG_SOCKET); return; }
    chmod(GW_UI_DIAG_SOCKET, 0600);
    g_diag_fd = fd;
}

GW_SHELL_INTERNAL lv_obj_t *gw_diag_find_scroll(lv_obj_t *obj) {
    uint32_t i;
    if (!obj) return NULL;
    /* Labels report SCROLLABLE in LVGL 8 even when they do not scroll.  Only
     * target a real container with content and a useful viewport. */
    if (!lv_obj_check_type(obj, &lv_label_class) &&
        lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE) &&
        lv_obj_get_child_cnt(obj) > 0 && lv_obj_get_height(obj) >= 100)
        return obj;
    for (i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *found = gw_diag_find_scroll(lv_obj_get_child(obj, (int32_t)i));
        if (found) return found;
    }
    return NULL;
}

GW_SHELL_INTERNAL void gw_diag_handle(const char *cmd) {
    int x, y, dy = 0;
    dprintf(2, "GW_UI_DIAG: cmd=%s\n", cmd);
    if (!strcmp(cmd, "NAV HOME")) {
        gw_ui_show(GW_SCREEN_HOME);
    } else if (!strcmp(cmd, "NAV SETTINGS")) {
        gw_ui_show(GW_SCREEN_SETTINGS);
    } else if (!strcmp(cmd, "NAV CALENDAR")) {
        gw_ui_show(GW_SCREEN_CALENDAR);
    } else if (!strcmp(cmd, "NAV NOTES") || !strcmp(cmd, "NAV TODOS")) {
        gw_ui_show(GW_SCREEN_NOTES);
    } else if (!strcmp(cmd, "NAV WIFI") || !strcmp(cmd, "NAV NETWORK")) {
        gw_ui_show(GW_SCREEN_NETWORK);
    } else if (!strcmp(cmd, "NAV APPS") || !strcmp(cmd, "NAV APPLICATIONS")) {
        gw_ui_show(GW_SCREEN_APPS);
    } else if (!strcmp(cmd, "NAV UPDATE") || !strcmp(cmd, "NAV UPDATES")) {
        gw_ui_show(GW_SCREEN_UPDATE);
    } else if (!strcmp(cmd, "NAV HUB") || !strcmp(cmd, "NAV KINDLEHUB")) {
        gw_ui_show(GW_SCREEN_HUB);
    } else if (!strcmp(cmd, "SCREEN")) {
        dprintf(2, "GW_UI_DIAG: screen=%s\n", gw_screen_name(g_active_screen));
    } else if (!strncmp(cmd, "WIFICONNECT ", 12)) {
        char ssid_hex[65], pass_hex[129];
        char command[GW_IPC_MAX_LINE], response[GW_IPC_MAX_LINE];
        if (sscanf(cmd + 12, "%64s %128s", ssid_hex, pass_hex) == 2 &&
            strlen(ssid_hex) % 2 == 0 && strlen(pass_hex) % 2 == 0 &&
            strspn(ssid_hex, "0123456789abcdefABCDEF") == strlen(ssid_hex) &&
            strspn(pass_hex, "0123456789abcdefABCDEF") == strlen(pass_hex)) {
            snprintf(command, sizeof(command), GW_CMD_NET_CONNECT " %s %s",
                     ssid_hex, pass_hex);
            if (ipc_cmd(command, response, sizeof(response)) == 0)
                dprintf(2, "GW_UI_DIAG: wifi connect ok resp=%s", response);
            else
                dprintf(2, "GW_UI_DIAG: wifi connect ipc failed\n");
        } else {
            dprintf(2, "GW_UI_DIAG: wifi connect invalid\n");
        }
    } else if (!strcmp(cmd, "NETFILL")) {
        static const char *const names[10] = {
            "Alpha Network", "Bravo Network", "Charlie Network",
            "Delta Network", "Echo Network", "Foxtrot Network",
            "Golf Network", "Hotel Network", "India Network",
            "Juliet Network"
        };
        int i;
        g_net_ap_count = 10;
        for (i = 0; i < 10; i++) {
            snprintf(g_net_aps[i].ssid, sizeof(g_net_aps[i].ssid), "%s",
                     names[i]);
            g_net_aps[i].signal = -55 - i * 2;
            g_net_aps[i].secure = i % 2;
        }
        g_net_selected_ssid[0] = '\0';
        network_build_rows();
        network_update_ui();
    } else if (!strcmp(cmd, "TYPE")) {
        /* Diagnostic-only: emulate one text-field change plus the key
         * highlight dirty region, without injecting touch events. */
        static int setup;
        if (g_net_password && g_net_keyboard) {
            lv_area_t kb, key;
            if (!setup) {
                gw_ui_show(GW_SCREEN_NETWORK);
                g_text_input_active = 1;
                gw_eink_set_text_input(1);
                lv_obj_clear_flag(g_net_keyboard, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(g_net_keyboard);
                lv_keyboard_set_textarea(g_net_keyboard, g_net_password);
                setup = 1;
            }
            gw_eink_note_input();
            lv_textarea_add_text(g_net_password, "a");
            lv_obj_get_coords(g_net_keyboard, &kb);
            key.x1 = kb.x1 + 6;
            key.y1 = kb.y1 + 6;
            key.x2 = kb.x1 + 100;
            key.y2 = kb.y1 + 110;
            lv_obj_invalidate_area(g_net_keyboard, &key);
        }
    } else if (sscanf(cmd, "TAPDOWN %d %d", &x, &y) == 2) {
        if (g_indev) {
            g_last_input_us = gw_now_us();
            gw_eink_note_input();
            g_touch_x = x;
            g_touch_y = y;
            g_touching = 1;
            lv_indev_read_timer_cb(g_indev->driver->read_timer);
        }
    } else if (!strcmp(cmd, "TAPUP")) {
        if (g_indev) {
            g_last_input_us = gw_now_us();
            gw_eink_note_input();
            g_touching = 0;
            lv_indev_read_timer_cb(g_indev->driver->read_timer);
        }
    } else if (sscanf(cmd, "DRAG %d", &dy) == 1) {
        /* Diagnostic-only held drag: emulates finger-down scroll frames so
         * the live-preview throttle can be measured on the panel. */
        lv_obj_t *scroll = gw_diag_find_scroll(lv_scr_act());
        if (scroll) {
            int step;
            gw_eink_set_touch_down(1);
            gw_eink_request_scroll(1500);
            for (step = 1; step <= 4; step++) {
                lv_obj_scroll_by(scroll, 0, dy / 4, LV_ANIM_OFF);
                lv_refr_now(NULL);
                gw_epdc_present();
                usleep(150000);
            }
            gw_eink_set_touch_down(0);
            gw_eink_request_scroll(400);
            lv_refr_now(NULL);
            gw_epdc_present();
            dprintf(2, "GW_UI_DIAG: drag dy=%d top=%d\n", dy,
                    (int)lv_obj_get_scroll_top(scroll));
        }
    } else if (sscanf(cmd, "SCROLL %d", &dy) == 1) {
        /* Diagnostic-only viewport move: exercises the scroll intent and the
         * on-release single-submit path when evdev injection is unavailable. */
        lv_obj_t *scroll = gw_diag_find_scroll(lv_scr_act());
        if (scroll) {
            gw_eink_note_input();
            gw_eink_request_scroll(400);
            lv_obj_scroll_by(scroll, 0, dy, LV_ANIM_OFF);
            lv_refr_now(NULL);
            dprintf(2, "GW_UI_DIAG: scroll dy=%d top=%d\n", dy,
                    (int)lv_obj_get_scroll_top(scroll));
        }
    } else if (sscanf(cmd, "SWIPE %d %d %d", &x, &y, &dy) == 3) {
        /* Diagnostic-only drag: press, move in four steps, release.  Verifies
         * on-release scrolling and the single-submit rule without a finger. */
        if (g_indev) {
            int step;
            g_last_input_us = gw_now_us();
            gw_eink_note_input();
            g_touch_x = x;
            g_touch_y = y;
            g_touching = 1;
            gw_eink_set_touch_down(1);
            lv_indev_read_timer_cb(g_indev->driver->read_timer);
            for (step = 1; step <= 4; step++) {
                g_last_input_us = gw_now_us();
                gw_eink_note_input();
                g_touch_y = y + dy * step / 4;
                lv_indev_read_timer_cb(g_indev->driver->read_timer);
                lv_timer_handler();
                usleep(60000);   /* diagnostic only: emulate a real drag */
            }
            g_last_input_us = gw_now_us();
            gw_eink_note_input();
            g_touching = 0;
            gw_eink_set_touch_down(0);
            lv_indev_read_timer_cb(g_indev->driver->read_timer);
            dprintf(2, "GW_UI_DIAG: swipe x=%d y=%d dy=%d\n", x, y, dy);
        }
    } else if (sscanf(cmd, "TAP %d %d", &x, &y) == 2) {
        if (g_indev) {
            /* Instrumented taps use the same input-to-render clocks as real
             * touch, so software-path latency can be compared before/after. */
            g_last_input_us = gw_now_us();
            gw_eink_note_input();
            g_touch_x = x;
            g_touch_y = y;
            g_touching = 1;
            lv_indev_read_timer_cb(g_indev->driver->read_timer);
            g_touching = 0;
            lv_indev_read_timer_cb(g_indev->driver->read_timer);
        }
    }
}

GW_SHELL_INTERNAL void gw_diag_poll(void) {
    int client;
    char buf[GW_IPC_MAX_LINE];
    ssize_t n;
    struct ucred cred;
    socklen_t clen = sizeof(cred);
    if (g_diag_fd < 0) return;
    client = accept(g_diag_fd, NULL, NULL);
    if (client < 0) return;
    if (getsockopt(client, SOL_SOCKET, SO_PEERCRED, &cred, &clen) == 0 && cred.uid != 0) {
        close(client); return;
    }
    fcntl(client, F_SETFL, O_NONBLOCK);
    n = recv(client, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        char *nl;
        buf[n] = '\0';
        nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        gw_diag_handle(buf);
        send(client, "OK\n", 3, 0);
    } else {
        send(client, "ERR\n", 4, 0);
    }
    close(client);
}
#endif

/* ── Main ────────────────────────────────────────────────────────────────── */

#if !GW_UI_HOST_SIM
int main(void) {
    install_crash_handler();
#if GW_UI_LIFECYCLE_SPIKE && GW_ENABLE_HW_TRIAL
    dprintf(2, "GW_UI_TRIAL_FLAGS=1\n");
#endif
#if !GW_UI_HOST_SIM
    {
        struct rlimit core_limit;
        core_limit.rlim_cur = core_limit.rlim_max = RLIM_INFINITY;
        (void)setrlimit(RLIMIT_CORE, &core_limit);
    }
#endif
    if (getenv("GW_UI_TEST_CRASH")) raise(SIGSEGV);
    dprintf(2, "GW_SHELL: starting (EPDC_accepted=%d touch_accepted=%d)\n",
            GW_UI_EPDC_ACCEPTED, GW_UI_TOUCH_ACCEPTED);
#if GW_UI_PERF
    dprintf(2, "GW_UI: perf logging %s\n",
            gw_perf_log_enabled() ? "enabled" : "disabled");
#endif

    /* Query system service; log but don't abort if unavailable at spike stage. */
    char status[GW_IPC_MAX_LINE] = {0};
    if (ipc_cmd(GW_CMD_STATUS, status, sizeof(status)) == 0)
        dprintf(2, "GW_SHELL: %s", status);
    else
        dprintf(2, "GW_SHELL: gw-system not available (normal at spike stage)\n");

    lv_init();
    gw_paint_policy_load();
    uint32_t previous_tick = lv_tick_get_cb();

    if (lv_port_epdc_init() < 0) {
        dprintf(2, "GW_SHELL: EPDC init failed\n");
        return 1;
    }
    if (lv_port_input_init() < 0) {
        dprintf(2, "GW_SHELL: touch handoff failed\n");
        return 1;
    }

    gw_ui_build();
    if (getenv("GW_UI_SMOKE_FB")) {
        dprintf(2, "GW_SHELL: QEMU smoke screens built\n");
        return 0;
    }
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
    /* The bootloader splash is still on the panel when the shell starts; a
     * partial first paint leaves it visible as ghost.  Do one full GC16 on
     * the first Home paint of each boot (not after every reader exit). */
    if (access("/run/gw-first-home-clean", F_OK) != 0) {
        int marker = open("/run/gw-first-home-clean",
                          O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
        if (marker >= 0) close(marker);
        gw_epdc_request_full();
        gw_epdc_present();
    }
#endif
#if GW_UI_DIAG
    gw_diag_init();
#endif
    evt_connect();

    {
        char *exit_status = strstr(status, " reader-exit=");
        if (exit_status && atoi(exit_status + strlen(" reader-exit=")) != 0)
            recovery_set("Reader stopped unexpectedly.");
    }

    dprintf(2, "GW_SHELL: entering render loop\n");

    /* E-ink render loop: drive LVGL, then pause between refreshes.  Unlike an
     * LCD we do not drive a fixed FPS; a static screen performs no EPDC work. */
    while (!g_exit_for_reader) {
        uint32_t current_tick = lv_tick_get_cb();
#if GW_UI_PERF
        long long render_start, render_end;
#endif
        lv_tick_inc(current_tick - previous_tick);
        previous_tick = current_tick;
        gw_eink_set_text_input(g_text_input_active);
        gw_eink_update_state();
        uint32_t delay = 0;
#if GW_UI_PERF
        render_start = gw_now_us();
#endif
        delay = lv_timer_handler();
        /* The reader handoff is requested from an LVGL event callback.  Leave
         * immediately instead of sleeping in poll() for up to a second; the
         * supervisor starts the reader as soon as this process exits. */
        if (g_exit_for_reader) break;
#if GW_UI_PERF
        render_end = gw_now_us();
#endif
        /* A dismissed keyboard is an ordinary content paint.  It must never
         * trigger a GC16 flash (U1). */
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
        if (g_settle_refresh_pending && !g_text_input_active &&
            !g_eink_popup_valid) {
            g_settle_refresh_pending = 0;
            gw_eink_request_content();
        }
        if (gw_epdc_scroll_cleanup_due()) {
            dprintf(2, "GW_UI: scroll idle clean fired\n");
            gw_epdc_scroll_cleanup_done();
            if (g_scroll_cleanup_valid && g_scroll_cleanup_area_only)
                g_force_scroll_cleanup = 1;
            else
                gw_epdc_request_full_waveform_reason(WAVEFORM_MODE_GC16,
                                                      "scroll_cleanup");
            gw_epdc_present();
        }
        if (g_dirty_valid || gw_eink_scroll_cleanup_pending())
            gw_epdc_present();
#endif
#if GW_UI_DIAG
        gw_diag_poll();
#endif
#if GW_UI_PERF
        GW_PERF_LOG("GW_UI: render handler_us=%lld delay=%u dirty=%d eink_state=%d input_to_render_us=%lld\n",
                    render_end - render_start, delay,
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
                g_dirty_valid,
#else
                0,
#endif
                (int)g_eink_state,
                g_last_input_us ? (render_start - g_last_input_us) : -1);
#endif
        if (delay > 1000) delay = 1000;
#if GW_UI_EPDC_ACCEPTED || GW_ENABLE_HW_TRIAL
        if (g_dirty_valid && delay > 8) delay = 8;
#endif
        if (g_evt_fd < 0) evt_connect();
        {
            struct pollfd pfds[2];
            int nfds = 0, touch_i = -1, evt_i = -1, ready;
            if (g_touch_fd >= 0) {
                touch_i = nfds;
                pfds[nfds].fd = g_touch_fd;
                pfds[nfds].events = POLLIN;
                pfds[nfds].revents = 0;
                nfds++;
            }
            if (g_evt_fd >= 0) {
                evt_i = nfds;
                pfds[nfds].fd = g_evt_fd;
                pfds[nfds].events = POLLIN;
                pfds[nfds].revents = 0;
                nfds++;
            }
            if (!nfds) {
                usleep((useconds_t)delay * 1000);
            } else {
                ready = poll(pfds, (nfds_t)nfds, (int)delay);
                if (ready > 0) {
                    if (touch_i >= 0 && (pfds[touch_i].revents & POLLIN) &&
                        g_indev != NULL && g_indev->driver->read_timer != NULL) {
                        uint32_t now = lv_tick_get_cb();
                        lv_tick_inc(now - previous_tick);
                        previous_tick = now;
                        lv_timer_ready(g_indev->driver->read_timer);
                    }
                    if (evt_i >= 0 &&
                        (pfds[evt_i].revents & (POLLIN | POLLHUP | POLLERR)))
                        evt_poll();
                }
            }
        }
    }

    return 0;
}
#endif /* !GW_UI_HOST_SIM */

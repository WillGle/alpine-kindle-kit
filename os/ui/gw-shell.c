/* SPDX-License-Identifier: GPL-3.0-or-later
 * Gleworks OS shell — reading-first Home, Calendar, Notes, Network and
 * Settings UI for the PW3 (1072x1448, 8 bpp EPDC).
 *
 * Renders through LVGL + the custom framebuffer/EPDC port.  All hardware
 * operations still go through gw-system IPC; this file never writes sysfs or
 * touches the panel directly.
 *
 * Layout rules live in gw_ui_style.h.  Fonts (with Vietnamese coverage) live
 * in gw_fonts.c.  Screens are built exactly once and updated by changing text
 * only, so a clock/battery refresh dirties a few labels instead of rebuilding
 * the UI.
 *
 * STATUS: SPIKE (EPDC acceptance still requires the deferred PW3 battery run).
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#if __has_include(<execinfo.h>)
#include <execinfo.h>
#define GW_HAS_BACKTRACE 1
#else
#define GW_HAS_BACKTRACE 0
#endif
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <lvgl/lvgl.h>
#include "lv_port_epdc.h"
#include "lv_port_input.h"
#include "gw_ui_style.h"
#include "gw_fonts.h"
#include "gw_ui_kit.h"
#include "gw_ui_strings.h"
#include "gw_icons.h"
#include "../system/gw-ipc.h"

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
#define GW_TODO_MAX 64
#define GW_TODO_TEXT_MAX 160

/* Paint policy.  The fast path is useful for latency, but DU4 plus a
 * deferred full-screen cleanup can look like ghosting followed by a flash.
 * Quality paints each screen transition once with GC16; balanced keeps DU4
 * and cleans every fourth transition; fast retains the per-transition REAGL
 * cleanup.  Select at runtime with GW_UI_PAINT_POLICY or
 * /etc/gleworks/paint-policy. */
typedef enum {
    GW_PAINT_QUALITY = 0,
    GW_PAINT_BALANCED = 1,
    GW_PAINT_FAST = 2
} gw_paint_policy_t;

static gw_paint_policy_t g_paint_policy = GW_PAINT_QUALITY;
static unsigned g_balanced_transitions;

static const char *gw_paint_policy_name(void) {
    switch (g_paint_policy) {
    case GW_PAINT_FAST: return "fast";
    case GW_PAINT_BALANCED: return "balanced";
    default: return "quality";
    }
}

static void gw_paint_policy_load(void) {
    const char *value = getenv("GW_UI_PAINT_POLICY");
    char buffer[32];
    if (!value) {
        FILE *file = fopen("/etc/gleworks/paint-policy", "r");
        if (file) {
            if (fgets(buffer, sizeof(buffer), file)) {
                char *end = buffer + strlen(buffer);
                while (end > buffer && (end[-1] == '\n' || end[-1] == '\r' ||
                                        end[-1] == ' ' || end[-1] == '\t'))
                    *--end = '\0';
                value = buffer;
            }
            fclose(file);
        }
    }
    if (value && !strcmp(value, "fast"))
        g_paint_policy = GW_PAINT_FAST;
    else if (value && !strcmp(value, "balanced"))
        g_paint_policy = GW_PAINT_BALANCED;
    else
        g_paint_policy = GW_PAINT_QUALITY;
    dprintf(2, "GW_SHELL: paint policy=%s\n", gw_paint_policy_name());
}

enum { GW_TODO_ACTIVE = 0, GW_TODO_DONE = 1, GW_TODO_DELETED = 2 };

typedef struct {
    int id;
    int status;
    long created;
    long updated;
    char due[11];                  /* YYYY-MM-DD, empty = no due date */
    char text[GW_TODO_TEXT_MAX];
} gw_todo_t;

static gw_todo_t g_todos[GW_TODO_MAX];
static int g_todo_count;
static int g_todo_next_id = 1;
static int g_todo_history;
static lv_obj_t *g_todo_list;
static lv_obj_t *g_todo_filter_active;
static lv_obj_t *g_todo_filter_history;

/* ── Screens and shared state ───────────────────────────────────────────── */

typedef enum {
    GW_SCREEN_HOME = 0,
    GW_SCREEN_SETTINGS,
    GW_SCREEN_CALENDAR,
    GW_SCREEN_NOTES,
    GW_SCREEN_NETWORK,
    GW_SCREEN_APPS,
    GW_SCREEN_UPDATE,
    GW_SCREEN_LOCK,
    GW_SCREEN_COUNT
} gw_screen_t;

typedef struct {
    char book_title[160];
    char book_author[96];
    int  book_progress;             /* -1 = unavailable/invalid */
    char note_preview[256];
    char event_title[160];
    char event_when[64];
} gw_ui_model_t;

static gw_ui_model_t g_model;
static int g_exit_for_reader;
static int g_active_screen = GW_SCREEN_HOME;

static lv_obj_t *g_home_screen;
static lv_obj_t *g_settings_screen;
static lv_obj_t *g_calendar_screen;
static lv_obj_t *g_notes_screen;
static lv_obj_t *g_network_screen;
static lv_obj_t *g_apps_screen;
static lv_obj_t *g_update_screen;
static lv_obj_t *g_lock_screen;

/* Incremental app list state. */
#define GW_APP_MAX 32
typedef struct {
    char id[64];
    char version[32];
    char description[128];
} gw_app_entry_t;
static gw_app_entry_t g_app_entries[GW_APP_MAX];
static int g_app_count;
static lv_obj_t *g_apps_list;
static lv_obj_t *g_apps_message;
static lv_obj_t *g_apps_empty_label;
static lv_obj_t *g_app_rows[GW_APP_MAX];
static lv_obj_t *g_app_detail_labels[GW_APP_MAX];
static unsigned g_apps_full_rebuilds;
static unsigned g_apps_row_updates;
static lv_obj_t *g_update_list;
static lv_obj_t *g_update_message;

static void apps_refresh(void);
static void update_scan(void);

/* Home status bar and cards. */
static lv_obj_t *g_clock_label;
static lv_obj_t *g_wifi_label;
static lv_obj_t *g_battery_label;
static lv_obj_t *g_date_label;
static lv_obj_t *g_wifi_button;
static lv_obj_t *g_light_button;
static lv_obj_t *g_lock_button;
static lv_obj_t *g_battery_button;
static lv_obj_t *g_home_reading_card;
static lv_obj_t *g_home_calendar_card;
static lv_obj_t *g_home_notes_card;

/* Continue Reading. */
static lv_obj_t *g_reading_body;
static lv_obj_t *g_reading_empty;
static lv_obj_t *g_book_title_label;
static lv_obj_t *g_book_author_label;
static lv_obj_t *g_book_progress_label;
static lv_obj_t *g_book_bar;

/* Home calendar / notes widgets. */
static lv_obj_t *g_widget_day_label;
static lv_obj_t *g_widget_month_label;
static lv_obj_t *g_widget_weekday_label;
static lv_obj_t *g_widget_event_label;
static lv_obj_t *g_widget_todo_count;
static lv_obj_t *g_widget_todo_rows[5];
static lv_obj_t *g_widget_todo_more;

/* Settings facts. */
static lv_obj_t *g_settings_battery_label;
static lv_obj_t *g_settings_storage_label;
static lv_obj_t *g_settings_version_label;
static lv_obj_t *g_settings_uptime_label;
static lv_obj_t *g_settings_date_label;
static lv_obj_t *g_settings_content;
static lv_obj_t *g_settings_wifi_label;
static lv_obj_t *g_settings_battery_row;
static lv_obj_t *g_settings_storage_row;
static lv_obj_t *g_settings_uptime_row;
static lv_obj_t *g_light_label;
static lv_obj_t *g_light_bar;
static int g_light_last = -1;
static int g_light_value = 37;

/* Calendar screen. */
#define GW_CAL_ROWS 6
#define GW_CAL_COLS 7
#define GW_CAL_CELLS (GW_CAL_ROWS * GW_CAL_COLS)
static lv_obj_t *g_calendar_title_label;
static lv_obj_t *g_calendar_cells[GW_CAL_CELLS];
static lv_obj_t *g_calendar_day_labels[GW_CAL_CELLS];
static lv_obj_t *g_calendar_dots[GW_CAL_CELLS];
static lv_obj_t *g_calendar_detail_title;
static lv_obj_t *g_calendar_detail_rows[3];
static lv_obj_t *g_calendar_detail_empty;
static int g_calendar_year;
static int g_calendar_month;
static int g_calendar_selected_day;

/* To-dos. */
static lv_obj_t *g_notes_area;
static lv_obj_t *g_notes_status;
static lv_obj_t *g_notes_keyboard;
static lv_obj_t *g_todo_due_buttons[3];
static int g_todo_due_index;

/* Network. */
#define GW_NET_MAX 16
typedef struct {
    char ssid[65];
    int signal;
    int secure;
} gw_net_ap_t;
static gw_net_ap_t g_net_aps[GW_NET_MAX];
static int g_net_ap_count;
static int g_net_selected;
static char g_net_ssid[65];
static char g_net_ip[32];
static int g_net_state = -1;      /* 1 connected, 0 disconnected, -1 unknown */
static int g_net_signal;
static lv_obj_t *g_net_connection_label;
static lv_obj_t *g_net_detail_label;
static lv_obj_t *g_net_signal_bars;
static lv_obj_t *g_net_list;
static lv_obj_t *g_net_rows[GW_NET_MAX];
static lv_obj_t *g_net_empty_label;
static lv_obj_t *g_net_password_row;
static lv_obj_t *g_net_password;
static lv_obj_t *g_net_eye_button;
static lv_obj_t *g_net_message;
static lv_obj_t *g_net_connect_button;
static lv_obj_t *g_net_scan_button;
static lv_obj_t *g_net_disconnect_button;
static lv_obj_t *g_net_forget_button;
static lv_obj_t *g_net_keyboard;
static int g_net_password_visible;
static int g_wifi_scan_pending;
static uint32_t g_wifi_scan_started_ms;
static uint32_t g_net_poll_deadline_ms;
static uint32_t g_net_poll_next_ms;

/* Shared input state and diagnostics. */
static int g_text_input_active;
static lv_obj_t *g_lock_label;
static lv_obj_t *g_recovery_label;

/* Paint instrumentation for the host suite. */
static unsigned g_full_paint_requests;
static unsigned g_screen_full_paints;

static void gw_ui_show(gw_screen_t screen);
static void recovery_set(const char *text);

/* ── IPC client helper ───────────────────────────────────────────────────── */

#if GW_UI_HOST_SIM
/* Deterministic host responses — no device, no /dev/fb0, no /run socket. */
static int ipc_cmd(const char *cmd, char *resp, size_t resp_size) {
    if (!cmd || !resp || resp_size == 0) return -1;
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
#else
/*
 * Send one command to gw-system and read back one line response.
 * Returns 0 on success, -1 on error.  buf must be at least GW_IPC_MAX_LINE.
 */
static int ipc_cmd(const char *cmd, char *resp, size_t resp_size) {
    int fd;
    struct sockaddr_un addr;
    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    size_t used = 0;

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(fd); return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, GW_IPC_CMD_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }

    size_t len = strlen(cmd);
    if (write(fd, cmd, len) != (ssize_t)len ||
        write(fd, "\n", 1) != 1) {
        close(fd); return -1;
    }

    while (used + 1 < resp_size) {
        ssize_t n = read(fd, resp + used, resp_size - 1 - used);
        if (n <= 0) { close(fd); return -1; }
        used += (size_t)n;
        if (resp[used - 1] == '\n') break;
    }
    close(fd);
    if (!used || resp[used - 1] != '\n') return -1;
    resp[used] = '\0';
    return 0;
}
#endif

/* ── LVGL tick shim and small helpers ───────────────────────────────────── */

/* ── Service event stream (lock/wake/unlock) ────────────────────────────── */

static int g_evt_fd = -1;
static char g_evt_line[GW_IPC_MAX_LINE];
static size_t g_evt_used;
static uint32_t g_evt_retry_ms;

static void evt_handle_line(const char *line) {
    if (!line || !line[0]) return;
    dprintf(2, "GW_SHELL: event %s\n", line);
    if (!strcmp(line, GW_EVT_PWR_LOCK) || !strcmp(line, GW_EVT_PWR_WAKE) ||
        !strcmp(line, GW_EVT_PWR_SUSPEND)) {
        /* The service owns the light and lock state.  This is the missing
         * half of a short press: put the lock screen on the panel *before*
         * the service suspends, instead of only turning the frontlight off. */
        if (g_lock_screen && lv_scr_act() != g_lock_screen) {
            g_eink_no_full_until_ms = 0;
            g_active_screen = GW_SCREEN_LOCK;
            lv_scr_load(g_lock_screen);
            dprintf(2, "GW_SHELL: lock screen engaged\n");
        }
    } else if (!strcmp(line, GW_EVT_PWR_UNLOCK)) {
        if (g_lock_screen && lv_scr_act() == g_lock_screen)
            gw_ui_show(GW_SCREEN_HOME);
    }
}

static void evt_close(void) {
    if (g_evt_fd >= 0) close(g_evt_fd);
    g_evt_fd = -1;
    g_evt_used = 0;
}

static void evt_connect(void) {
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

static void evt_poll(void) {
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

static uint32_t lv_tick_get_cb(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static unsigned g_ui_text_updates;

static void label_set_if_changed(lv_obj_t *label, const char *text) {
    const char *current;
    if (label == NULL || text == NULL) return;
    current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
        g_ui_text_updates++;
    }
}

static void container_set_hidden(lv_obj_t *obj, int hidden) {
    if (!obj) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static const char *gw_screen_name(gw_screen_t screen) {
    switch (screen) {
    case GW_SCREEN_HOME: return "home";
    case GW_SCREEN_SETTINGS: return "settings";
    case GW_SCREEN_CALENDAR: return "calendar";
    case GW_SCREEN_NOTES: return "todos";
    case GW_SCREEN_NETWORK: return "network";
    case GW_SCREEN_APPS: return "apps";
    case GW_SCREEN_UPDATE: return "update";
    case GW_SCREEN_LOCK: return "lock";
    default: return "unknown";
    }
}

/* ── English date and calendar helpers ─────────────────────────────────── */

static void gw_ui_now(struct tm *out) {
#if GW_UI_HOST_SIM
    /* Deterministic preview clock: Saturday, 31 October 2026, 09:41. */
    memset(out, 0, sizeof(*out));
    out->tm_year = 126;      /* 2026 */
    out->tm_mon = 9;         /* October */
    out->tm_mday = 31;
    out->tm_hour = 9;
    out->tm_min = 41;
    out->tm_wday = 6;        /* Saturday */
    out->tm_yday = 303;
#else
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) *out = *t;
    else memset(out, 0, sizeof(*out));
#endif
}

static void format_hero_date(char *buf, size_t size) {
    struct tm t;
    gw_ui_now(&t);
    if (t.tm_year == 0) {
        snprintf(buf, size, "%s", GW_STR_DATE_UNAVAILABLE);
        return;
    }
    snprintf(buf, size, "%s, %d %s %d",
             gw_str_weekday[t.tm_wday], t.tm_mday,
             gw_str_month[t.tm_mon], t.tm_year + 1900);
}

static void format_month_title(int year, int month, char *buf, size_t size) {
    snprintf(buf, size, "%s %d", gw_str_month[month], year);
}

static void format_iso_date(int year, int month, int day, char *buf, size_t size) {
    snprintf(buf, size, "%04d-%02d-%02d", year, month + 1, day);
}

static void today_iso(char *buf, size_t size) {
    struct tm t;
    gw_ui_now(&t);
    format_iso_date(t.tm_year + 1900, t.tm_mon, t.tm_mday, buf, size);
}

static void iso_relative_days(int days, char *buf, size_t size) {
    struct tm t;
    gw_ui_now(&t);
    t.tm_mday += days;
    t.tm_hour = 12;
    t.tm_min = 0;
    t.tm_sec = 0;
    mktime(&t);
    format_iso_date(t.tm_year + 1900, t.tm_mon, t.tm_mday, buf, size);
}

static void format_due_label(const char *iso, char *buf, size_t size) {
    char today[11], tomorrow[11];
    int year, month, day;
    if (!iso || !iso[0]) {
        snprintf(buf, size, "%s", GW_STR_NO_DATE);
        return;
    }
    today_iso(today, sizeof(today));
    iso_relative_days(1, tomorrow, sizeof(tomorrow));
    if (!strcmp(iso, today)) {
        snprintf(buf, size, "%s", GW_STR_TODAY);
        return;
    }
    if (!strcmp(iso, tomorrow)) {
        snprintf(buf, size, "%s", GW_STR_TOMORROW);
        return;
    }
    if (sscanf(iso, "%d-%d-%d", &year, &month, &day) == 3 &&
        month >= 1 && month <= 12) {
        snprintf(buf, size, "%d %s", day, gw_str_month_short[month - 1]);
        return;
    }
    snprintf(buf, size, "%s", GW_STR_NO_DATE);
}

static int days_in_month(int year, int month) {
    static const int days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month];
}

/* ── TODO storage ───────────────────────────────────────────────────────── */

static void todo_rebuild_list(void);
static void todo_preview_refresh(void);

static int g_todo_last_deleted = -1;

static void todo_sanitize(char *dst, size_t dst_size, const char *src) {
    size_t j = 0;
    if (!dst || dst_size == 0) return;
    while (src && *src && j + 1 < dst_size) {
        char c = *src++;
        if (c == '\t' || c == '\r' || c == '\n') c = ' ';
        dst[j++] = c;
    }
    while (j > 0 && dst[j - 1] == ' ') j--;
    dst[j] = '\0';
}

static void todo_copy_due(char *dst, size_t dst_size, const char *src) {
    int year, month, day;
    if (!src || !src[0] || sscanf(src, "%d-%d-%d", &year, &month, &day) != 3 ||
        month < 1 || month > 12 || day < 1 || day > 31) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%04d-%02d-%02d", year, month, day);
}

#if !GW_UI_HOST_SIM
static void todo_store_save(void) {
    char tmp[128];
    FILE *f;
    int i;
    snprintf(tmp, sizeof(tmp), "%s.tmp", GW_TODO_PATH);
    f = fopen(tmp, "w");
    if (!f) return;
    for (i = 0; i < g_todo_count; i++)
        fprintf(f, "%d\t%d\t%ld\t%ld\t%s\t%s\n",
                g_todos[i].id, g_todos[i].status,
                g_todos[i].created, g_todos[i].updated,
                g_todos[i].due, g_todos[i].text);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    rename(tmp, GW_TODO_PATH);
}
#else
static void todo_store_save(void) { }
#endif

/* Tolerant TSV parser: five fields (legacy) or six fields with due date. */
static int todo_parse_line(const char *line, gw_todo_t *out) {
    char copy[512];
    char *fields[7];
    int nf = 0;
    char *p;
    size_t n;
    if (!line || !out) return -1;
    snprintf(copy, sizeof(copy), "%s", line);
    n = strlen(copy);
    while (n && (copy[n - 1] == '\n' || copy[n - 1] == '\r')) copy[--n] = '\0';
    fields[nf++] = copy;
    for (p = copy; *p && nf < 7; p++) {
        if (*p == '\t') { *p = '\0'; fields[nf++] = p + 1; }
    }
    if (nf < 5) return -1;
    out->id = atoi(fields[0]);
    out->status = atoi(fields[1]);
    out->created = atol(fields[2]);
    out->updated = atol(fields[3]);
    if (out->id <= 0 || out->status < GW_TODO_ACTIVE ||
        out->status > GW_TODO_DELETED)
        return -1;
    todo_copy_due(out->due, sizeof(out->due), nf >= 6 ? fields[4] : "");
    todo_sanitize(out->text, sizeof(out->text), nf >= 6 ? fields[5] : fields[4]);
    return out->text[0] ? 0 : -1;
}

static void todo_store_load(void) {
    g_todo_count = 0;
    g_todo_next_id = 1;
    g_todo_last_deleted = -1;
#if GW_UI_HOST_SIM
    {
        struct { int status; const char *due; const char *text; } mock[] = {
            { GW_TODO_ACTIVE,  "today",    "Mua pin CR2032 cho đồng hồ" },
            { GW_TODO_ACTIVE,  "today",    "Kiểm tra lịch sạc PW3" },
            { GW_TODO_ACTIVE,  "tomorrow", "Đọc 20 trang sách" },
            { GW_TODO_DONE,    "",         "Sạc Kindle PW3" },
            { GW_TODO_DELETED, "",         "Gọi điện cho Minh" }
        };
        unsigned i;
        for (i = 0; i < sizeof(mock) / sizeof(mock[0]) && g_todo_count < GW_TODO_MAX; i++) {
            gw_todo_t *t = &g_todos[g_todo_count++];
            t->id = g_todo_next_id++;
            t->status = mock[i].status;
            t->created = t->updated = (long)i;
            if (!strcmp(mock[i].due, "today")) today_iso(t->due, sizeof(t->due));
            else if (!strcmp(mock[i].due, "tomorrow"))
                iso_relative_days(1, t->due, sizeof(t->due));
            else t->due[0] = '\0';
            snprintf(t->text, sizeof(t->text), "%s", mock[i].text);
        }
    }
#else
    {
        FILE *f = fopen(GW_TODO_PATH, "r");
        char line[512];
        if (!f) return;
        while (fgets(line, sizeof(line), f) && g_todo_count < GW_TODO_MAX) {
            gw_todo_t parsed;
            if (todo_parse_line(line, &parsed) != 0) continue;
            g_todos[g_todo_count] = parsed;
            if (parsed.id >= g_todo_next_id) g_todo_next_id = parsed.id + 1;
            g_todo_count++;
        }
        fclose(f);
    }
#endif
    todo_preview_refresh();
}

static void todo_preview_refresh(void) {
    int i;
    g_model.note_preview[0] = '\0';
    for (i = 0; i < g_todo_count; i++) {
        if (g_todos[i].status == GW_TODO_ACTIVE) {
            snprintf(g_model.note_preview, sizeof(g_model.note_preview),
                     "%s", g_todos[i].text);
            break;
        }
    }
    if (!g_model.note_preview[0]) {
        for (i = 0; i < g_todo_count; i++) {
            if (g_todos[i].status == GW_TODO_DONE) {
                snprintf(g_model.note_preview, sizeof(g_model.note_preview),
                         "%s (done)", g_todos[i].text);
                break;
            }
        }
    }
    (void)g_model.note_preview;
}

static gw_todo_t *todo_find(int id) {
    int i;
    for (i = 0; i < g_todo_count; i++)
        if (g_todos[i].id == id) return &g_todos[i];
    return NULL;
}

static int todo_due_on(int year, int month, int day) {
    char iso[11];
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

static void todo_commit(void) {
    todo_store_save();
    todo_preview_refresh();
    todo_rebuild_list();
}

/* ── Status refresh ─────────────────────────────────────────────────────── */

static void network_update_ui(void);
static void network_scan_apply(void);
static void settings_facts_update(void);

static void lock_apply_status(const char *status) {
    const char *field;
    if (!status) return;
    field = strstr(status, " lock=");
    if (!field) return;
    field += strlen(" lock=");
    if (!strncmp(field, "locked", 6)) {
        if (g_lock_screen && lv_scr_act() != g_lock_screen) {
            g_eink_no_full_until_ms = 0;
            g_active_screen = GW_SCREEN_LOCK;
            lv_scr_load(g_lock_screen);
            dprintf(2, "GW_SHELL: startup/status lock engaged\n");
        }
    } else if (!strncmp(field, "unlocked", 8)) {
        if (g_lock_screen && lv_scr_act() == g_lock_screen)
            gw_ui_show(GW_SCREEN_HOME);
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
    unsigned i;
    g_net_ap_count = 0;
    g_net_selected = -1;
    for (i = 0; i < 50 && g_net_ap_count < GW_NET_MAX; i++) {
        gw_net_ap_t ap;
        snprintf(cmd, sizeof(cmd), "NET RESULT %u", i);
        if (ipc_cmd(cmd, response, sizeof(response)) != 0) break;
        if (net_result_parse(response, &ap) != 0) break;
        g_net_aps[g_net_ap_count++] = ap;
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
    char power[24] = "unknown", net[24] = "unknown";
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

    lock_apply_status(status);

    if (battery >= 0 && battery <= 100)
        snprintf(text, sizeof(text), "%d%%", battery);
    else if (!strcmp(power, "charging"))
        snprintf(text, sizeof(text), "Charging");
    else
        snprintf(text, sizeof(text), "--");
    label_set_if_changed(g_battery_label, text);
    label_set_if_changed(g_settings_battery_label, text);
    container_set_hidden(g_settings_battery_row, battery < 0 && strcmp(power, "charging"));

    if (!strcmp(net, "connected")) {
        if (net_status_fetch() == 0 && g_net_ssid[0]) {
            label_set_if_changed(g_wifi_label, g_net_ssid);
            snprintf(text, sizeof(text), "%s | %s", g_net_ssid,
                     g_net_ip[0] ? g_net_ip : GW_STR_CONNECTED);
        } else {
            label_set_if_changed(g_wifi_label, GW_STR_ONLINE);
            snprintf(text, sizeof(text), "%s", GW_STR_CONNECTED);
        }
        label_set_if_changed(g_settings_wifi_label, text);
        label_set_if_changed(g_net_connection_label, GW_STR_CONNECTED);
        if (g_net_ssid[0]) label_set_if_changed(g_net_detail_label, g_net_ssid);
    } else if (!strcmp(net, "disconnected")) {
        label_set_if_changed(g_wifi_label, GW_STR_OFFLINE);
        label_set_if_changed(g_settings_wifi_label, GW_STR_NOT_CONNECTED);
        label_set_if_changed(g_net_connection_label, GW_STR_NOT_CONNECTED);
        g_net_state = 0;
        g_net_ssid[0] = '\0';
        g_net_ip[0] = '\0';
    } else {
        label_set_if_changed(g_wifi_label, GW_STR_OFFLINE);
        label_set_if_changed(g_settings_wifi_label, GW_STR_UNKNOWN);
        label_set_if_changed(g_net_connection_label, GW_STR_UNKNOWN);
    }

    if (light >= 0 && light <= 100) {
        char light_text[16];
        g_light_value = light;
        snprintf(light_text, sizeof(light_text), "%d%%", light);
        label_set_if_changed(g_light_label, light_text);
        if (g_light_bar) lv_bar_set_value(g_light_bar, light, LV_ANIM_OFF);
        g_light_last = light;
    }
    network_update_ui();
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
        label_set_if_changed(g_wifi_label, GW_STR_OFFLINE);
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
    if (g_wifi_scan_pending && now - g_wifi_scan_started_ms >= 800) {
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
        if (g_lock_screen) lv_scr_load(g_lock_screen);
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
        gw_ui_show(GW_SCREEN_HOME);
    } else if (g_lock_label) {
        lv_label_set_text(g_lock_label, "Unlock unavailable");
    }
}

static void light_apply_ui(void) {
    char text[16];
    snprintf(text, sizeof(text), "%d%%", g_light_value);
    label_set_if_changed(g_light_label, text);
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

static void light_minus_cb(lv_event_t *event) { (void)event; light_step(-10); }
static void light_plus_cb(lv_event_t *event) { (void)event; light_step(10); }

/* ── Navigation callbacks ───────────────────────────────────────────────── */

static void nav_home_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_HOME); }
static void nav_library_cb(lv_event_t *event) { reader_launch(event); }
static void nav_calendar_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_CALENDAR); }
static void nav_todos_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_NOTES); }
static void nav_settings_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_SETTINGS); }
static void calendar_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_CALENDAR); }
static void notes_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_NOTES); }
static void network_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_NETWORK); }
static void apps_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_APPS); }
static void update_open_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_UPDATE); }

/* ── Common widget builders ─────────────────────────────────────────────── */

static lv_obj_t *make_screen(void) {
    return gw_ui_kit_page_create();
}

static lv_obj_t *make_hairline(lv_obj_t *parent, int width) {
    return gw_ui_kit_hairline_create(parent, width);
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int width,
                             int height, int secondary) {
    return gw_ui_kit_button_create(parent, text, width, height,
                                   secondary ? GW_UI_BUTTON_SECONDARY
                                             : GW_UI_BUTTON_PRIMARY);
}

static lv_obj_t *make_bottom_nav(lv_obj_t *scr, int active) {
    gw_ui_nav_cbs_t cbs = {
        nav_home_cb, nav_library_cb, nav_calendar_cb, nav_todos_cb,
        nav_settings_cb
    };
    gw_ui_kit_bottom_nav_create(scr, active, &cbs);
    return NULL;
}

static lv_obj_t *make_content(lv_obj_t *scr, int top) {
    return gw_ui_kit_content_create(scr, top);
}

static void gw_ui_request_full(void *user_data) {
    (void)user_data;
    g_full_paint_requests++;
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
    gw_epdc_request_full();
#endif
}

static void set_clickable(lv_obj_t *obj) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/* ── Home: status bar ───────────────────────────────────────────────────── */

static lv_obj_t *status_target_create(lv_obj_t *bar, const char *icon,
                                      int x, int width, lv_event_cb_t cb) {
    lv_obj_t *button = lv_btn_create(bar);
    lv_obj_set_size(button, width, GW_UI_STATUS_BAR_H);
    lv_obj_align(button, LV_ALIGN_RIGHT_MID, x, 0);
    gw_ui_style_flat(button);
    gw_ui_style_button_feedback(button, 0);
    if (icon && icon[0]) {
        lv_obj_t *icon_label = gw_ui_label(button, icon, &gw_font_24,
                                           gw_ui_c_text());
        lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 12, 0);
    }
    if (cb) lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    return button;
}

static void build_status_bar(lv_obj_t *scr) {
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_t *line;
    lv_obj_set_size(bar, GW_UI_PANEL_W, GW_UI_STATUS_BAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    gw_ui_style_plain_panel(bar);

    g_clock_label = gw_ui_label(bar, "00:00", &gw_font_36_bold, gw_ui_c_text());
    lv_obj_align(g_clock_label, LV_ALIGN_LEFT_MID, GW_UI_MARGIN, 0);

    g_battery_button = status_target_create(bar, NULL, -48, GW_UI_TOUCH_MIN,
                                            nav_settings_cb);
    g_battery_label = gw_ui_label(g_battery_button, "--", &gw_font_24_bold,
                                  gw_ui_c_text());
    lv_obj_center(g_battery_label);

    g_lock_button = status_target_create(bar, GW_ICON_LOCK, -144,
                                         GW_UI_TOUCH_MIN, lock_screen_cb);
    g_light_button = status_target_create(bar, NULL, -240, GW_UI_TOUCH_MIN,
                                          nav_settings_cb);
    {
        lv_obj_t *light = gw_ui_label(g_light_button, "Light", &gw_font_24_bold,
                                      gw_ui_c_text());
        lv_obj_center(light);
    }
    g_wifi_button = status_target_create(bar, NULL, -336, 220, network_open_cb);
    g_wifi_label = gw_ui_label(g_wifi_button, GW_STR_OFFLINE, &gw_font_24_bold,
                               gw_ui_c_text());
    lv_obj_set_width(g_wifi_label, 196);
    lv_obj_set_style_text_align(g_wifi_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(g_wifi_label, LV_LABEL_LONG_DOT);
    lv_obj_align(g_wifi_label, LV_ALIGN_RIGHT_MID, -12, 0);

    line = make_hairline(scr, GW_UI_PANEL_W);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, GW_UI_STATUS_BAR_H - GW_UI_HAIRLINE);
}

/* ── Home: Continue Reading ─────────────────────────────────────────────── */

static lv_obj_t *home_card(lv_obj_t *parent, lv_event_cb_t cb) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    gw_ui_style_card(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, GW_UI_SPACE_2, LV_PART_MAIN);
    set_clickable(card);
    lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, NULL);
    return card;
}

static void build_continue_card(lv_obj_t *parent) {
    lv_obj_t *card = home_card(parent, reader_launch);
    lv_obj_t *eyebrow;
    lv_obj_set_height(card, 440);
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
        lv_obj_set_style_bg_color(g_book_bar, lv_color_make(220, 220, 220),
                                  LV_PART_MAIN);
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

static void build_calendar_widget(lv_obj_t *parent) {
    lv_obj_t *card = home_card(parent, calendar_open_cb);
    lv_obj_t *row, *col;
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

    g_widget_event_label = gw_ui_label(card, GW_STR_NO_UPCOMING_EVENTS,
                                       &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(g_widget_event_label, LV_PCT(100));
    lv_label_set_long_mode(g_widget_event_label, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(g_widget_event_label, 1);

    {
        lv_obj_t *footer = gw_ui_label(card, GW_STR_VIEW_CALENDAR, &gw_font_28,
                                       gw_ui_c_muted());
        lv_obj_set_width(footer, LV_PCT(100));
    }
}

static void build_notes_widget(lv_obj_t *parent) {
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
    g_widget_todo_more = gw_ui_label(card, "", &gw_font_28, gw_ui_c_muted());
    lv_obj_set_width(g_widget_todo_more, LV_PCT(100));
    container_set_hidden(g_widget_todo_more, 1);

    {
        lv_obj_t *footer = gw_ui_label(card, GW_STR_VIEW_ALL, &gw_font_28,
                                       gw_ui_c_muted());
        lv_obj_set_width(footer, LV_PCT(100));
    }
}

static void build_home_screen(void) {
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

static void recovery_set(const char *text) {
    if (!g_recovery_label && g_home_screen) {
        g_recovery_label = gw_ui_label(g_home_screen, "", &gw_font_24,
                                       gw_ui_c_secondary());
        lv_obj_align(g_recovery_label, LV_ALIGN_BOTTOM_LEFT, GW_UI_MARGIN,
                     -GW_UI_NAV_BAR_H - 40);
    }
    if (g_recovery_label) lv_label_set_text(g_recovery_label, text);
}

/* ── Settings ───────────────────────────────────────────────────────────── */

static lv_obj_t *settings_section(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = gw_ui_kit_section_label_create(parent, text);
    lv_obj_set_style_pad_top(label, GW_UI_SPACE_2, LV_PART_MAIN);
    return label;
}

static lv_obj_t *settings_row(lv_obj_t *parent, const char *title,
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

static lv_obj_t *settings_info_row(lv_obj_t *parent, const char *title,
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

static void settings_storage_value(char *buf, size_t size) {
#if GW_UI_HOST_SIM
    snprintf(buf, size, "20.1 GB free");
#else
    struct statvfs vfs;
    if (statvfs("/var/lib/gleworks", &vfs) != 0 && statvfs("/", &vfs) != 0) {
        buf[0] = '\0';
        return;
    }
    {
        double free_bytes = (double)vfs.f_bavail * (double)vfs.f_frsize;
        snprintf(buf, size, "%.1f GB free", free_bytes / (1024.0 * 1024.0 * 1024.0));
    }
#endif
}

static void settings_uptime_value(char *buf, size_t size) {
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

static void settings_facts_update(void) {
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

static void build_settings_screen(void) {
    lv_obj_t *content, *row;
    g_settings_screen = make_screen();
    gw_ui_kit_title_bar_create(g_settings_screen, GW_STR_SETTINGS);
    make_bottom_nav(g_settings_screen, 4);
    content = gw_ui_kit_scroll_content_create(g_settings_screen,
                                              GW_UI_APP_BAR_H + GW_UI_GAP,
                                              GW_UI_NAV_BAR_H + GW_UI_GAP);
    g_settings_content = content;

    settings_section(content, GW_STR_READING);
    settings_row(content, GW_STR_LIBRARY_ROW, GW_STR_READER_SUB, GW_STR_OPEN,
                 reader_launch, NULL);

    settings_section(content, GW_STR_DISPLAY);
    row = lv_obj_create(content);
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
        g_light_bar = lv_bar_create(track);
        lv_obj_set_size(g_light_bar, 200, 12);
        lv_obj_center(g_light_bar);
        lv_bar_set_range(g_light_bar, 0, 100);
        lv_bar_set_value(g_light_bar, g_light_value, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(g_light_bar, lv_color_make(210, 210, 210),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_light_bar, gw_ui_c_text(), LV_PART_INDICATOR);
        lv_obj_set_style_radius(g_light_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(g_light_bar, 0, LV_PART_INDICATOR);
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

static void calendar_render(void);
static void calendar_detail_render(void);

static void calendar_select_day(int day) {
    g_calendar_selected_day = day;
    calendar_render();
    calendar_detail_render();
}

static void calendar_day_cb(lv_event_t *event) {
    int day = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    if (day >= 1 && day <= 31) calendar_select_day(day);
}

static void calendar_month_shift(int delta) {
    int year = g_calendar_year, month = g_calendar_month;
    struct tm t;
    month += delta;
    while (month < 0) { month += 12; year--; }
    while (month > 11) { month -= 12; year++; }
    g_calendar_year = year;
    g_calendar_month = month;
    gw_ui_now(&t);
    if (year == t.tm_year + 1900 && month == t.tm_mon)
        g_calendar_selected_day = t.tm_mday;
    {
        int dim = days_in_month(year, month);
        if (g_calendar_selected_day > dim) g_calendar_selected_day = dim;
        if (g_calendar_selected_day < 1) g_calendar_selected_day = 1;
    }
    calendar_render();
    calendar_detail_render();
    /* A month change may cause one full-screen GC16; day selection is a
     * partial update. */
    gw_ui_request_full(NULL);
}

static void calendar_prev_cb(lv_event_t *event) { (void)event; calendar_month_shift(-1); }
static void calendar_next_cb(lv_event_t *event) { (void)event; calendar_month_shift(1); }

static void calendar_open_todos_cb(lv_event_t *event) {
    (void)event;
    gw_ui_show(GW_SCREEN_NOTES);
}

static void calendar_render(void) {
    struct tm now;
    int first_wday, dim, cell, i;
    char title[64];
    gw_ui_now(&now);
    format_month_title(g_calendar_year, g_calendar_month, title, sizeof(title));
    label_set_if_changed(g_calendar_title_label, title);
    dim = days_in_month(g_calendar_year, g_calendar_month);
    /* Monday-first offset. */
    {
        struct tm first;
        memset(&first, 0, sizeof(first));
        first.tm_year = g_calendar_year - 1900;
        first.tm_mon = g_calendar_month;
        first.tm_mday = 1;
        first.tm_hour = 12;
        mktime(&first);
        first_wday = (first.tm_wday + 6) % 7;
    }
    cell = 1 - first_wday;
    for (i = 0; i < GW_CAL_CELLS; i++, cell++) {
        lv_obj_t *obj = g_calendar_cells[i];
        lv_obj_t *label = g_calendar_day_labels[i];
        lv_obj_t *dot = g_calendar_dots[i];
        int in_month = cell >= 1 && cell <= dim;
        int is_today = in_month && g_calendar_year == now.tm_year + 1900 &&
                       g_calendar_month == now.tm_mon && cell == now.tm_mday;
        int selected = in_month && cell == g_calendar_selected_day;
        char text[16];
        if (in_month) snprintf(text, sizeof(text), "%d", cell);
        else snprintf(text, sizeof(text), " ");
        label_set_if_changed(label, text);
        lv_obj_set_style_text_font(label,
                                   is_today ? &gw_font_32_bold : &gw_font_32,
                                   LV_PART_MAIN);
        lv_obj_set_style_text_color(label,
                                    in_month ? gw_ui_c_text() : gw_ui_c_muted(),
                                    LV_PART_MAIN);
        if (selected) {
            lv_obj_set_style_border_width(obj, 3, LV_PART_MAIN);
            lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL, LV_PART_MAIN);
            lv_obj_set_style_border_color(obj, gw_ui_c_text(), LV_PART_MAIN);
        } else if (is_today) {
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN);
            lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM,
                                         LV_PART_MAIN);
            lv_obj_set_style_border_color(obj, gw_ui_c_text(), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
        }
        if (in_month) set_clickable(obj);
        else lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(obj, (void *)(intptr_t)(in_month ? cell : 0));
        /* Event dots are created lazily so empty months allocate none. */
        if (in_month && todo_due_on(g_calendar_year, g_calendar_month, cell)) {
            if (!dot) {
                dot = lv_obj_create(obj);
                lv_obj_set_size(dot, 8, 8);
                lv_obj_set_style_bg_color(dot, gw_ui_c_text(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(dot, 4, LV_PART_MAIN);
                lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -20);
                g_calendar_dots[i] = dot;
            }
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
        } else if (dot) {
            lv_obj_del(dot);
            g_calendar_dots[i] = NULL;
        }
    }
}

static void calendar_detail_ensure(int index) {
    lv_obj_t *detail, *box, *title, *due;
    if (index < 0 || index >= 3 || g_calendar_detail_rows[index]) return;
    detail = lv_btn_create(g_calendar_detail_empty
                               ? lv_obj_get_parent(g_calendar_detail_empty)
                               : g_calendar_screen);
    lv_obj_set_width(detail, LV_PCT(100));
    lv_obj_set_height(detail, 96);
    gw_ui_style_flat(detail);
    lv_obj_set_flex_flow(detail, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(detail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(detail, GW_UI_SPACE_2, LV_PART_MAIN);
    box = gw_ui_label(detail, "[ ]", &gw_font_28,
                      gw_ui_c_secondary());
    title = gw_ui_label(detail, "", &gw_font_32, gw_ui_c_text());
    due = gw_ui_label(detail, "", &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_add_event_cb(detail, calendar_open_todos_cb, LV_EVENT_CLICKED, NULL);
    if (g_calendar_detail_empty)
        lv_obj_move_to_index(detail,
                             lv_obj_get_index(g_calendar_detail_empty));
    (void)box;
    (void)due;
    g_calendar_detail_rows[index] = detail;
}

static void calendar_detail_render(void) {
    char iso[11];
    int i, shown = 0;
    struct tm t;
    gw_ui_now(&t);
    format_iso_date(g_calendar_year, g_calendar_month, g_calendar_selected_day,
                    iso, sizeof(iso));
    label_set_if_changed(g_calendar_detail_title,
                         (g_calendar_year == t.tm_year + 1900 &&
                          g_calendar_month == t.tm_mon &&
                          g_calendar_selected_day == t.tm_mday)
                             ? GW_STR_DUE_THIS_DAY : GW_STR_UPCOMING);
    for (i = 0; i < g_todo_count && shown < 3; i++) {
        gw_todo_t *todo = &g_todos[i];
        if (todo->status != GW_TODO_ACTIVE || strcmp(todo->due, iso)) continue;
        calendar_detail_ensure(shown);
        {
            lv_obj_t *row = g_calendar_detail_rows[shown];
            lv_obj_t *box = lv_obj_get_child(row, 0);
            lv_obj_t *title = lv_obj_get_child(row, 1);
            lv_obj_t *due = lv_obj_get_child(row, 2);
            char due_text[32];
            container_set_hidden(row, 0);
            container_set_hidden(box, 0);
            label_set_if_changed(title, todo->text);
            format_due_label(todo->due, due_text, sizeof(due_text));
            label_set_if_changed(due, due_text);
            lv_obj_set_user_data(row, (void *)(intptr_t)todo->id);
        }
        shown++;
    }
    for (i = shown; i < 3; i++)
        if (g_calendar_detail_rows[i])
            container_set_hidden(g_calendar_detail_rows[i], 1);
    container_set_hidden(g_calendar_detail_empty, shown > 0);
    if (shown == 0) {
        /* The existing single system event is shown when it matches today. */
        if (g_calendar_year == t.tm_year + 1900 &&
            g_calendar_month == t.tm_mon && g_calendar_selected_day == t.tm_mday &&
            g_model.event_title[0]) {
            label_set_if_changed(g_calendar_detail_empty, g_model.event_title);
        } else {
            label_set_if_changed(g_calendar_detail_empty, GW_STR_NO_ITEMS_DAY);
        }
    }
}

static void build_calendar_screen(void) {
    lv_obj_t *content, *row, *line;
    struct tm t;
    int i;
    gw_ui_now(&t);
    g_calendar_year = t.tm_year + 1900;
    g_calendar_month = t.tm_mon;
    g_calendar_selected_day = t.tm_mday;

    g_calendar_screen = make_screen();
    gw_ui_kit_title_bar_create(g_calendar_screen, GW_STR_CALENDAR);
    make_bottom_nav(g_calendar_screen, 2);
    content = make_content(g_calendar_screen, GW_UI_APP_BAR_H + GW_UI_GAP);
    lv_obj_set_style_pad_row(content, GW_UI_SPACE_1, LV_PART_MAIN);

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *prev = gw_ui_kit_button_icon_create(row, GW_ICON_CHEVRON_LEFT,
                                                       NULL, 96, 96,
                                                       GW_UI_BUTTON_SECONDARY);
        g_calendar_title_label = gw_ui_label(row, "October 2026",
                                             &gw_font_44_bold, gw_ui_c_text());
        lv_obj_t *next = gw_ui_kit_button_icon_create(row, GW_ICON_CHEVRON_RIGHT,
                                                       NULL, 96, 96,
                                                       GW_UI_BUTTON_SECONDARY);
        lv_obj_add_event_cb(prev, calendar_prev_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(next, calendar_next_cb, LV_EVENT_CLICKED, NULL);
    }

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 48);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    for (i = 0; i < 7; i++) {
        lv_obj_t *label = gw_ui_label(row, gw_str_weekday_short[i], &gw_font_28,
                                      gw_ui_c_muted());
        lv_obj_set_flex_grow(label, 1);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
    line = make_hairline(content, GW_UI_CONTENT_W);
    (void)line;

    for (i = 0; i < GW_CAL_ROWS; i++) {
        int d;
        lv_obj_t *grid_row = lv_obj_create(content);
        lv_obj_set_width(grid_row, LV_PCT(100));
        lv_obj_set_height(grid_row, GW_UI_TOUCH_MIN);
        gw_ui_style_plain_panel(grid_row);
        lv_obj_set_flex_flow(grid_row, LV_FLEX_FLOW_ROW);
        for (d = 0; d < GW_CAL_COLS; d++) {
            int index = i * GW_CAL_COLS + d;
            lv_obj_t *cell = lv_btn_create(grid_row);
            lv_obj_t *label;
            lv_obj_set_flex_grow(cell, 1);
            lv_obj_set_height(cell, GW_UI_TOUCH_MIN);
            gw_ui_style_flat(cell);
            label = gw_ui_label(cell, "", &gw_font_32, gw_ui_c_text());
            lv_obj_align(label, LV_ALIGN_CENTER, 0, -6);
            g_calendar_cells[index] = cell;
            g_calendar_day_labels[index] = label;
            g_calendar_dots[index] = NULL;
            lv_obj_add_event_cb(cell, calendar_day_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    line = make_hairline(content, GW_UI_CONTENT_W);
    (void)line;
    g_calendar_detail_title = gw_ui_label(content, GW_STR_UPCOMING,
                                           &gw_font_28, gw_ui_c_muted());
    lv_obj_set_width(g_calendar_detail_title, LV_PCT(100));
    for (i = 0; i < 3; i++) g_calendar_detail_rows[i] = NULL;
    g_calendar_detail_empty = gw_ui_label(content, GW_STR_NO_ITEMS_DAY,
                                           &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(g_calendar_detail_empty, LV_PCT(100));
    lv_obj_set_style_pad_top(g_calendar_detail_empty, GW_UI_SPACE_1,
                             LV_PART_MAIN);

    calendar_render();
    calendar_detail_render();
}

/* ── To-dos ─────────────────────────────────────────────────────────────── */

static void todo_due_style(void) {
    int i;
    for (i = 0; i < 3; i++) {
        lv_obj_t *button = g_todo_due_buttons[i];
        lv_obj_t *label;
        lv_obj_t *underline;
        if (!button) continue;
        label = lv_obj_get_child(button, 0);
        underline = lv_obj_get_child(button, 1);
        if (!label) continue;
        lv_obj_set_style_text_font(label,
                                   i == g_todo_due_index
                                       ? &gw_font_32_bold : &gw_font_32,
                                   LV_PART_MAIN);
        if (underline) container_set_hidden(underline, i != g_todo_due_index);
    }
}

static void todo_due_cb(lv_event_t *event) {
    g_todo_due_index = (int)(intptr_t)lv_event_get_user_data(event);
    todo_due_style();
}

static void todo_filter_style(void) {
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
    container_set_hidden(lv_obj_get_child(active, 1), g_todo_history);
    container_set_hidden(lv_obj_get_child(history, 1), !g_todo_history);
}

static void todo_add_from_input(void) {
    const char *text;
    char due[11] = "";
    static const int offsets[3] = { 0, 0, 1 };
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

static void todo_add_cb(lv_event_t *event) { (void)event; todo_add_from_input(); }

static void todo_check_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    todo_toggle(id);
    todo_commit();
}

static void todo_restore_action(void *user_data) {
    int id = (int)(intptr_t)user_data;
    todo_restore(id);
    todo_commit();
}

static void todo_delete_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    todo_delete(id);
    todo_commit();
    gw_ui_kit_undo_create(GW_STR_TASK_DELETED, GW_STR_UNDO,
                          todo_restore_action, (void *)(intptr_t)id);
}

static void todo_restore_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    todo_restore(id);
    todo_commit();
}

static void todo_filter_cb(lv_event_t *event) {
    g_todo_history = (int)(intptr_t)lv_event_get_user_data(event) ? 1 : 0;
    todo_filter_style();
    todo_rebuild_list();
}

static void todo_delete_button(lv_obj_t *row, int id) {
    lv_obj_t *button = gw_ui_kit_button_icon_create(row, GW_ICON_TRASH, NULL,
                                                    96, 96,
                                                    GW_UI_BUTTON_SECONDARY);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, todo_delete_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)id);
}

static void todo_rebuild_list(void) {
    int i, shown = 0, active = 0, done = 0, deleted = 0;
    if (!g_todo_list) return;

    lv_obj_clean(g_todo_list);
    for (i = 0; i < g_todo_count; i++) {
        if (g_todos[i].status == GW_TODO_ACTIVE) active++;
        else if (g_todos[i].status == GW_TODO_DONE) done++;
        else if (g_todos[i].status == GW_TODO_DELETED) deleted++;
    }

    if (!g_todo_history) {
        for (i = 0; i < g_todo_count; i++) {
            gw_todo_t *t = &g_todos[i];
            lv_obj_t *row, *text;
            if (t->status != GW_TODO_ACTIVE) continue;
            row = gw_ui_kit_row_create(g_todo_list, 112);
            gw_ui_kit_checkbox_create(row, 0, todo_check_cb,
                                      (void *)(intptr_t)t->id);
            text = gw_ui_label(row, t->text, &gw_font_32, gw_ui_c_text());
            lv_obj_set_flex_grow(text, 1);
            lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
            if (t->due[0]) {
                char due[32];
                format_due_label(t->due, due, sizeof(due));
                {
                    lv_obj_t *chip = gw_ui_label(row, due, &gw_font_28,
                                                 gw_ui_c_secondary());
                    lv_obj_set_width(chip, 150);
                }
            }
            todo_delete_button(row, t->id);
            shown++;
        }
    } else {
        for (i = g_todo_count - 1; i >= 0; i--) {
            gw_todo_t *t = &g_todos[i];
            lv_obj_t *row, *status, *text, *restore;
            if (t->status != GW_TODO_DONE && t->status != GW_TODO_DELETED)
                continue;
            row = gw_ui_kit_row_create(g_todo_list, 112);
            status = gw_ui_label(row, t->status == GW_TODO_DONE
                                          ? GW_STR_DONE : GW_STR_DELETED,
                                 &gw_font_28, gw_ui_c_muted());
            lv_obj_set_width(status, 132);
            text = gw_ui_label(row, t->text, &gw_font_32, gw_ui_c_text());
            lv_obj_set_flex_grow(text, 1);
            lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
            restore = make_button(row, GW_STR_RESTORE, 150, GW_UI_TOUCH_MIN, 1);
            lv_obj_add_event_cb(restore, todo_restore_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)t->id);
            shown++;
        }
    }

    if (shown == 0) {
        lv_obj_t *empty = gw_ui_label(
            g_todo_list,
            g_todo_history ? GW_STR_NO_HISTORY_TASKS : GW_STR_NO_ACTIVE_TASKS,
            &gw_font_32, gw_ui_c_muted());
        lv_obj_set_width(empty, LV_PCT(100));
        lv_obj_set_style_pad_all(empty, GW_UI_GAP, LV_PART_MAIN);
    }

    {
        char summary[128];
        snprintf(summary, sizeof(summary), GW_STR_TODO_SUMMARY_FMT, active, done,
                 deleted);
        label_set_if_changed(g_notes_status, summary);
    }
}

static void notes_keyboard_event(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_CANCEL) {
        g_text_input_active = 0;
        gw_eink_set_text_input(0);
        if (g_notes_keyboard) lv_obj_add_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
        if (g_todo_list)
            lv_obj_set_style_pad_bottom(g_todo_list, 0, LV_PART_MAIN);
        g_settle_refresh_pending = 1;
        gw_eink_hold_partial(500);
    } else if (code == LV_EVENT_READY) {
        todo_add_from_input();
    }
}

static void notes_focus_event(lv_event_t *event) {
    if (lv_event_get_code(event) == LV_EVENT_FOCUSED && g_notes_keyboard) {
        g_text_input_active = 1;
        gw_eink_set_text_input(1);
        lv_obj_clear_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_notes_keyboard);
        lv_keyboard_set_textarea(g_notes_keyboard, g_notes_area);
        if (g_todo_list)
            lv_obj_set_style_pad_bottom(g_todo_list, 480, LV_PART_MAIN);
    } else if (lv_event_get_code(event) == LV_EVENT_DEFOCUSED && g_notes_keyboard) {
        g_text_input_active = 0;
        gw_eink_set_text_input(0);
        lv_obj_add_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
        if (g_todo_list)
            lv_obj_set_style_pad_bottom(g_todo_list, 0, LV_PART_MAIN);
        g_settle_refresh_pending = 1;
        gw_eink_hold_partial(500);
    }
}

static void build_notes_screen(void) {
    lv_obj_t *content, *input_row, *due_row, *filter_row;
    int i;
    static const char *const due_names[3] = {
        GW_STR_NO_DATE, GW_STR_TODAY, GW_STR_TOMORROW
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
    lv_obj_set_style_pad_column(due_row, GW_UI_SPACE_2, LV_PART_MAIN);
    {
        lv_obj_t *label = gw_ui_label(due_row, GW_STR_DUE, &gw_font_32,
                                      gw_ui_c_secondary());
        lv_obj_set_width(label, 80);
    }
    g_todo_due_index = 0;
    for (i = 0; i < 3; i++) {
        lv_obj_t *button = make_button(due_row, due_names[i], 200,
                                       GW_UI_TOUCH_MIN, 1);
        lv_obj_t *underline = lv_obj_create(button);
        lv_obj_set_size(underline, 100, 4);
        lv_obj_set_style_bg_color(underline, gw_ui_c_text(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(underline, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(underline, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(underline, 0, LV_PART_MAIN);
        lv_obj_clear_flag(underline, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(underline, LV_ALIGN_BOTTOM_MID, 0, -4);
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
    {
        int k;
        for (k = 0; k < 2; k++) {
            lv_obj_t *button = k == 0 ? g_todo_filter_active
                                      : g_todo_filter_history;
            lv_obj_t *underline = lv_obj_create(button);
            lv_obj_set_size(underline, 100, 4);
            lv_obj_set_style_bg_color(underline, gw_ui_c_text(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(underline, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(underline, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(underline, 0, LV_PART_MAIN);
            lv_obj_clear_flag(underline, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_align(underline, LV_ALIGN_BOTTOM_MID, 0, -4);
        }
    }
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
    lv_obj_set_style_pad_row(g_todo_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_todo_list, LV_DIR_VER);
    lv_obj_add_flag(g_todo_list, LV_OBJ_FLAG_SCROLLABLE);
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

static void network_build_rows(void) {
    int i;
    if (!g_net_list) return;
    lv_obj_clean(g_net_list);
    g_net_empty_label = NULL;
    for (i = 0; i < GW_NET_MAX; i++) g_net_rows[i] = NULL;
    if (g_net_ap_count == 0) {
        g_net_empty_label = gw_ui_label(g_net_list, GW_STR_NO_NETWORKS,
                                        &gw_font_28, gw_ui_c_muted());
        lv_obj_set_width(g_net_empty_label, LV_PCT(100));
        lv_obj_set_style_pad_all(g_net_empty_label, GW_UI_GAP, LV_PART_MAIN);
        return;
    }
    for (i = 0; i < g_net_ap_count; i++) {
        gw_net_ap_t *ap = &g_net_aps[i];
        lv_obj_t *row = lv_btn_create(g_net_list);
        lv_obj_t *ssid, *dbm, *lock;
        char text[32];
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
        snprintf(text, sizeof(text), "%s  %d dBm", GW_ICON_SIGNAL, ap->signal);
        dbm = gw_ui_label(row, text, &gw_font_28, gw_ui_c_secondary());
        lv_obj_set_width(dbm, 190);
        lock = gw_ui_label(row, ap->secure ? GW_ICON_LOCK : "",
                           &gw_font_28, gw_ui_c_secondary());
        lv_obj_set_width(lock, 32);
        if (g_net_state == 1 && !strcmp(ap->ssid, g_net_ssid)) {
            lv_obj_t *check = gw_ui_label(row, LV_SYMBOL_OK, &gw_font_28,
                                          gw_ui_c_text());
            lv_obj_set_width(check, 32);
        }
        lv_obj_add_event_cb(row, network_select_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_user_data(row, (void *)(intptr_t)i);
        g_net_rows[i] = row;
    }
}

static void network_select_cb(lv_event_t *event) {
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    if (index < 0 || index >= g_net_ap_count) return;
    g_net_selected = index;
    label_set_if_changed(g_net_message, GW_STR_SELECT_NETWORK);
    network_update_ui();
}

static void network_update_ui(void) {
    int i;
    if (!g_net_list) return;
    for (i = 0; i < g_net_ap_count && i < GW_NET_MAX; i++) {
        lv_obj_t *row = g_net_rows[i];
        if (!row) continue;
        lv_obj_set_style_border_width(row, i == g_net_selected ? 3 : 0,
                                      LV_PART_MAIN);
        lv_obj_set_style_border_color(row, gw_ui_c_text(), LV_PART_MAIN);
    }
    if (g_net_signal_bars && g_net_state == 1) {
        int level = signal_bars_for(g_net_signal);
        uint32_t n = lv_obj_get_child_cnt(g_net_signal_bars);
        uint32_t k;
        for (k = 0; k < n; k++) {
            lv_obj_t *bar = lv_obj_get_child(g_net_signal_bars, (int32_t)k);
            lv_obj_set_style_bg_color(
                bar, (int)k < level ? gw_ui_c_text() : gw_ui_c_white(),
                LV_PART_MAIN);
        }
    }
    if (g_net_disconnect_button)
        container_set_hidden(g_net_disconnect_button, g_net_state != 1);
    if (g_net_forget_button)
        container_set_hidden(g_net_forget_button, g_net_state != 1);
    if (g_net_connection_label)
        label_set_if_changed(g_net_connection_label,
                             g_net_state == 1 ? GW_STR_CONNECTED
                             : g_net_state == 0 ? GW_STR_NOT_CONNECTED
                                                : GW_STR_UNKNOWN);
    if (g_net_detail_label) {
        char text[160];
        if (g_net_state == 1 && g_net_ssid[0])
            snprintf(text, sizeof(text), "%s | %s", g_net_ssid,
                     g_net_ip[0] ? g_net_ip : GW_STR_CONNECTED);
        else if (g_net_state == 0)
            snprintf(text, sizeof(text), "%s", GW_STR_NOT_CONNECTED);
        else
            snprintf(text, sizeof(text), "%s", GW_STR_UNKNOWN);
        label_set_if_changed(g_net_detail_label, text);
    }
    {
        int selected_secure = g_net_selected >= 0 &&
                              g_net_selected < g_net_ap_count &&
                              g_net_aps[g_net_selected].secure;
        int selected_connected = g_net_selected >= 0 &&
                                 g_net_selected < g_net_ap_count &&
                                 g_net_state == 1 &&
                                 !strcmp(g_net_aps[g_net_selected].ssid,
                                         g_net_ssid);
        container_set_hidden(g_net_password_row,
                             !(selected_secure && !selected_connected));
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
    (void)event;
    if (g_net_selected < 0 || g_net_selected >= g_net_ap_count) {
        label_set_if_changed(g_net_message, GW_STR_SELECT_NETWORK);
        return;
    }
    if (gw_ui_kit_button_is_busy(g_net_connect_button)) return;
    ssid = g_net_aps[g_net_selected].ssid;
    password = g_net_password ? lv_textarea_get_text(g_net_password) : "";
    ssid_len = strlen(ssid);
    password_len = strlen(password);
    if (g_net_aps[g_net_selected].secure &&
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
        g_text_input_active = 0;
        gw_eink_set_text_input(0);
        lv_obj_add_flag(g_net_keyboard, LV_OBJ_FLAG_HIDDEN);
        g_settle_refresh_pending = 1;
        gw_eink_hold_partial(500);
    }
}

static void wifi_keyboard_event(lv_event_t *event) {
    g_text_input_active = 0;
    gw_eink_set_text_input(0);
    if (g_net_keyboard) lv_obj_add_flag(g_net_keyboard, LV_OBJ_FLAG_HIDDEN);
    g_settle_refresh_pending = 1;
    gw_eink_hold_partial(500);
    (void)event;
}

static void build_network_screen(void) {
    lv_obj_t *content, *row, *card, *line;

    g_network_screen = make_screen();
    gw_ui_kit_app_bar_create(g_network_screen, GW_STR_NETWORK, nav_home_cb);
    content = make_content(g_network_screen, GW_UI_APP_BAR_H + GW_UI_GAP);
    lv_obj_set_style_pad_row(content, GW_UI_SPACE_2, LV_PART_MAIN);

    card = lv_obj_create(content);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 300);
    gw_ui_style_card(card);
    lv_obj_set_style_pad_all(card, GW_UI_SPACE_2, LV_PART_MAIN);
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
    g_net_detail_label = gw_ui_label(card, "", &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(g_net_detail_label, LV_PCT(100));
    lv_label_set_long_mode(g_net_detail_label, LV_LABEL_LONG_DOT);
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
    lv_obj_set_flex_flow(g_net_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_net_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_net_list, LV_DIR_VER);
    lv_obj_add_flag(g_net_list, LV_OBJ_FLAG_SCROLLABLE);

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

#if GW_UI_HOST_SIM
    g_net_ap_count = 2;
    snprintf(g_net_aps[0].ssid, sizeof(g_net_aps[0].ssid), "Home Wi-Fi");
    g_net_aps[0].signal = -50;
    g_net_aps[0].secure = 1;
    snprintf(g_net_aps[1].ssid, sizeof(g_net_aps[1].ssid), "Cafe");
    g_net_aps[1].signal = -78;
    g_net_aps[1].secure = 0;
#endif
    network_build_rows();
    net_status_fetch();
    g_net_signal = -50;
    network_update_ui();
}

/* ── Applications and app updates ─────────────────────────────────────── */

static void apps_remove_cb(lv_event_t *event);

static void strip_newline(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

static int apps_fetch(gw_app_entry_t *out, int max) {
    char response[GW_IPC_MAX_LINE];
    int count = 0, i, used = 0;
    if (ipc_cmd("APP COUNT", response, sizeof(response)) == 0)
        sscanf(response, "APPS %d", &count);
    for (i = 0; i < count && used < max; i++) {
        char command[64], id[64] = "", version[32] = "", description[128] = "";
        snprintf(command, sizeof(command), "APP INFO %d", i);
        if (ipc_cmd(command, response, sizeof(response)) != 0) break;
        if (sscanf(response, "APP %63s %31s %127[^\n]", id, version,
                   description) < 2)
            continue;
        if (!id[0]) break;
        snprintf(out[used].id, sizeof(out[used].id), "%s", id);
        snprintf(out[used].version, sizeof(out[used].version), "%s", version);
        snprintf(out[used].description, sizeof(out[used].description), "%s",
                 description);
        used++;
    }
    return used;
}

static void apps_make_row(int index, const gw_app_entry_t *entry) {
    lv_obj_t *row = lv_obj_create(g_apps_list);
    lv_obj_t *button;
    char detail[220];
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 120);
    gw_ui_style_plain_panel(row);
    lv_obj_set_style_border_width(row, GW_UI_HAIRLINE, LV_PART_MAIN);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_SPACE_2, LV_PART_MAIN);
    snprintf(detail, sizeof(detail), "%s\n%s | %s", entry->id, entry->version,
             entry->description);
    g_app_detail_labels[index] = gw_ui_label(row, detail, &gw_font_32,
                                             gw_ui_c_text());
    lv_obj_set_flex_grow(g_app_detail_labels[index], 1);
    lv_label_set_long_mode(g_app_detail_labels[index], LV_LABEL_LONG_DOT);
    button = make_button(row, GW_STR_REMOVE, 170, GW_UI_TOUCH_MIN, 1);
    lv_obj_set_user_data(button, (void *)(intptr_t)index);
    lv_obj_add_event_cb(button, apps_remove_cb, LV_EVENT_CLICKED, NULL);
    g_app_rows[index] = row;
}

static void apps_apply_entries(gw_app_entry_t *entries, int count) {
    int i, rebuild = 0;
    if (!g_apps_list) return;
    if (count != g_app_count) rebuild = 1;
    if (!rebuild) {
        for (i = 0; i < count; i++)
            if (strcmp(entries[i].id, g_app_entries[i].id)) { rebuild = 1; break; }
    }
    if (rebuild) {
        lv_obj_clean(g_apps_list);
        g_apps_empty_label = NULL;
        for (i = 0; i < GW_APP_MAX; i++) {
            g_app_rows[i] = NULL;
            g_app_detail_labels[i] = NULL;
        }
        if (count == 0) {
            g_apps_empty_label = gw_ui_label(g_apps_list, GW_STR_NO_APPS,
                                             &gw_font_28, gw_ui_c_secondary());
            lv_obj_set_width(g_apps_empty_label, LV_PCT(100));
            lv_obj_set_style_pad_all(g_apps_empty_label, GW_UI_GAP,
                                     LV_PART_MAIN);
        }
        for (i = 0; i < count; i++) apps_make_row(i, &entries[i]);
        g_apps_full_rebuilds++;
    } else {
        /* Single-row changes update that row only; the panel is not rebuilt. */
        for (i = 0; i < count; i++) {
            char detail[200];
            if (!strcmp(entries[i].version, g_app_entries[i].version) &&
                !strcmp(entries[i].description, g_app_entries[i].description))
                continue;
            snprintf(detail, sizeof(detail), "%s\n%s | %s", entries[i].id,
                     entries[i].version, entries[i].description);
            if (g_app_detail_labels[i]) {
                label_set_if_changed(g_app_detail_labels[i], detail);
                g_apps_row_updates++;
            }
        }
    }
    memcpy(g_app_entries, entries, sizeof(g_app_entries[0]) * (size_t)count);
    g_app_count = count;
}

static void apps_rebuild_list(void) {
    gw_app_entry_t entries[GW_APP_MAX];
    int count;
    if (!g_apps_list) return;
    count = apps_fetch(entries, GW_APP_MAX);
    apps_apply_entries(entries, count);
}

static void apps_remove_action(void *user_data) {
    int index = (int)(intptr_t)user_data;
    char command[GW_IPC_MAX_LINE], response[GW_IPC_MAX_LINE];
    if (index < 0 || index >= g_app_count || !g_app_entries[index].id[0]) return;
    snprintf(command, sizeof(command), "APP REMOVE %s", g_app_entries[index].id);
    if (ipc_cmd(command, response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2))
        label_set_if_changed(g_apps_message, GW_STR_APP_REMOVED);
    else {
        strip_newline(response);
        snprintf(command, sizeof(command), GW_STR_REMOVE_FAILED_FMT,
                 response[0] ? response : GW_STR_UNAVAILABLE);
        label_set_if_changed(g_apps_message, command);
    }
    apps_refresh();
}

static void apps_remove_cb(lv_event_t *event) {
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    gw_ui_kit_confirm_create(GW_STR_REMOVE_TITLE, GW_STR_REMOVE_MESSAGE,
                             GW_STR_REMOVE, apps_remove_action,
                             (void *)(intptr_t)index);
}

static void apps_install_usb_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE], message[192];
    lv_obj_t *button = lv_event_get_target(event);
    if (gw_ui_kit_button_is_busy(button)) return;
    gw_ui_kit_busy_set(button, 1, "...");
    if (ipc_cmd("APP INSTALL USB", response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2)) {
        label_set_if_changed(g_apps_message, GW_STR_APP_INSTALLED);
    } else {
        strip_newline(response);
        snprintf(message, sizeof(message), GW_STR_INSTALL_FAILED_FMT,
                 response[0] ? response : GW_STR_UNAVAILABLE);
        label_set_if_changed(g_apps_message, message);
    }
    gw_ui_kit_busy_set(button, 0, NULL);
    apps_refresh();
}

static void apps_refresh(void) {
    apps_rebuild_list();
}

static void apps_refresh_cb(lv_event_t *event) {
    (void)event;
    apps_refresh();
    label_set_if_changed(g_apps_message, GW_STR_APP_LIST_REFRESHED);
}

static void update_scan(void) {
    char response[GW_IPC_MAX_LINE], listing[640] = "";
    int count = 0, i;
    if (!g_update_list) return;
    label_set_if_changed(g_update_list, GW_STR_SCANNING_USB);
    label_set_if_changed(g_update_message, GW_STR_NO_UPDATE_APPLIED);
    if (ipc_cmd("APP SCAN", response, sizeof(response)) == 0)
        sscanf(response, "APP USB %d", &count);
    if (count <= 0) {
        label_set_if_changed(g_update_list, GW_STR_NO_PACKAGES);
        return;
    }
    for (i = 0; i < count && i < 6; i++) {
        char command[64];
        size_t used = strlen(listing);
        snprintf(command, sizeof(command), "APP USB %d", i);
        if (ipc_cmd(command, response, sizeof(response)) != 0) break;
        if (!strncmp(response, "APP USBFILE ", 12)) {
            char *name = response + 12;
            strip_newline(name);
            snprintf(listing + used, sizeof(listing) - used, "%s\n", name);
        }
    }
    if (!listing[0]) strcpy(listing, GW_STR_NO_PACKAGES);
    label_set_if_changed(g_update_list, listing);
}

static void update_scan_cb(lv_event_t *event) {
    (void)event;
    update_scan();
}

static void update_install_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE], message[192];
    lv_obj_t *button = lv_event_get_target(event);
    if (gw_ui_kit_button_is_busy(button)) return;
    gw_ui_kit_busy_set(button, 1, "...");
    label_set_if_changed(g_update_message, GW_STR_INSTALL_UPDATE);
    if (ipc_cmd("APP INSTALL USB", response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2)) {
        strip_newline(response);
        snprintf(message, sizeof(message), GW_STR_UPDATE_APPLIED_FMT, response);
        label_set_if_changed(g_update_message, message);
    } else {
        strip_newline(response);
        snprintf(message, sizeof(message), GW_STR_UPDATE_REJECTED_FMT,
                 response[0] ? response : GW_STR_UNAVAILABLE);
        label_set_if_changed(g_update_message, message);
    }
    gw_ui_kit_busy_set(button, 0, NULL);
    update_scan();
}

static void build_apps_screen(void) {
    lv_obj_t *content, *row, *label;

    g_apps_screen = make_screen();
    gw_ui_kit_app_bar_create(g_apps_screen, GW_STR_APPLICATIONS, nav_home_cb);
    content = make_content(g_apps_screen, GW_UI_APP_BAR_H + GW_UI_GAP);

    settings_section(content, GW_STR_INSTALLED);
    g_apps_list = lv_obj_create(content);
    lv_obj_set_width(g_apps_list, LV_PCT(100));
    lv_obj_set_height(g_apps_list, 440);
    gw_ui_style_plain_panel(g_apps_list);
    lv_obj_set_flex_flow(g_apps_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_apps_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_apps_list, LV_DIR_VER);
    lv_obj_add_flag(g_apps_list, LV_OBJ_FLAG_SCROLLABLE);

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    {
        lv_obj_t *install = make_button(row, GW_STR_INSTALL_USB,
                                        (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                        GW_UI_TOUCH_MIN, 0);
        lv_obj_t *refresh = make_button(row, GW_STR_REFRESH,
                                        (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                        GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(install, apps_install_usb_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(refresh, apps_refresh_cb, LV_EVENT_CLICKED, NULL);
    }
    g_apps_message = gw_ui_label(content, GW_STR_APP_UPDATES_SUB, &gw_font_28,
                                 gw_ui_c_secondary());
    lv_obj_set_width(g_apps_message, LV_PCT(100));
    lv_label_set_long_mode(g_apps_message, LV_LABEL_LONG_DOT);
    {
        lv_obj_t *update = make_button(content, GW_STR_APP_UPDATES,
                                       GW_UI_CONTENT_W, GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(update, update_open_cb, LV_EVENT_CLICKED, NULL);
    }

    settings_section(content, GW_STR_BUILT_IN);
    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 120);
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
            lv_obj_t *launch = make_button(row, GW_STR_OPEN, 170,
                                           GW_UI_TOUCH_MIN, 1);
            lv_obj_add_event_cb(launch, reader_launch, LV_EVENT_CLICKED, NULL);
        }
    }
    label = gw_ui_label(content, GW_STR_REMOVE_MESSAGE, &gw_font_28,
                        gw_ui_c_muted());
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    apps_refresh();
}

static void build_update_screen(void) {
    lv_obj_t *content, *row, *label;

    g_update_screen = make_screen();
    gw_ui_kit_app_bar_create(g_update_screen, GW_STR_APP_UPDATES, nav_home_cb);
    content = make_content(g_update_screen, GW_UI_APP_BAR_H + GW_UI_GAP);

    settings_section(content, GW_STR_SYSTEM);
    label = gw_ui_label(content, GW_STR_OS_VERSION, &gw_font_36_bold,
                        gw_ui_c_text());
    lv_obj_set_width(label, LV_PCT(100));
    label = gw_ui_label(content, GW_STR_APP_BUNDLES_SUB, &gw_font_28,
                        gw_ui_c_secondary());
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    settings_section(content, GW_STR_USB_PACKAGES);
    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    {
        lv_obj_t *scan = make_button(row, GW_STR_SCAN_USB,
                                     (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                     GW_UI_TOUCH_MIN, 1);
        lv_obj_t *install = make_button(row, GW_STR_INSTALL_UPDATE,
                                        (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                        GW_UI_TOUCH_MIN, 0);
        lv_obj_add_event_cb(scan, update_scan_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(install, update_install_cb, LV_EVENT_CLICKED, NULL);
    }
    g_update_list = gw_ui_label(content, GW_STR_NO_PACKAGES, &gw_font_32,
                                gw_ui_c_text());
    lv_obj_set_width(g_update_list, LV_PCT(100));
    lv_label_set_long_mode(g_update_list, LV_LABEL_LONG_WRAP);
    g_update_message = gw_ui_label(content, GW_STR_NO_UPDATE_APPLIED,
                                   &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(g_update_message, LV_PCT(100));
    lv_label_set_long_mode(g_update_message, LV_LABEL_LONG_DOT);
}

/* ── Lock screen ────────────────────────────────────────────────────────── */

static void build_lock_screen(void) {
    lv_obj_t *button, *label;
    struct tm t;
    char date[96];
    g_lock_screen = make_screen();
    gw_ui_now(&t);
    snprintf(date, sizeof(date), "%02d:%02d  |  %s, %d %s %d", t.tm_hour,
             t.tm_min, gw_str_weekday[t.tm_wday], t.tm_mday,
             gw_str_month[t.tm_mon], t.tm_year + 1900);
    label = gw_ui_label(g_lock_screen, date, &gw_font_36, gw_ui_c_secondary());
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -200);
    g_lock_label = gw_ui_label(g_lock_screen, GW_STR_LOCKED, &gw_font_44_bold,
                               gw_ui_c_text());
    lv_obj_align(g_lock_label, LV_ALIGN_CENTER, 0, -120);
    {
        lv_obj_t *hint = gw_ui_label(g_lock_screen, GW_STR_TAP_TO_UNLOCK,
                                     &gw_font_32, gw_ui_c_secondary());
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, -40);
    }
    button = make_button(g_lock_screen, GW_STR_UNLOCK, 300, GW_UI_TOUCH_MIN, 0);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 100);
    lv_obj_add_event_cb(button, unlock_screen_cb, LV_EVENT_CLICKED, NULL);
}

/* ── Model application and screen switching ─────────────────────────────── */

static void home_todo_ensure_row(int index) {
    lv_obj_t *row, *box;
    if (g_widget_todo_rows[index] || !g_home_notes_card) return;
    row = lv_obj_create(g_home_notes_card);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 56);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_SPACE_2, LV_PART_MAIN);
    box = gw_ui_label(row, "[ ]", &gw_font_28,
                      gw_ui_c_secondary());
    (void)box;
    g_widget_todo_rows[index] = gw_ui_label(row, "", &gw_font_32,
                                            gw_ui_c_text());
    lv_obj_set_flex_grow(g_widget_todo_rows[index], 1);
    lv_label_set_long_mode(g_widget_todo_rows[index], LV_LABEL_LONG_DOT);
    if (g_widget_todo_more)
        lv_obj_move_to_index(row, lv_obj_get_index(g_widget_todo_more));
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
                snprintf(text, sizeof(text), "%s  |  %s", g_todos[i].text,
                         due);
            } else {
                snprintf(text, sizeof(text), "%s", g_todos[i].text);
            }
            label_set_if_changed(label, text);
            container_set_hidden(lv_obj_get_parent(label), 0);
        }
        shown++;
    }
    for (i = shown; i < 5; i++) home_todo_remove_row(i);
    if (active > shown) {
        snprintf(text, sizeof(text), GW_STR_MORE_FMT, active - shown);
        label_set_if_changed(g_widget_todo_more, text);
        container_set_hidden(g_widget_todo_more, 0);
    } else {
        container_set_hidden(g_widget_todo_more, 1);
    }
    if (active > 0) {
        snprintf(text, sizeof(text), GW_STR_ACTIVE_FMT, active);
        label_set_if_changed(g_widget_todo_count, text);
    } else {
        label_set_if_changed(g_widget_todo_count, GW_STR_NO_TASKS_YET);
    }
    (void)g_model.note_preview;

    {
        char day[8], month[32], weekday[24];
        snprintf(day, sizeof(day), "%d", t.tm_mday);
        snprintf(month, sizeof(month), "%s %d", gw_str_month[t.tm_mon],
                 t.tm_year + 1900);
        snprintf(weekday, sizeof(weekday), "%s", gw_str_weekday[t.tm_wday]);
        label_set_if_changed(g_widget_day_label, day);
        label_set_if_changed(g_widget_month_label, month);
        label_set_if_changed(g_widget_weekday_label, weekday);
    }

    if (g_model.event_title[0]) {
        snprintf(text, sizeof(text), "%s | %s",
                 g_model.event_when[0] ? g_model.event_when : GW_STR_TODAY,
                 g_model.event_title);
    } else {
        snprintf(text, sizeof(text), "%s", GW_STR_NO_UPCOMING_EVENTS);
    }
    label_set_if_changed(g_widget_event_label, text);
    if (g_calendar_screen) calendar_detail_render();
}

static void model_load(void) {
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

static void build_all_screens(void) {
    build_home_screen();
    build_settings_screen();
    build_calendar_screen();
    build_notes_screen();
    build_network_screen();
    build_apps_screen();
    build_update_screen();
    build_lock_screen();
}

static void gw_ui_show(gw_screen_t screen) {
    lv_obj_t *target = NULL;
    switch (screen) {
    case GW_SCREEN_HOME: target = g_home_screen; break;
    case GW_SCREEN_SETTINGS: target = g_settings_screen; break;
    case GW_SCREEN_CALENDAR: target = g_calendar_screen; break;
    case GW_SCREEN_NOTES: target = g_notes_screen; break;
    case GW_SCREEN_NETWORK: target = g_network_screen; break;
    case GW_SCREEN_APPS: target = g_apps_screen; break;
    case GW_SCREEN_UPDATE: target = g_update_screen; break;
    case GW_SCREEN_LOCK: target = g_lock_screen; break;
    default: return;
    }
    if (!target) return;
    /* Quality mode paints the new screen exactly once with GC16: one clean
     * flash instead of DU4 followed by a deferred full-screen cleanup.
     * Balanced/fast retain the fast transition path. */
    if (g_paint_policy != GW_PAINT_QUALITY) {
        gw_eink_hold_partial(700);
        gw_eink_request_fast(700);
        gw_eink_request_transition(700);
    }
    /* Leaving any screen dismisses both keyboards.  Otherwise a hidden
     * keyboard could remain visible when returning to Notes/Network while the
     * fast-text state was already off. */
    if (g_notes_keyboard) lv_obj_add_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (g_net_keyboard) lv_obj_add_flag(g_net_keyboard, LV_OBJ_FLAG_HIDDEN);
    /* Screen transitions are cleaned by the deferred large-DU path; only a
     * dismissed keyboard needs the GC16 settle cleanup here. */
    if (g_text_input_active)
        g_settle_refresh_pending = 1;
    g_text_input_active = 0;
    gw_eink_set_text_input(0);
    g_active_screen = screen;
    lv_scr_load(target);
    /* Paint the transition in this event cycle instead of waiting for the
     * next render loop; DU4 gives a smooth four-level image immediately and
     * a deferred ghost-compensating pass cleans residue later. */
    if (g_paint_policy == GW_PAINT_QUALITY) g_screen_full_paints++;
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
    lv_refr_now(NULL);
    /* Quality: force one full GC16 over the already-correct framebuffer.
     * This is the single clean paint for the transition; no deferred pass. */
    if (g_paint_policy == GW_PAINT_QUALITY)
        gw_epdc_request_full();
    gw_epdc_present();
#endif
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
    }
}

static void gw_ui_build(void) {
    model_load();
    build_all_screens();
    apply_model();
    /* Sync the service lock before the first paint.  A shell restarted during
     * a reader lock must show the lock screen once, not paint Home and then
     * immediately paint over it. */
    lock_sync_now();
    if (lv_scr_act() != g_lock_screen)
        gw_ui_show(GW_SCREEN_HOME);
    gw_ui_kit_set_full_paint_cb(gw_ui_request_full);
    status_update(NULL);
    lv_timer_create(status_update, 30000, NULL);
    lv_timer_create(ui_tick_cb, 1000, NULL);
}

/* ── Crash diagnostics ──────────────────────────────────────────────────── */
static void crash_handler(int sig, siginfo_t *info, void *ctx) {
    (void)ctx;
    if (info)
        dprintf(2, "GW_SHELL: fatal signal %d at %p\n", sig, (void *)info->si_addr);
    else
        dprintf(2, "GW_SHELL: fatal signal %d\n", sig);
#if GW_HAS_BACKTRACE
    {
        void *frames[32];
        int n = backtrace(frames, 32);
        backtrace_symbols_fd(frames, n, 2);
    }
#endif
    _exit(128 + sig);
}

static void install_crash_handler(void) {
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
static int g_diag_fd = -1;

static void gw_diag_init(void) {
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

static void gw_diag_handle(const char *cmd) {
    int x, y;
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
        g_net_selected = -1;
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

static void gw_diag_poll(void) {
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

int main(void) {
    install_crash_handler();
#if GW_UI_LIFECYCLE_SPIKE && GW_UI_EPDC_SPIKE && GW_UI_TOUCH_SPIKE
    dprintf(2, "GW_UI_TRIAL_FLAGS=1\n");
#endif
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
        /* One quality cleanup after any fast interactive burst.  It waits
         * for 600 ms of quiet so a second tap is not interrupted by a flash. */
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
        if (g_eink_du_large && gw_eink_idle_ms() >= 600 &&
            !g_text_input_active && !g_eink_popup_valid) {
            g_eink_du_large = 0;
            g_eink_du_used = 0;
            if (g_paint_policy == GW_PAINT_FAST) {
                lv_refr_now(NULL);
                /* Ghost-compensating full pass: no GC16 white flash. */
                gw_epdc_request_full_waveform(WAVEFORM_MODE_REAGL);
            } else if (g_paint_policy == GW_PAINT_BALANCED &&
                       ++g_balanced_transitions >= 4) {
                g_balanced_transitions = 0;
                lv_refr_now(NULL);
                gw_epdc_request_full();
            }
        }
#endif
#if GW_UI_PERF
        render_start = gw_now_us();
#endif
        uint32_t delay = lv_timer_handler();
#if GW_UI_PERF
        render_end = gw_now_us();
#endif
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
        if (!g_text_input_active) {
            g_text_cleanup_pending = 0;
            g_text_cleanup_force = 0;
        } else if (g_text_cleanup_pending &&
                   (int32_t)(gw_eink_now_ms() - g_text_cleanup_deadline_ms) >= 0) {
            gw_epdc_add_dirty(g_text_cleanup_rect.x1, g_text_cleanup_rect.y1,
                              g_text_cleanup_rect.x2, g_text_cleanup_rect.y2);
            g_text_cleanup_force = 1;
            g_text_cleanup_pending = 0;
        }
        /* Do not run the grayscale cleanup while the keyboard or a popup is
         * focused: a flash in the middle of typing or menu use is worse than
         * a little residue.  It runs on dismissal/screen change. */
        if (GW_UI_EINK_FAST_MODE && g_settle_refresh_pending &&
            !g_text_input_active && !g_eink_popup_valid) {
            g_settle_refresh_pending = 0;
            /* Make sure a just-dismissed keyboard has reached the framebuffer
             * before the full GC16 cleanup uses it. */
            lv_refr_now(NULL);
            gw_epdc_request_full();
        }
        /* Key feedback must be immediate: press and release are separate A2
         * updates for the key, while the text field uses a separate DU region.
         * Both are small; no full-screen coalescing is involved. */
        if (g_dirty_valid) gw_epdc_present();
#endif
#if GW_UI_DIAG
        gw_diag_poll();
#endif
#if GW_UI_PERF
        GW_PERF_LOG("GW_UI: render handler_us=%lld delay=%u dirty=%d eink_state=%d input_to_render_us=%lld\n",
                    render_end - render_start, delay,
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
                g_dirty_valid,
#else
                0,
#endif
                (int)g_eink_state,
                g_last_input_us ? (render_start - g_last_input_us) : -1);
#endif
        if (delay > 1000) delay = 1000;
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
        if (g_text_cleanup_pending) {
            if (delay > 20) delay = 20;
        } else if (g_dirty_valid && delay > 8) {
            delay = 8;
        }
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

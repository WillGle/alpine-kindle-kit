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
#include <lvgl/lvgl.h>
#include "lv_port_epdc.h"
#include "lv_port_input.h"
#include "gw_ui_style.h"
#include "gw_fonts.h"
#include "gw_ui_kit.h"
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

/* App manager / update screen state. */
static lv_obj_t *g_apps_list;
static lv_obj_t *g_apps_message;
static lv_obj_t *g_update_list;
static lv_obj_t *g_update_message;
static char g_app_ids[8][64];
static int g_app_count;

static void apps_refresh(void);
static void update_scan(void);

/* Home status bar and hero. */
static lv_obj_t *g_clock_label;
static lv_obj_t *g_wifi_label;
static lv_obj_t *g_battery_label;
static lv_obj_t *g_date_label;
static lv_obj_t *g_hero_title;
static lv_obj_t *g_home_reading_card;
static lv_obj_t *g_home_calendar_card;
static lv_obj_t *g_home_notes_card;
static lv_obj_t *g_quick_tiles[4];

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
static lv_obj_t *g_widget_note_label;

/* Dedicated screens. */
static lv_obj_t *g_calendar_title_label;
static lv_obj_t *g_calendar_event_label;
static lv_obj_t *g_notes_area;
static lv_obj_t *g_notes_status;
static lv_obj_t *g_notes_keyboard;
static lv_obj_t *g_wifi_status_label;
static lv_obj_t *g_wifi_list;
static lv_obj_t *g_wifi_password;
static lv_obj_t *g_wifi_message;
static lv_obj_t *g_wifi_keyboard;
static int g_wifi_scan_pending;
static int g_text_input_active;
static lv_obj_t *g_light_label;
static lv_obj_t *g_light_slider;
static lv_obj_t *g_light_bar;
static lv_obj_t *g_settings_battery_label;
static int g_light_slider_updating;
static int g_light_last = -1;
static lv_obj_t *g_lock_label;
static lv_obj_t *g_recovery_label;

static void gw_ui_show(gw_screen_t screen);

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
    if (!strncmp(cmd, "NET RESULT ", 11)) {
        static const char *nets[] = { "Home Wi-Fi", "Library Wi-Fi", "Cafe" };
        unsigned index = (unsigned)atoi(cmd + 11);
        if (index >= sizeof(nets) / sizeof(nets[0])) return -1;
        {
            const unsigned char *s = (const unsigned char *)nets[index];
            size_t i, used = (size_t)snprintf(resp, resp_size, "NET SSID ");
            for (i = 0; s[i] && used + 2 < resp_size; i++)
                used += (size_t)snprintf(resp + used, resp_size - used, "%02x", s[i]);
            snprintf(resp + used, resp_size - used, "\n");
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

/* ── Localized date and calendar helpers ────────────────────────────────── */

static const char *const vi_weekday[7] = {
    "Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư", "Thứ Năm", "Thứ Sáu", "Thứ Bảy"
};
static const char *const vi_month[12] = {
    "tháng 1", "tháng 2", "tháng 3", "tháng 4", "tháng 5", "tháng 6",
    "tháng 7", "tháng 8", "tháng 9", "tháng 10", "tháng 11", "tháng 12"
};

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
        snprintf(buf, size, "Date unavailable");
        return;
    }
    snprintf(buf, size, "%s, %d %s %d",
             vi_weekday[t.tm_wday], t.tm_mday,
             vi_month[t.tm_mon], t.tm_year + 1900);
}

static int days_in_month(int year, int month) {
    static const int days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month];
}

/* ── TODO storage ───────────────────────────────────────────────────────── */

static void todo_rebuild_list(void);
static void todo_preview_refresh(void);

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

#if !GW_UI_HOST_SIM
static void todo_store_save(void) {
    char tmp[128];
    FILE *f;
    int i;
    snprintf(tmp, sizeof(tmp), "%s.tmp", GW_TODO_PATH);
    f = fopen(tmp, "w");
    if (!f) return;
    for (i = 0; i < g_todo_count; i++)
        fprintf(f, "%d\t%d\t%ld\t%ld\t%s\n",
                g_todos[i].id, g_todos[i].status,
                g_todos[i].created, g_todos[i].updated,
                g_todos[i].text);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    rename(tmp, GW_TODO_PATH);
}
#else
static void todo_store_save(void) { }
#endif

static void todo_store_load(void) {
    g_todo_count = 0;
    g_todo_next_id = 1;
#if GW_UI_HOST_SIM
    {
        static const struct { int status; const char *text; } mock[] = {
            { GW_TODO_ACTIVE,  "Mua pin CR2032 cho đồng hồ" },
            { GW_TODO_ACTIVE,  "Kiểm tra lịch sạc PW3" },
            { GW_TODO_DONE,    "Sạc Kindle PW3" },
            { GW_TODO_DELETED, "Gọi điện cho Minh" }
        };
        unsigned i;
        for (i = 0; i < sizeof(mock) / sizeof(mock[0]) && g_todo_count < GW_TODO_MAX; i++) {
            gw_todo_t *t = &g_todos[g_todo_count++];
            t->id = g_todo_next_id++;
            t->status = mock[i].status;
            t->created = t->updated = (long)i;
            snprintf(t->text, sizeof(t->text), "%s", mock[i].text);
        }
    }
#else
    {
        FILE *f = fopen(GW_TODO_PATH, "r");
        char line[512];
        if (!f) return;
        while (fgets(line, sizeof(line), f) && g_todo_count < GW_TODO_MAX) {
            int id, status;
            long created, updated;
            char text[GW_TODO_TEXT_MAX];
            if (sscanf(line, "%d\t%d\t%ld\t%ld\t%159[^\n]",
                       &id, &status, &created, &updated, text) != 5)
                continue;
            if (id <= 0 || status < GW_TODO_ACTIVE || status > GW_TODO_DELETED)
                continue;
            g_todos[g_todo_count].id = id;
            g_todos[g_todo_count].status = status;
            g_todos[g_todo_count].created = created;
            g_todos[g_todo_count].updated = updated;
            todo_sanitize(g_todos[g_todo_count].text,
                          sizeof(g_todos[g_todo_count].text), text);
            if (!g_todos[g_todo_count].text[0]) continue;
            if (id >= g_todo_next_id) g_todo_next_id = id + 1;
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
    label_set_if_changed(g_widget_note_label,
                         g_model.note_preview[0] ? g_model.note_preview
                                                 : "No tasks yet.");
}

static gw_todo_t *todo_find(int id) {
    int i;
    for (i = 0; i < g_todo_count; i++)
        if (g_todos[i].id == id) return &g_todos[i];
    return NULL;
}

static int todo_add(const char *text) {
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

static void status_update(lv_timer_t *timer) {
    char status[GW_IPC_MAX_LINE], text[128];
    char power[24] = "unknown", net[24] = "unknown";
    int battery = -1, light = -1;
    char *field;
    (void)timer;
    if (!g_clock_label) return;

    /* Clock and localized date update only when the displayed day/minute
     * changes, so a static Home stays electrically quiet. */
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

    {
        int status_rc;
#if GW_UI_PERF
        long long status_t0 = gw_now_us();
        status_rc = ipc_cmd(GW_CMD_STATUS, status, sizeof(status));
        {
            long long status_t1 = gw_now_us();
            GW_PERF_LOG("GW_UI: status ipc_us=%lld rc=%d\n",
                        status_t1 - status_t0, status_rc);
        }
#else
        status_rc = ipc_cmd(GW_CMD_STATUS, status, sizeof(status));
#endif
        if (status_rc < 0) {
            label_set_if_changed(g_battery_label, "--");
            label_set_if_changed(g_wifi_label, "Offline");
            label_set_if_changed(g_wifi_status_label, "Status unavailable");
            return;
        }
    }

    field = strstr(status, " battery=");
    if (field) battery = atoi(field + strlen(" battery="));
    field = strstr(status, " light=");
    if (field) light = atoi(field + strlen(" light="));
    field = strstr(status, " power=");
    if (field) sscanf(field, " power=%23s", power);
    field = strstr(status, " net=");
    if (field) sscanf(field, " net=%23s", net);

    /* Fallback for a shell that (re)starts after the service already locked:
     * the event stream normally handles this immediately, but STATUS must not
     * leave the user on Home with the light off. */
    lock_apply_status(status);

    if (battery >= 0 && battery <= 100)
        snprintf(text, sizeof(text), "%d%%", battery);
    else if (!strcmp(power, "charging"))
        snprintf(text, sizeof(text), "Charging");
    else
        snprintf(text, sizeof(text), "--");
    label_set_if_changed(g_battery_label, text);
    label_set_if_changed(g_settings_battery_label, text);

    if (!strcmp(net, "connected")) {
        label_set_if_changed(g_wifi_label, "Wi-Fi");
        label_set_if_changed(g_wifi_status_label, "Connected");
    } else if (!strcmp(net, "disconnected")) {
        label_set_if_changed(g_wifi_label, "Offline");
        label_set_if_changed(g_wifi_status_label, "Not connected");
    } else {
        label_set_if_changed(g_wifi_label, "Offline");
        label_set_if_changed(g_wifi_status_label, "Unknown");
    }

    if (light >= 0 && light <= 100 && light != g_light_last) {
        char light_text[16];
        snprintf(light_text, sizeof(light_text), "%d%%", light);
        label_set_if_changed(g_light_label, light_text);
        g_light_last = light;
        if (g_light_slider && !lv_obj_has_state(g_light_slider, LV_STATE_PRESSED)) {
            g_light_slider_updating = 1;
            lv_slider_set_value(g_light_slider, light, LV_ANIM_OFF);
            if (g_light_bar) lv_bar_set_value(g_light_bar, light, LV_ANIM_OFF);
            g_light_slider_updating = 0;
        }
    }

    /* Wi-Fi scan results are hex-encoded SSIDs, decoded into a bounded
     * dropdown.  No arbitrary shell or raw network data reaches LVGL. */
    if (g_wifi_scan_pending && g_wifi_list) {
        char options[2048] = "", response[GW_IPC_MAX_LINE], cmd[32];
        size_t used = 0;
        unsigned i;
        int error = 0;
        field = strstr(status, " wifi-error=");
        if (field) error = atoi(field + strlen(" wifi-error="));
        g_wifi_scan_pending = 0;
        if (!error) for (i = 0; i < 50; i++) {
            size_t j, len;
            snprintf(cmd, sizeof(cmd), "NET RESULT %u", i);
            if (ipc_cmd(cmd, response, sizeof(response)) || strncmp(response, "NET SSID ", 9)) break;
            len = strcspn(response + 9, "\r\n");
            if (!len || (len & 1) || used + len / 2 + 2 >= sizeof(options)) break;
            if (used) options[used++] = '\n';
            for (j = 0; j < len; j += 2) {
                unsigned value;
                if (sscanf(response + 9 + j, "%2x", &value) != 1) break;
                options[used++] = (char)value;
            }
            options[used] = '\0';
        }
        if (strcmp(lv_dropdown_get_options(g_wifi_list),
                   used ? options : "No networks found") != 0)
            lv_dropdown_set_options(g_wifi_list,
                                    used ? options : "No networks found");
        label_set_if_changed(g_wifi_message,
                             error ? "Scan failed" : used ? "Select a network" : "No networks found");
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
        if (g_recovery_label) lv_label_set_text(g_recovery_label, "Display error");
        return;
    }
#endif
    if (ipc_cmd(GW_CMD_APP_LAUNCH " koreader", response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        g_exit_for_reader = 1;
    } else if (g_recovery_label) {
        lv_label_set_text(g_recovery_label, "Reader unavailable");
    }
#else
    if (g_recovery_label) lv_label_set_text(g_recovery_label, "Reader unavailable");
#endif
}

static void lock_screen_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE];
    (void)event;
    if (ipc_cmd(GW_CMD_LOCK, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        if (g_lock_screen) lv_scr_load(g_lock_screen);
    } else if (g_recovery_label) {
        lv_label_set_text(g_recovery_label, "Lock unavailable");
    }
}

static void unlock_screen_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE];
    (void)event;
    if (ipc_cmd(GW_CMD_UNLOCK, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        if (g_lock_label) lv_label_set_text(g_lock_label, "Screen locked");
        gw_ui_show(GW_SCREEN_HOME);
    } else if (g_lock_label) {
        lv_label_set_text(g_lock_label, "Unlock unavailable");
    }
}

static void light_slider_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE], cmd[32];
    int value;
    if (g_light_slider_updating) return;
    value = lv_slider_get_value(lv_event_get_target(event));
    snprintf(cmd, sizeof(cmd), GW_CMD_LIGHT_SET " %d", value);
    ipc_cmd(cmd, response, sizeof(response));
    if (g_light_label) {
        char text[16];
        snprintf(text, sizeof(text), "%d%%", value);
        label_set_if_changed(g_light_label, text);
        g_light_last = value;
    }
    if (g_light_bar) lv_bar_set_value(g_light_bar, value, LV_ANIM_OFF);
}

/* ── Navigation callbacks ───────────────────────────────────────────────── */

static void nav_home_cb(lv_event_t *event) { (void)event; gw_ui_show(GW_SCREEN_HOME); }
static void nav_books_cb(lv_event_t *event) { reader_launch(event); }
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

static lv_obj_t *make_app_bar(lv_obj_t *scr, const char *title,
                              lv_event_cb_t back_cb) {
    return gw_ui_kit_app_bar_create(scr, title, back_cb);
}

static lv_obj_t *make_bottom_nav(lv_obj_t *scr, int active) {
    gw_ui_nav_cbs_t cbs = { nav_home_cb, nav_books_cb, nav_settings_cb };
    gw_ui_kit_bottom_nav_create(scr, active, &cbs);
    return NULL;
}

static lv_obj_t *make_content(lv_obj_t *scr, int top) {
    return gw_ui_kit_content_create(scr, top);
}

/* ── Home: status bar and hero ──────────────────────────────────────────── */

static void build_status_bar(lv_obj_t *scr) {
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_t *line;
    lv_obj_set_size(bar, GW_UI_PANEL_W, GW_UI_STATUS_BAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    gw_ui_style_plain_panel(bar);
    lv_obj_set_style_pad_left(bar, GW_UI_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_right(bar, GW_UI_MARGIN, LV_PART_MAIN);

    g_clock_label = gw_ui_label(bar, "00:00", &gw_font_36_bold, gw_ui_c_text());
    lv_obj_align(g_clock_label, LV_ALIGN_LEFT_MID, 0, 0);

    g_wifi_label = gw_ui_label(bar, "Wi-Fi", &gw_font_24_bold, gw_ui_c_secondary());
    lv_obj_align(g_wifi_label, LV_ALIGN_RIGHT_MID, -132, 0);

    g_battery_label = gw_ui_label(bar, "--", &gw_font_24_bold, gw_ui_c_secondary());
    lv_obj_align(g_battery_label, LV_ALIGN_RIGHT_MID, 0, 0);

    line = make_hairline(scr, GW_UI_PANEL_W);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, GW_UI_STATUS_BAR_H - GW_UI_HAIRLINE);
}

static void build_hero(lv_obj_t *scr) {
    lv_obj_t *title = gw_ui_label(scr, "Today.", &gw_font_72_bold, gw_ui_c_text());
    g_hero_title = title;
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, GW_UI_MARGIN,
                 GW_UI_STATUS_BAR_H + 20);
    g_date_label = gw_ui_label(scr, "Date unavailable", &gw_font_36,
                               gw_ui_c_secondary());
    lv_obj_align(g_date_label, LV_ALIGN_TOP_LEFT, GW_UI_MARGIN,
                 GW_UI_STATUS_BAR_H + 120);
}

/* ── Home: Continue Reading ─────────────────────────────────────────────── */

static void build_continue_card(lv_obj_t *scr, int y, int height) {
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_t *eyebrow;
    lv_obj_set_size(card, GW_UI_CONTENT_W, height);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, GW_UI_MARGIN, y);
    g_home_reading_card = card;
    gw_ui_style_card(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 10, LV_PART_MAIN);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, reader_launch, LV_EVENT_CLICKED, NULL);

    eyebrow = gw_ui_label(card, "CONTINUE READING", &gw_font_24_bold, gw_ui_c_muted());

    g_reading_body = lv_obj_create(card);
    lv_obj_set_width(g_reading_body, LV_PCT(100));
    lv_obj_set_height(g_reading_body, LV_SIZE_CONTENT);
    gw_ui_style_plain_panel(g_reading_body);
    lv_obj_set_flex_flow(g_reading_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_reading_body, 8, LV_PART_MAIN);

    g_book_title_label = gw_ui_label(g_reading_body, "No book open",
                                     &gw_font_44_bold, gw_ui_c_text());
    lv_obj_set_width(g_book_title_label, LV_PCT(100));
    lv_label_set_long_mode(g_book_title_label, LV_LABEL_LONG_DOT);

    g_book_author_label = gw_ui_label(g_reading_body, "", &gw_font_28,
                                      gw_ui_c_secondary());
    lv_obj_set_width(g_book_author_label, LV_PCT(100));
    lv_label_set_long_mode(g_book_author_label, LV_LABEL_LONG_DOT);

    {
        lv_obj_t *row = lv_obj_create(g_reading_body);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 32);
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
        lv_obj_set_style_bg_color(g_book_bar, lv_color_make(220, 220, 220), LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_book_bar, gw_ui_c_text(), LV_PART_INDICATOR);
        lv_obj_set_style_radius(g_book_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(g_book_bar, 0, LV_PART_INDICATOR);

        g_book_progress_label = gw_ui_label(row, "", &gw_font_24,
                                            gw_ui_c_secondary());
        lv_obj_set_width(g_book_progress_label, 190);
        lv_obj_set_style_text_align(g_book_progress_label, LV_TEXT_ALIGN_RIGHT,
                                    LV_PART_MAIN);
    }

    g_reading_empty = lv_obj_create(card);
    lv_obj_set_width(g_reading_empty, LV_PCT(100));
    lv_obj_set_flex_grow(g_reading_empty, 1);
    gw_ui_style_plain_panel(g_reading_empty);
    lv_obj_set_flex_flow(g_reading_empty, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_reading_empty, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(g_reading_empty, 8, LV_PART_MAIN);
    gw_ui_label(g_reading_empty, "No book open", &gw_font_36_bold, gw_ui_c_text());
    gw_ui_label(g_reading_empty, "Open Books to start reading.", &gw_font_28,
                gw_ui_c_secondary());
    {
        lv_obj_t *browse = make_button(g_reading_empty, "Browse Books", 260,
                                       GW_UI_TOUCH_MIN, 0);
        lv_obj_add_event_cb(browse, reader_launch, LV_EVENT_CLICKED, NULL);
    }
    (void)eyebrow;
}

/* ── Home: Calendar and Notes widgets ───────────────────────────────────── */

static void build_calendar_widget(lv_obj_t *scr, int x, int y, int width, int height) {
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_t *row, *col;
    lv_obj_set_size(card, width, height);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);
    g_home_calendar_card = card;
    gw_ui_style_card(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, LV_PART_MAIN);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, calendar_open_cb, LV_EVENT_CLICKED, NULL);

    gw_ui_label(card, "CALENDAR", &gw_font_24_bold, gw_ui_c_muted());

    row = lv_obj_create(card);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 118);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 20, LV_PART_MAIN);

    g_widget_day_label = gw_ui_label(row, "31", &gw_font_72_bold, gw_ui_c_text());
    col = lv_obj_create(row);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, 100);
    gw_ui_style_plain_panel(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, LV_PART_MAIN);
    g_widget_weekday_label = gw_ui_label(col, "Saturday", &gw_font_28, gw_ui_c_text());
    g_widget_month_label = gw_ui_label(col, "October 2026", &gw_font_24,
                                       gw_ui_c_secondary());

    g_widget_event_label = gw_ui_label(card, "No upcoming events", &gw_font_24,
                                       gw_ui_c_secondary());
    lv_obj_set_width(g_widget_event_label, LV_PCT(100));
    lv_label_set_long_mode(g_widget_event_label, LV_LABEL_LONG_DOT);
}

static void build_notes_widget(lv_obj_t *scr, int x, int y, int width, int height) {
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_size(card, width, height);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);
    g_home_notes_card = card;
    gw_ui_style_card(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 12, LV_PART_MAIN);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, notes_open_cb, LV_EVENT_CLICKED, NULL);

    gw_ui_label(card, "TODOS", &gw_font_24_bold, gw_ui_c_muted());
    g_widget_note_label = gw_ui_label(card, "No tasks yet.", &gw_font_28,
                                      gw_ui_c_text());
    lv_obj_set_width(g_widget_note_label, LV_PCT(100));
    lv_label_set_long_mode(g_widget_note_label, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(g_widget_note_label, 1);

    {
        lv_obj_t *new_task = make_button(card, "New task", 220,
                                         GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(new_task, notes_open_cb, LV_EVENT_CLICKED, NULL);
    }
}

/* ── Home: Quick Access ─────────────────────────────────────────────────── */

static lv_obj_t *make_quick_tile(lv_obj_t *scr, const char *title,
                                 const char *subtitle, int x, int y,
                                 lv_event_cb_t cb) {
    lv_obj_t *tile = lv_btn_create(scr);
    lv_obj_t *t, *s;
    lv_obj_set_size(tile, GW_UI_QUICK_W, 170);
    lv_obj_align(tile, LV_ALIGN_TOP_LEFT, x, y);
    gw_ui_style_button_secondary(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tile, 8, LV_PART_MAIN);
    t = gw_ui_label(tile, title, &gw_font_36_bold, gw_ui_c_text());
    s = gw_ui_label(tile, subtitle, &gw_font_24, gw_ui_c_secondary());
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_event_cb(tile, cb, LV_EVENT_CLICKED, NULL);
    return tile;
}

static void build_quick_access(lv_obj_t *scr, int y) {
    lv_obj_t *label = gw_ui_label(scr, "QUICK ACCESS", &gw_font_24_bold, gw_ui_c_muted());
    int i;
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, GW_UI_MARGIN, y);
    y += 40;
    for (i = 0; i < 4; i++) {
        int x = GW_UI_MARGIN + i * (GW_UI_QUICK_W + GW_UI_GAP);
        switch (i) {
        case 0: g_quick_tiles[i] = make_quick_tile(scr, "Books", "Library", x, y, reader_launch); break;
        case 1: g_quick_tiles[i] = make_quick_tile(scr, "Network", "Wi-Fi & IP", x, y, network_open_cb); break;
        case 2: g_quick_tiles[i] = make_quick_tile(scr, "Light", "Frontlight", x, y, nav_settings_cb); break;
        default: g_quick_tiles[i] = make_quick_tile(scr, "Lock", "Secure", x, y, lock_screen_cb); break;
        }
    }
}

static void build_home_screen(void) {
    g_home_screen = lv_scr_act();
    gw_ui_style_screen(g_home_screen);

    build_status_bar(g_home_screen);
    build_hero(g_home_screen);

    build_continue_card(g_home_screen, GW_UI_STATUS_BAR_H + 202, 320);
    build_calendar_widget(g_home_screen, GW_UI_MARGIN,
                          GW_UI_STATUS_BAR_H + 550, GW_UI_COL_W, 360);
    build_notes_widget(g_home_screen, GW_UI_MARGIN + GW_UI_COL_W + GW_UI_GAP,
                       GW_UI_STATUS_BAR_H + 550, GW_UI_COL_W, 360);
    build_quick_access(g_home_screen, GW_UI_STATUS_BAR_H + 942);
    make_bottom_nav(g_home_screen, 0);

    g_recovery_label = gw_ui_label(g_home_screen, "", &gw_font_24,
                                   gw_ui_c_secondary());
    lv_obj_align(g_recovery_label, LV_ALIGN_BOTTOM_LEFT, GW_UI_MARGIN,
                 -GW_UI_NAV_BAR_H - 40);
}

/* ── Settings ───────────────────────────────────────────────────────────── */

static lv_obj_t *settings_section(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = gw_ui_kit_section_label_create(parent, text);
    lv_obj_set_style_pad_top(label, 12, LV_PART_MAIN);
    return label;
}

static lv_obj_t *settings_row(lv_obj_t *parent, const char *title,
                              const char *subtitle, const char *button_text,
                              lv_event_cb_t cb) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *col;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 120);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    col = lv_obj_create(row);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, 100);
    gw_ui_style_plain_panel(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, LV_PART_MAIN);
    gw_ui_label(col, title, &gw_font_36_bold, gw_ui_c_text());
    if (subtitle && subtitle[0])
        gw_ui_label(col, subtitle, &gw_font_24, gw_ui_c_secondary());

    if (button_text) {
        lv_obj_t *button = make_button(row, button_text, 170, GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    }
    return row;
}

static lv_obj_t *settings_info_row(lv_obj_t *parent, const char *title,
                                   lv_obj_t **value_out) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *label, *value;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 64);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    label = gw_ui_label(row, title, &gw_font_28, gw_ui_c_text());
    lv_obj_set_flex_grow(label, 1);
    value = gw_ui_label(row, "--", &gw_font_28, gw_ui_c_secondary());
    if (value_out) *value_out = value;
    return row;
}

static void build_settings_screen(void) {
    lv_obj_t *content, *row, *line, *label;
    lv_obj_t *battery_value = NULL;

    g_settings_screen = make_screen();
    make_app_bar(g_settings_screen, "Settings", nav_home_cb);
    make_bottom_nav(g_settings_screen, 2);
    content = make_content(g_settings_screen, GW_UI_APP_BAR_H + GW_UI_GAP);

    settings_section(content, "READING");
    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 120);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    {
        lv_obj_t *b1 = make_button(row, "Books", (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                   GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(b1, reader_launch, LV_EVENT_CLICKED, NULL);
        lv_obj_t *b2 = make_button(row, "Continue Reading",
                                   (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                   GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(b2, reader_launch, LV_EVENT_CLICKED, NULL);
    }
    line = make_hairline(content, GW_UI_CONTENT_W);
    settings_section(content, "DISPLAY");
    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 120);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *col = lv_obj_create(row);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_height(col, 100);
        gw_ui_style_plain_panel(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(col, 2, LV_PART_MAIN);
        gw_ui_label(col, "Frontlight", &gw_font_36_bold, gw_ui_c_text());
        gw_ui_label(col, "Brightness", &gw_font_24, gw_ui_c_secondary());

        lv_obj_t *track = lv_obj_create(row);
        lv_obj_set_size(track, 380, GW_UI_TOUCH_MIN);
        gw_ui_style_plain_panel(track);

        g_light_bar = lv_bar_create(track);
        lv_obj_set_size(g_light_bar, 380, 12);
        lv_obj_align(g_light_bar, LV_ALIGN_CENTER, 0, 0);
        lv_bar_set_range(g_light_bar, 0, 100);
        lv_bar_set_value(g_light_bar, 37, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(g_light_bar, lv_color_make(210, 210, 210),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_light_bar, gw_ui_c_text(), LV_PART_INDICATOR);
        lv_obj_set_style_radius(g_light_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(g_light_bar, 0, LV_PART_INDICATOR);

        /* Transparent 96 px slider over a thin visual track: the full-height
         * touch target stays accessible without a heavy E-ink knob. */
        g_light_slider = lv_slider_create(track);
        lv_obj_set_size(g_light_slider, 380, GW_UI_TOUCH_MIN);
        lv_obj_align(g_light_slider, LV_ALIGN_CENTER, 0, 0);
        lv_slider_set_range(g_light_slider, 0, 100);
        lv_slider_set_value(g_light_slider, 37, LV_ANIM_OFF);
        lv_obj_set_style_bg_opa(g_light_slider, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_light_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(g_light_slider, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_border_width(g_light_slider, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_light_slider, 0, LV_PART_INDICATOR);
        lv_obj_set_style_border_width(g_light_slider, 0, LV_PART_KNOB);
        lv_obj_set_style_shadow_width(g_light_slider, 0, LV_PART_KNOB);
        lv_obj_add_event_cb(g_light_slider, light_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

        g_light_label = gw_ui_label(row, "37%", &gw_font_28, gw_ui_c_text());
        lv_obj_set_width(g_light_label, 86);
        lv_obj_set_style_text_align(g_light_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    }
    line = make_hairline(content, GW_UI_CONTENT_W);
    settings_section(content, "CONNECTIVITY");
    settings_row(content, "Wi-Fi", "Networks and connection", "Open",
                 network_open_cb);
    line = make_hairline(content, GW_UI_CONTENT_W);
    settings_section(content, "SOFTWARE");
    settings_row(content, "Applications", "Install, update, remove", "Open",
                 apps_open_cb);
    line = make_hairline(content, GW_UI_CONTENT_W);
    settings_section(content, "DEVICE");
    {
        lv_obj_t *lock_button = make_button(content, "Lock now",
                                            GW_UI_CONTENT_W, GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(lock_button, lock_screen_cb, LV_EVENT_CLICKED, NULL);
    }
    (void)battery_value;
    (void)line;
    (void)label;
}

/* ── Calendar screen ────────────────────────────────────────────────────── */

static void build_calendar_screen(void) {
    lv_obj_t *content, *row, *line;
    struct tm t;
    int year, month, today, first_wday, dim, cell, i;
    gw_ui_now(&t);
    year = t.tm_year + 1900;
    month = t.tm_mon;
    today = t.tm_mday;

    g_calendar_screen = make_screen();
    make_app_bar(g_calendar_screen, "Calendar", nav_home_cb);
    make_bottom_nav(g_calendar_screen, -1);
    content = make_content(g_calendar_screen, GW_UI_APP_BAR_H + GW_UI_GAP);
    lv_obj_set_style_pad_row(content, 10, LV_PART_MAIN);

    {
        char title[64];
        snprintf(title, sizeof(title), "%s %d",
                 vi_month[month], year);
        g_calendar_title_label = gw_ui_label(content, title, &gw_font_44_bold,
                                             gw_ui_c_text());
    }

    /* Weekday header: Vietnamese short names. */
    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 44);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    {
        static const char *const wd[7] = { "T2", "T3", "T4", "T5", "T6", "T7", "CN" };
        for (i = 0; i < 7; i++) {
            lv_obj_t *l = gw_ui_label(row, wd[i], &gw_font_24_bold, gw_ui_c_muted());
            lv_obj_set_flex_grow(l, 1);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        }
    }
    line = make_hairline(content, GW_UI_CONTENT_W);
    (void)line;

    /* Monday-first month grid. */
    first_wday = (t.tm_wday + 6) % 7;
    dim = days_in_month(year, month);
    cell = 1 - first_wday;
    for (i = 0; i < 6; i++) {
        int d;
        row = lv_obj_create(content);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 78);
        gw_ui_style_plain_panel(row);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        for (d = 0; d < 7; d++, cell++) {
            lv_obj_t *slot = lv_obj_create(row);
            lv_obj_t *label;
            char text[16];
            lv_obj_set_flex_grow(slot, 1);
            lv_obj_set_height(slot, 70);
            gw_ui_style_plain_panel(slot);
            if (cell < 1 || cell > dim) continue;
            snprintf(text, sizeof(text), "%d", cell);
            label = gw_ui_label(slot, text, &gw_font_28, gw_ui_c_text());
            lv_obj_center(label);
            if (cell == today) {
                lv_obj_set_style_bg_color(label, gw_ui_c_text(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
                lv_obj_set_style_pad_all(label, 8, LV_PART_MAIN);
                lv_obj_set_style_radius(label, 6, LV_PART_MAIN);
            }
        }
        if (cell > dim) break;
    }

    {
        lv_obj_t *up = gw_ui_label(content, "UPCOMING", &gw_font_24_bold, gw_ui_c_muted());
        lv_obj_set_style_pad_top(up, 12, LV_PART_MAIN);
    }
    g_calendar_event_label = gw_ui_label(content, "No upcoming events",
                                         &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(g_calendar_event_label, LV_PCT(100));
    lv_label_set_long_mode(g_calendar_event_label, LV_LABEL_LONG_DOT);
}

/* ── TODO screen ────────────────────────────────────────────────────────── */

static void todo_add_from_input(void) {
    const char *text;
    int rc;
    if (!g_notes_area) return;
    text = lv_textarea_get_text(g_notes_area);
    rc = todo_add(text);
    if (rc == 0) {
        lv_textarea_set_text(g_notes_area, "");
        label_set_if_changed(g_notes_status, "Task added");
        todo_commit();
    } else if (rc == -2) {
        label_set_if_changed(g_notes_status, "Task limit reached");
    } else {
        label_set_if_changed(g_notes_status, "Type a task first");
    }
}

static void todo_add_cb(lv_event_t *event) {
    (void)event;
    todo_add_from_input();
}

static void todo_check_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    todo_toggle(id);
    todo_commit();
}

static void todo_delete_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    todo_delete(id);
    todo_commit();
}

static void todo_restore_cb(lv_event_t *event) {
    int id = (int)(intptr_t)lv_event_get_user_data(event);
    todo_restore(id);
    todo_commit();
}

static void todo_filter_style(void) {
    lv_obj_t *active_label, *history_label;
    if (g_todo_filter_active) {
        if (g_todo_history) gw_ui_style_button_secondary(g_todo_filter_active);
        else gw_ui_style_button(g_todo_filter_active);
        active_label = lv_obj_get_child(g_todo_filter_active, 0);
        if (active_label)
            lv_obj_set_style_text_color(active_label,
                                        g_todo_history ? gw_ui_c_text()
                                                       : lv_color_white(),
                                        LV_PART_MAIN);
    }
    if (g_todo_filter_history) {
        if (g_todo_history) gw_ui_style_button(g_todo_filter_history);
        else gw_ui_style_button_secondary(g_todo_filter_history);
        history_label = lv_obj_get_child(g_todo_filter_history, 0);
        if (history_label)
            lv_obj_set_style_text_color(history_label,
                                        g_todo_history ? lv_color_white()
                                                       : gw_ui_c_text(),
                                        LV_PART_MAIN);
    }
}

static void todo_filter_cb(lv_event_t *event) {
    g_todo_history = (int)(intptr_t)lv_event_get_user_data(event) ? 1 : 0;
    todo_filter_style();
    todo_rebuild_list();
}

static lv_obj_t *todo_action_button(lv_obj_t *row, const char *text,
                                    int width, lv_event_cb_t cb, int id) {
    lv_obj_t *button = make_button(row, text, width, GW_UI_TOUCH_MIN, 1);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, (void *)(intptr_t)id);
    return button;
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
        int pass;
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; i < g_todo_count; i++) {
                gw_todo_t *t = &g_todos[i];
                lv_obj_t *row, *box, *text;
                lv_color_t colour;
                if (pass == 0 && t->status != GW_TODO_ACTIVE) continue;
                if (pass == 1 && t->status != GW_TODO_DONE) continue;
                row = gw_ui_kit_row_create(g_todo_list, 104);
                box = gw_ui_kit_checkbox_create(row, t->status == GW_TODO_DONE,
                                                todo_check_cb,
                                                (void *)(intptr_t)t->id);
                (void)box;
                colour = t->status == GW_TODO_DONE ? gw_ui_c_muted()
                                                   : gw_ui_c_text();
                text = gw_ui_label(row, t->text, &gw_font_28, colour);
                lv_obj_set_flex_grow(text, 1);
                lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
                todo_action_button(row, "Delete", 120, todo_delete_cb, t->id);
                shown++;
            }
        }
    } else {
        for (i = g_todo_count - 1; i >= 0; i--) {
            gw_todo_t *t = &g_todos[i];
            lv_obj_t *row, *status, *text;
            if (t->status != GW_TODO_DONE && t->status != GW_TODO_DELETED)
                continue;
            row = gw_ui_kit_row_create(g_todo_list, 104);
            status = gw_ui_label(row,
                                 t->status == GW_TODO_DONE ? "DONE" : "DELETED",
                                 &gw_font_24_bold, gw_ui_c_muted());
            lv_obj_set_width(status, 132);
            text = gw_ui_label(row, t->text, &gw_font_28, gw_ui_c_text());
            lv_obj_set_flex_grow(text, 1);
            lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
            todo_action_button(row, "Restore", 150, todo_restore_cb, t->id);
            shown++;
        }
    }

    if (shown == 0) {
        lv_obj_t *empty = gw_ui_label(
            g_todo_list,
            g_todo_history ? "No completed or deleted tasks."
                           : "No tasks yet.",
            &gw_font_28, gw_ui_c_muted());
        lv_obj_set_style_pad_all(empty, 24, LV_PART_MAIN);
    }

    {
        char summary[96];
        snprintf(summary, sizeof(summary), "%d active · %d done · %d deleted",
                 active, done, deleted);
        label_set_if_changed(g_notes_status, summary);
    }
}

static void notes_keyboard_event(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_CANCEL) {
        g_text_input_active = 0;
        gw_eink_set_text_input(0);
        if (g_notes_keyboard) lv_obj_add_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
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
    } else if (lv_event_get_code(event) == LV_EVENT_DEFOCUSED && g_notes_keyboard) {
        g_text_input_active = 0;
        gw_eink_set_text_input(0);
        lv_obj_add_flag(g_notes_keyboard, LV_OBJ_FLAG_HIDDEN);
        g_settle_refresh_pending = 1;
        gw_eink_hold_partial(500);
    }
}

static void build_notes_screen(void) {
    lv_obj_t *content, *input_row, *filter_row;

    g_notes_screen = make_screen();
    make_app_bar(g_notes_screen, "To-dos", nav_home_cb);
    make_bottom_nav(g_notes_screen, -1);
    content = make_content(g_notes_screen, GW_UI_APP_BAR_H + GW_UI_GAP);
    lv_obj_set_style_pad_row(content, 16, LV_PART_MAIN);

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
    lv_textarea_set_placeholder_text(g_notes_area, "New task...");
    lv_obj_set_flex_grow(g_notes_area, 1);
    lv_obj_set_height(g_notes_area, GW_UI_TOUCH_MIN);
    gw_ui_style_textarea(g_notes_area);
    lv_obj_set_style_text_font(g_notes_area, &gw_font_28, LV_PART_MAIN);
    lv_obj_set_style_anim_time(g_notes_area, 0, LV_PART_CURSOR);
    lv_obj_add_event_cb(g_notes_area, notes_focus_event, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(g_notes_area, notes_focus_event, LV_EVENT_DEFOCUSED, NULL);

    {
        lv_obj_t *add = make_button(input_row, "Add", 160,
                                    GW_UI_TOUCH_MIN, 0);
        lv_obj_add_event_cb(add, todo_add_cb, LV_EVENT_CLICKED, NULL);
    }

    filter_row = lv_obj_create(content);
    lv_obj_set_width(filter_row, LV_PCT(100));
    lv_obj_set_height(filter_row, 112);
    gw_ui_style_plain_panel(filter_row);
    lv_obj_set_flex_flow(filter_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(filter_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(filter_row, GW_UI_GAP, LV_PART_MAIN);
    g_todo_filter_active = make_button(filter_row, "Active", 180,
                                       GW_UI_TOUCH_MIN, 0);
    g_todo_filter_history = make_button(filter_row, "History", 180,
                                        GW_UI_TOUCH_MIN, 1);
    lv_obj_add_event_cb(g_todo_filter_active, todo_filter_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)0);
    lv_obj_add_event_cb(g_todo_filter_history, todo_filter_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)1);
    g_notes_status = gw_ui_label(filter_row, "", &gw_font_24,
                                 gw_ui_c_secondary());
    lv_obj_set_flex_grow(g_notes_status, 1);

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
    todo_filter_style();
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

/* ── Network screen ─────────────────────────────────────────────────────── */

static void wifi_scan(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE];
    (void)event;
    if (ipc_cmd("NET SCAN", response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        g_wifi_scan_pending = 1;
        label_set_if_changed(g_wifi_message, "Scanning...");
    } else {
        label_set_if_changed(g_wifi_message, "Scan unavailable");
    }
}

static void wifi_connect(lv_event_t *event) {
    static const char hex[] = "0123456789abcdef";
    char selected[65];
    const unsigned char *ssid = (const unsigned char *)selected;
    const unsigned char *password =
        (const unsigned char *)lv_textarea_get_text(g_wifi_password);
    char command[GW_IPC_MAX_LINE], response[GW_IPC_MAX_LINE];
    size_t ssid_len, password_len, i, n;
    (void)event;
    if (!g_wifi_list || !g_wifi_password) return;
    lv_dropdown_get_selected_str(g_wifi_list, selected, sizeof(selected));
    ssid_len = strlen(selected);
    password_len = strlen((const char *)password);
    if (!strcmp(selected, "No networks found")) ssid_len = 0;
    if (ssid_len < 1 || ssid_len > 32 || password_len < 8 || password_len > 63) {
        label_set_if_changed(g_wifi_message,
                             "SSID 1-32 bytes; password 8-63 characters");
        return;
    }
    n = (size_t)snprintf(command, sizeof(command), GW_CMD_NET_CONNECT " ");
    if (n >= sizeof(command)) return;
    for (i = 0; i < ssid_len; i++) {
        command[n++] = hex[ssid[i] >> 4];
        command[n++] = hex[ssid[i] & 15];
    }
    command[n++] = ' ';
    if (n + password_len * 2 + 1 >= sizeof(command)) return;
    for (i = 0; i < password_len; i++) {
        command[n++] = hex[password[i] >> 4];
        command[n++] = hex[password[i] & 15];
    }
    command[n] = '\0';
    if (ipc_cmd(command, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n")) {
        label_set_if_changed(g_wifi_message, "Connecting...");
        lv_textarea_set_text(g_wifi_password, "");
    } else {
        label_set_if_changed(g_wifi_message, "Connection request failed");
    }
    memset(command, 0, sizeof(command));
}

static void wifi_disconnect(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE];
    (void)event;
    if (ipc_cmd(GW_CMD_NET_DISCO, response, sizeof(response)) == 0 &&
        !strcmp(response, "OK\n"))
        label_set_if_changed(g_wifi_message, "Disconnecting...");
    else
        label_set_if_changed(g_wifi_message, "Disconnect failed");
}

static void wifi_focus(lv_event_t *event) {
    if (lv_event_get_code(event) == LV_EVENT_FOCUSED && g_wifi_keyboard) {
        g_text_input_active = 1;
        gw_eink_set_text_input(1);
        lv_obj_clear_flag(g_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_wifi_keyboard);
        lv_keyboard_set_textarea(g_wifi_keyboard, g_wifi_password);
    } else if (lv_event_get_code(event) == LV_EVENT_DEFOCUSED && g_wifi_keyboard) {
        g_text_input_active = 0;
        gw_eink_set_text_input(0);
        lv_obj_add_flag(g_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
        g_settle_refresh_pending = 1;
        gw_eink_hold_partial(500);
    }
}

static void wifi_keyboard_event(lv_event_t *event) {
    g_text_input_active = 0;
    gw_eink_set_text_input(0);
    if (g_wifi_keyboard) lv_obj_add_flag(g_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    g_settle_refresh_pending = 1;
    gw_eink_hold_partial(500);
    (void)event;
}

static void wifi_dropdown_click(lv_event_t *event) {
    (void)event;
    /* A popup and the keyboard do not share the screen well: dismiss the
     * keyboard first so the list gets one clean, fast partial redraw. */
    if (g_wifi_keyboard) lv_obj_add_flag(g_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (g_text_input_active) {
        g_text_input_active = 0;
        gw_eink_set_text_input(0);
    }
}

static void build_network_screen(void) {
    lv_obj_t *content, *row, *line, *label;

    g_network_screen = make_screen();
    make_app_bar(g_network_screen, "Network", nav_home_cb);
    make_bottom_nav(g_network_screen, -1);
    content = make_content(g_network_screen, GW_UI_APP_BAR_H + GW_UI_GAP);

    label = gw_ui_label(content, "CONNECTION", &gw_font_24_bold, gw_ui_c_muted());
    (void)label;
    g_wifi_status_label = gw_ui_label(content, "Unknown", &gw_font_36_bold,
                                      gw_ui_c_text());

    g_wifi_list = lv_dropdown_create(content);
    lv_obj_set_width(g_wifi_list, LV_PCT(100));
    lv_obj_set_height(g_wifi_list, GW_UI_TOUCH_MIN);
    lv_obj_set_style_text_font(g_wifi_list, &gw_font_28, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_wifi_list, &lv_font_montserrat_28,
                               LV_PART_INDICATOR);
    lv_obj_set_style_text_color(g_wifi_list, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_wifi_list, GW_UI_HAIRLINE, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_wifi_list, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_radius(g_wifi_list, 4, LV_PART_MAIN);
    lv_dropdown_set_options(g_wifi_list,
#if GW_UI_HOST_SIM
                            "Home Wi-Fi\nLibrary Wi-Fi\nCafe"
#else
                            "No networks found"
#endif
                            );
    lv_obj_add_event_cb(g_wifi_list, wifi_dropdown_click, LV_EVENT_CLICKED, NULL);
    /* Create the list once so its popup style is applied before the first
     * user-visible open. */
    lv_dropdown_open(g_wifi_list);
    {
        lv_obj_t *list = lv_dropdown_get_list(g_wifi_list);
        if (list) gw_ui_kit_style_dropdown_list(list);
    }
    lv_dropdown_close(g_wifi_list);

    g_wifi_password = lv_textarea_create(content);
    lv_textarea_set_one_line(g_wifi_password, true);
    lv_textarea_set_password_mode(g_wifi_password, true);
    /* Hide characters immediately.  LVGL's default 1500 ms reveal schedules
     * an extra whole-textarea repaint after every key press. */
    lv_textarea_set_password_show_time(g_wifi_password, 0);
    lv_textarea_set_max_length(g_wifi_password, 63);
    lv_textarea_set_placeholder_text(g_wifi_password, "Password (8-63 characters)");
    lv_obj_set_width(g_wifi_password, LV_PCT(100));
    lv_obj_set_height(g_wifi_password, 112);
    gw_ui_style_textarea(g_wifi_password);
    lv_obj_set_style_text_font(g_wifi_password, &gw_font_28, LV_PART_MAIN);
    lv_obj_set_style_anim_time(g_wifi_password, 0, LV_PART_CURSOR);
    /* The password field does not need a visible caret bar; the key
     * highlight is the feedback. */
    lv_obj_set_style_bg_opa(g_wifi_password, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_border_width(g_wifi_password, 0, LV_PART_CURSOR);
    lv_obj_add_event_cb(g_wifi_password, wifi_focus, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(g_wifi_password, wifi_focus, LV_EVENT_DEFOCUSED, NULL);

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 112);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    {
        lv_obj_t *scan = make_button(row, "Scan", 200, GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(scan, wifi_scan, LV_EVENT_CLICKED, NULL);
        lv_obj_t *connect = make_button(row, "Connect", 240, GW_UI_TOUCH_MIN, 0);
        lv_obj_add_event_cb(connect, wifi_connect, LV_EVENT_CLICKED, NULL);
        lv_obj_t *disconnect = make_button(row, "Disconnect", 240,
                                           GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(disconnect, wifi_disconnect, LV_EVENT_CLICKED, NULL);
    }
    line = make_hairline(content, GW_UI_CONTENT_W);
    (void)line;

    g_wifi_message = gw_ui_label(content, "Select a network and enter its password",
                                 &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(g_wifi_message, LV_PCT(100));
    lv_label_set_long_mode(g_wifi_message, LV_LABEL_LONG_DOT);

    g_wifi_keyboard = lv_keyboard_create(g_network_screen);
    lv_obj_set_size(g_wifi_keyboard, GW_UI_CONTENT_W, 480);
    lv_obj_align(g_wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, -GW_UI_NAV_BAR_H);
    gw_ui_kit_style_keyboard(g_wifi_keyboard);
    gw_eink_set_keyboard_rect(1, GW_UI_MARGIN,
                              GW_UI_PANEL_H - GW_UI_NAV_BAR_H - 480,
                              GW_UI_MARGIN + GW_UI_CONTENT_W - 1,
                              GW_UI_PANEL_H - GW_UI_NAV_BAR_H - 1);
    lv_obj_add_event_cb(g_wifi_keyboard, wifi_keyboard_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_wifi_keyboard, wifi_keyboard_event, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(g_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);

    status_update(NULL);
}

/* ── Applications and app updates ─────────────────────────────────────── */

static void strip_newline(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

static void apps_remove_cb(lv_event_t *event) {
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    char command[GW_IPC_MAX_LINE], response[GW_IPC_MAX_LINE];
    if (index < 0 || index >= g_app_count || !g_app_ids[index][0]) return;
    snprintf(command, sizeof(command), "APP REMOVE %s", g_app_ids[index]);
    if (ipc_cmd(command, response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2))
        label_set_if_changed(g_apps_message, "Application removed. Data was kept.");
    else {
        strip_newline(response);
        snprintf(command, sizeof(command), "Remove failed: %s", response);
        label_set_if_changed(g_apps_message, command);
    }
    apps_refresh();
}

static void apps_install_usb_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE], message[192];
    (void)event;
    if (ipc_cmd("APP INSTALL USB", response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2)) {
        label_set_if_changed(g_apps_message, "Installed and verified from USB.");
    } else {
        strip_newline(response);
        snprintf(message, sizeof(message), "Install failed: %s",
                 response[0] ? response : "unavailable");
        label_set_if_changed(g_apps_message, message);
    }
    apps_refresh();
}

static void apps_rebuild_list(void) {
    char response[GW_IPC_MAX_LINE];
    int count = 0, shown, i;
    if (!g_apps_list) return;
    lv_obj_clean(g_apps_list);
    g_app_count = 0;
    if (ipc_cmd("APP COUNT", response, sizeof(response)) == 0)
        sscanf(response, "APPS %d", &count);
    if (count <= 0) {
        lv_obj_t *empty = gw_ui_label(g_apps_list, "No user applications installed",
                                     &gw_font_28, gw_ui_c_secondary());
        lv_obj_set_style_pad_all(empty, 12, LV_PART_MAIN);
        return;
    }
    shown = count > 8 ? 8 : count;
    for (i = 0; i < shown; i++) {
        char command[64], id[64] = "", version[32] = "", description[128] = "";
        lv_obj_t *row, *col, *button;
        snprintf(command, sizeof(command), "APP INFO %d", i);
        if (ipc_cmd(command, response, sizeof(response)) != 0) break;
        if (sscanf(response, "APP %63s %31s %127[^\n]", id, version, description) < 2)
            continue;
        if (!id[0]) continue;
        snprintf(g_app_ids[g_app_count], sizeof(g_app_ids[0]), "%s", id);
        row = lv_obj_create(g_apps_list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 112);
        gw_ui_style_plain_panel(row);
        lv_obj_set_style_border_width(row, GW_UI_HAIRLINE, LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, gw_ui_c_border(), LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 12, LV_PART_MAIN);

        col = lv_obj_create(row);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_height(col, 100);
        gw_ui_style_plain_panel(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        gw_ui_label(col, id, &gw_font_36_bold, gw_ui_c_text());
        {
            char detail[192];
            snprintf(detail, sizeof(detail), "%s%s%s", version,
                     description[0] ? " · " : "", description);
            lv_label_set_long_mode(gw_ui_label(col, detail, &gw_font_24,
                                              gw_ui_c_secondary()),
                                   LV_LABEL_LONG_DOT);
        }
        button = make_button(row, "Remove", 170, GW_UI_TOUCH_MIN, 1);
        lv_obj_set_user_data(button, (void *)(intptr_t)g_app_count);
        lv_obj_add_event_cb(button, apps_remove_cb, LV_EVENT_CLICKED, NULL);
        g_app_count++;
    }
}

static void apps_refresh(void) {
    apps_rebuild_list();
}

static void apps_refresh_cb(lv_event_t *event) {
    (void)event;
    apps_refresh();
    label_set_if_changed(g_apps_message, "Application list refreshed.");
}

static void update_scan(void) {
    char response[GW_IPC_MAX_LINE], listing[640] = "";
    int count = 0, i;
    if (!g_update_list) return;
    label_set_if_changed(g_update_list, "Scanning USB...");
    label_set_if_changed(g_update_message, "Reading /mnt/us/gleworks/apps");
    if (ipc_cmd("APP SCAN", response, sizeof(response)) == 0)
        sscanf(response, "APP USB %d", &count);
    if (count <= 0) {
        label_set_if_changed(g_update_list, "No signed packages found on USB.");
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
    if (!listing[0]) strcpy(listing, "Packages found.");
    label_set_if_changed(g_update_list, listing);
}

static void update_scan_cb(lv_event_t *event) {
    (void)event;
    update_scan();
}

static void update_install_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE], message[192];
    (void)event;
    label_set_if_changed(g_update_message, "Verifying signature and installing...");
    if (ipc_cmd("APP INSTALL USB", response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2)) {
        snprintf(message, sizeof(message), "Update applied: %s", response);
        strip_newline(message);
        label_set_if_changed(g_update_message, message);
    } else {
        strip_newline(response);
        snprintf(message, sizeof(message), "Update rejected: %s",
                 response[0] ? response : "unavailable");
        label_set_if_changed(g_update_message, message);
    }
    update_scan();
}

static void build_apps_screen(void) {
    lv_obj_t *content, *row, *line, *label;

    g_apps_screen = make_screen();
    make_app_bar(g_apps_screen, "Applications", nav_home_cb);
    make_bottom_nav(g_apps_screen, 2);
    content = make_content(g_apps_screen, GW_UI_APP_BAR_H + GW_UI_GAP);

    settings_section(content, "INSTALLED");
    g_apps_list = lv_obj_create(content);
    lv_obj_set_width(g_apps_list, LV_PCT(100));
    lv_obj_set_height(g_apps_list, 460);
    gw_ui_style_plain_panel(g_apps_list);
    lv_obj_set_flex_flow(g_apps_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_apps_list, 0, LV_PART_MAIN);

    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    {
        lv_obj_t *install = make_button(row, "Install from USB",
                                        (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                        GW_UI_TOUCH_MIN, 0);
        lv_obj_t *refresh = make_button(row, "Refresh",
                                        (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                        GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(install, apps_install_usb_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(refresh, apps_refresh_cb, LV_EVENT_CLICKED, NULL);
    }
    g_apps_message = gw_ui_label(content,
                                 "Bundles are signature-checked before install.",
                                 &gw_font_28, gw_ui_c_secondary());
    lv_obj_set_width(g_apps_message, LV_PCT(100));
    lv_label_set_long_mode(g_apps_message, LV_LABEL_LONG_DOT);
    {
        lv_obj_t *update = make_button(content, "App updates",
                                       GW_UI_CONTENT_W, GW_UI_TOUCH_MIN, 1);
        lv_obj_add_event_cb(update, update_open_cb, LV_EVENT_CLICKED, NULL);
    }

    line = make_hairline(content, GW_UI_CONTENT_W);
    (void)line;
    settings_section(content, "BUILT-IN");
    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 120);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *col = lv_obj_create(row);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_height(col, 100);
        gw_ui_style_plain_panel(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        gw_ui_label(col, "Reader", &gw_font_36_bold, gw_ui_c_text());
        gw_ui_label(col, "KOReader · built into this OS", &gw_font_24,
                    gw_ui_c_secondary());
        {
            lv_obj_t *launch = make_button(row, "Open", 170, GW_UI_TOUCH_MIN, 0);
            lv_obj_add_event_cb(launch, reader_launch, LV_EVENT_CLICKED, NULL);
        }
    }
    label = gw_ui_label(content,
                        "Uninstall keeps application data. Old versions remain "
                        "available for rollback.",
                        &gw_font_24, gw_ui_c_muted());
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    apps_refresh();
}

static void build_update_screen(void) {
    lv_obj_t *content, *row, *label;

    g_update_screen = make_screen();
    make_app_bar(g_update_screen, "App updates", nav_home_cb);
    make_bottom_nav(g_update_screen, -1);
    content = make_content(g_update_screen, GW_UI_APP_BAR_H + GW_UI_GAP);

    settings_section(content, "SYSTEM");
    label = gw_ui_label(content, "GleWorks OS 0.1 · app bundles",
                        &gw_font_36_bold, gw_ui_c_text());
    lv_obj_set_width(label, LV_PCT(100));
    label = gw_ui_label(content,
                        "Signed application bundles from USB.\n"
                        "Previous versions are retained for rollback. OS image "
                        "A/B update is not enabled.",
                        &gw_font_24, gw_ui_c_secondary());
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    settings_section(content, "USB PACKAGES");
    row = lv_obj_create(content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_TOUCH_MIN);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, GW_UI_GAP, LV_PART_MAIN);
    {
        lv_obj_t *scan = make_button(row, "Check USB",
                                     (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                     GW_UI_TOUCH_MIN, 1);
        lv_obj_t *install = make_button(row, "Install update",
                                        (GW_UI_CONTENT_W - GW_UI_GAP) / 2,
                                        GW_UI_TOUCH_MIN, 0);
        lv_obj_add_event_cb(scan, update_scan_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(install, update_install_cb, LV_EVENT_CLICKED, NULL);
    }
    g_update_list = gw_ui_label(content, "Press Check USB", &gw_font_28,
                                gw_ui_c_text());
    lv_obj_set_width(g_update_list, LV_PCT(100));
    lv_label_set_long_mode(g_update_list, LV_LABEL_LONG_WRAP);
    g_update_message = gw_ui_label(content, "No update applied", &gw_font_28,
                                   gw_ui_c_secondary());
    lv_obj_set_width(g_update_message, LV_PCT(100));
    lv_label_set_long_mode(g_update_message, LV_LABEL_LONG_DOT);

    label = gw_ui_label(content,
                        "Only bundles signed by the configured release key are "
                        "accepted. Corrupt or unsigned files are rejected before "
                        "extraction.",
                        &gw_font_24, gw_ui_c_muted());
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
}

/* ── Lock screen ────────────────────────────────────────────────────────── */

static void build_lock_screen(void) {
    lv_obj_t *button;
    g_lock_screen = make_screen();
    g_lock_label = gw_ui_label(g_lock_screen, "Screen locked", &gw_font_44_bold,
                               gw_ui_c_text());
    lv_obj_align(g_lock_label, LV_ALIGN_CENTER, 0, -120);
    {
        lv_obj_t *hint = gw_ui_label(g_lock_screen, "Touch Unlock to continue",
                                     &gw_font_28, gw_ui_c_secondary());
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, -40);
    }
    button = make_button(g_lock_screen, "Unlock", 300, GW_UI_TOUCH_MIN, 0);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 100);
    lv_obj_add_event_cb(button, unlock_screen_cb, LV_EVENT_CLICKED, NULL);
}

/* ── Model application and screen switching ─────────────────────────────── */

static void apply_model(void) {
    int has_book = g_model.book_title[0] != '\0';
    struct tm t;
    char text[256];
    gw_ui_now(&t);

    if (g_reading_body && g_reading_empty) {
        container_set_hidden(g_reading_body, !has_book);
        container_set_hidden(g_reading_empty, has_book);
    }
    if (has_book) {
        label_set_if_changed(g_book_title_label, g_model.book_title);
        label_set_if_changed(g_book_author_label,
                             g_model.book_author[0] ? g_model.book_author
                                                    : "Unknown author");
        if (g_model.book_progress >= 0 && g_model.book_progress <= 100) {
            snprintf(text, sizeof(text), "%d%% read", g_model.book_progress);
            label_set_if_changed(g_book_progress_label, text);
            container_set_hidden(g_book_bar, 0);
            lv_bar_set_value(g_book_bar, g_model.book_progress, LV_ANIM_OFF);
        } else {
            label_set_if_changed(g_book_progress_label, "Progress unavailable");
            container_set_hidden(g_book_bar, 1);
        }
    }

    label_set_if_changed(g_widget_note_label,
                         g_model.note_preview[0] ? g_model.note_preview
                                                 : "No tasks yet.");

    {
        char day[8], month[32], weekday[24];
        snprintf(day, sizeof(day), "%d", t.tm_mday);
        snprintf(month, sizeof(month), "%s %d", vi_month[t.tm_mon],
                 t.tm_year + 1900);
        snprintf(weekday, sizeof(weekday), "%s", vi_weekday[t.tm_wday]);
        label_set_if_changed(g_widget_day_label, day);
        label_set_if_changed(g_widget_month_label, month);
        label_set_if_changed(g_widget_weekday_label, weekday);
    }

    if (g_model.event_title[0]) {
        snprintf(text, sizeof(text), "%s · %s",
                 g_model.event_when[0] ? g_model.event_when : "Today",
                 g_model.event_title);
    } else {
        snprintf(text, sizeof(text), "No upcoming events");
    }
    label_set_if_changed(g_widget_event_label, text);
    label_set_if_changed(g_calendar_event_label, text);
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
    if (g_wifi_keyboard) lv_obj_add_flag(g_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (g_wifi_list && lv_dropdown_is_open(g_wifi_list))
        lv_dropdown_close(g_wifi_list);
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
    status_update(NULL);
    lv_timer_create(status_update, 30000, NULL);
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
        if (g_wifi_list) lv_dropdown_set_options(g_wifi_list,
            "Alpha Network\nBravo Network\nCharlie Network\nDelta Network\n"
            "Echo Network\nFoxtrot Network\nGolf Network\nHotel Network\n"
            "India Network\nJuliet Network");
    } else if (!strcmp(cmd, "TYPE")) {
        /* Diagnostic-only: emulate one text-field change plus the key
         * highlight dirty region, without injecting touch events. */
        static int setup;
        if (g_wifi_password && g_wifi_keyboard) {
            lv_area_t kb, key;
            if (!setup) {
                gw_ui_show(GW_SCREEN_NETWORK);
                g_text_input_active = 1;
                gw_eink_set_text_input(1);
                lv_obj_clear_flag(g_wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(g_wifi_keyboard);
                lv_keyboard_set_textarea(g_wifi_keyboard, g_wifi_password);
                setup = 1;
            }
            gw_eink_note_input();
            lv_textarea_add_text(g_wifi_password, "a");
            lv_obj_get_coords(g_wifi_keyboard, &kb);
            key.x1 = kb.x1 + 6;
            key.y1 = kb.y1 + 6;
            key.x2 = kb.x1 + 100;
            key.y2 = kb.y1 + 110;
            lv_obj_invalidate_area(g_wifi_keyboard, &key);
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
        if (exit_status && atoi(exit_status + strlen(" reader-exit=")) != 0 &&
            g_recovery_label) {
            lv_label_set_text(g_recovery_label, "Reader stopped unexpectedly.");
        }
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
        /* Track the dropdown popup after LVGL has completed its layout and
         * positioning.  Popup frames use DU partial updates and are exempt
         * from the full-screen area promotion. */
        {
            static int popup_was_open;
            int popup_open = g_wifi_list && lv_dropdown_is_open(g_wifi_list);
            if (popup_open) {
                lv_obj_t *list = lv_dropdown_get_list(g_wifi_list);
                if (list) {
                    lv_area_t a, d;
                    lv_obj_get_coords(list, &a);
                    lv_obj_get_coords(g_wifi_list, &d);
                    if (d.x1 < a.x1) a.x1 = d.x1;
                    if (d.y1 < a.y1) a.y1 = d.y1;
                    if (d.x2 > a.x2) a.x2 = d.x2;
                    if (d.y2 > a.y2) a.y2 = d.y2;
                    gw_eink_set_popup_rect(1, a.x1, a.y1, a.x2, a.y2);
                }
            } else if (popup_was_open) {
                gw_eink_set_popup_rect(0, 0, 0, 0, 0);
                gw_eink_hold_partial(500);
            }
            popup_was_open = popup_open;
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

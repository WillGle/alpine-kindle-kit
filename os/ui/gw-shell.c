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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
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
#include "gw_shell_types.h"
#define GW_SHELL_STATE_IMPLEMENTATION
#include "gw_shell_state.h"
#undef GW_SHELL_STATE_IMPLEMENTATION

/* File-scope functions are external only for the host test binary.  Production
 * builds keep internal linkage; local statics are untouched. */
#if GW_UI_HOST_SIM
#define GW_SHELL_INTERNAL
#else
#define GW_SHELL_INTERNAL static
#endif

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

#include "gw_ipc_client.inc"
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
    case GW_SCREEN_LOCK: return "lock";
    default: return "unknown";
    }
}

/* ── English date and calendar helpers ─────────────────────────────────── */

GW_SHELL_INTERNAL void gw_ui_now(struct tm *out) {
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

GW_SHELL_INTERNAL void format_hero_date(char *buf, size_t size) {
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

GW_SHELL_INTERNAL void format_month_title(int year, int month, char *buf, size_t size) {
    snprintf(buf, size, "%s %d", gw_str_month[month], year);
}

GW_SHELL_INTERNAL void format_iso_date(int year, int month, int day, char *buf, size_t size) {
    snprintf(buf, size, "%04d-%02d-%02d", year, month + 1, day);
}

GW_SHELL_INTERNAL void today_iso(char *buf, size_t size) {
    struct tm t;
    gw_ui_now(&t);
    format_iso_date(t.tm_year + 1900, t.tm_mon, t.tm_mday, buf, size);
}

GW_SHELL_INTERNAL void iso_relative_days(int days, char *buf, size_t size) {
    struct tm t;
    gw_ui_now(&t);
    t.tm_mday += days;
    t.tm_hour = 12;
    t.tm_min = 0;
    t.tm_sec = 0;
    mktime(&t);
    format_iso_date(t.tm_year + 1900, t.tm_mon, t.tm_mday, buf, size);
}

GW_SHELL_INTERNAL void format_due_label(const char *iso, char *buf, size_t size) {
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

GW_SHELL_INTERNAL int days_in_month(int year, int month) {
    static const int days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month];
}

/* ── TODO storage ───────────────────────────────────────────────────────── */

GW_SHELL_INTERNAL void todo_rebuild_list(void);
GW_SHELL_INTERNAL void apply_model(void);
GW_SHELL_INTERNAL void todo_restore_action(void *user_data);


GW_SHELL_INTERNAL void todo_sanitize(char *dst, size_t dst_size, const char *src) {
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

GW_SHELL_INTERNAL void todo_copy_due(char *dst, size_t dst_size, const char *src) {
    int year, month, day;
    if (!src || !src[0] || sscanf(src, "%d-%d-%d", &year, &month, &day) != 3 ||
        month < 1 || month > 12 || day < 1 || day > 31) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%04d-%02d-%02d", year, month, day);
}

#include "gw_format.inc"
#include "gw_todo_store.inc"
#include "gw_status.inc"
#include "gw_shell_common.inc"
#include "gw_screen_home.inc"
#include "gw_screen_settings.inc"
#include "gw_screen_calendar.inc"
#include "gw_screen_notes.inc"
#include "gw_screen_network.inc"
#include "gw_screen_apps.inc"
#include "gw_screen_lock.inc"
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
    build_update_screen();
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
    case GW_SCREEN_LOCK: target = g_lock_screen; break;
    default: return;
    }
    if (!target) return;

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
        lock_refresh_time();
    }

    g_active_screen = screen;
    gw_eink_request_scene();
    lv_scr_load(target);
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
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
    gw_ui_kit_set_modal_close_cb(gw_ui_request_full);
    status_update(NULL);
    lv_timer_create(status_update, 30000, NULL);
    lv_timer_create(ui_tick_cb, 1000, NULL);
}

/* ── Crash diagnostics ──────────────────────────────────────────────────── */
GW_SHELL_INTERNAL void crash_handler(int sig, siginfo_t *info, void *ctx) {
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
    if (getenv("GW_UI_SMOKE_FB")) {
        dprintf(2, "GW_SHELL: QEMU smoke screens built\n");
        return 0;
    }
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
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
#if GW_UI_PERF
        render_end = gw_now_us();
#endif
        /* A dismissed keyboard is an ordinary content paint.  It must never
         * trigger a GC16 flash (U1). */
#if GW_UI_EPDC_ACCEPTED || GW_UI_EPDC_SPIKE
        if (g_settle_refresh_pending && !g_text_input_active &&
            !g_eink_popup_valid) {
            g_settle_refresh_pending = 0;
            gw_eink_request_content();
        }
        if (gw_epdc_scroll_cleanup_due()) {
            dprintf(2, "GW_UI: scroll idle clean fired\n");
            gw_epdc_scroll_cleanup_done();
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

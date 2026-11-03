/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_shell_internal.h"

/* ── Lock screen ────────────────────────────────────────────────────────── */

#define LOCK_CONF   "/mnt/us/gleworks/lockscreen.conf"
#define LOCK_RAW    "/mnt/us/gleworks/lockscreen.raw"
#define LOCK_RAW_MAX (GW_UI_PANEL_W * GW_UI_PANEL_H)

static uint8_t *g_lock_pixels;
static lv_img_dsc_t g_lock_dsc;
static time_t g_lock_config_mtime;
static time_t g_lock_raw_mtime;
static int g_lock_image_valid;

static int lock_read_full(int fd, uint8_t *out, size_t len) {
    size_t done = 0;
    while (done < len) {
        ssize_t got = read(fd, out + done, len - done);
        if (got < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (got == 0) return -1;
        done += (size_t)got;
    }
    return 0;
}

static int lock_image_load(void) {
    int fd;
    struct stat st;

    if (!g_lock_pixels) {
        g_lock_pixels = malloc(LOCK_RAW_MAX);
        if (!g_lock_pixels) return 0;
        g_lock_dsc.header.always_zero = 0;
        g_lock_dsc.header.w = GW_UI_PANEL_W;
        g_lock_dsc.header.h = GW_UI_PANEL_H;
        g_lock_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        g_lock_dsc.data_size = LOCK_RAW_MAX;
        g_lock_dsc.data = g_lock_pixels;
    }
    fd = open(LOCK_RAW, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return 0;
    if (fstat(fd, &st) != 0 || st.st_size != LOCK_RAW_MAX) {
        close(fd);
        g_lock_image_valid = 0;
        return 0;
    }
    if (g_lock_image_valid && st.st_mtime == g_lock_raw_mtime) {
        close(fd);
        return 1;
    }
    if (lock_read_full(fd, g_lock_pixels, LOCK_RAW_MAX) < 0) {
        close(fd);
        g_lock_image_valid = 0;
        return 0;
    }
    close(fd);
    /* KOReader exports raw Y8; LVGL draws RGB332. */
    gw_y8_to_rgb332_buffer(g_lock_pixels, LOCK_RAW_MAX);
    g_lock_raw_mtime = st.st_mtime;
    g_lock_image_valid = 1;
    return 1;
}

GW_SHELL_INTERNAL void lock_refresh_time(void) {
    struct tm t;
    char clock_text[16], date[96];
    gw_ui_now(&t);
    snprintf(clock_text, sizeof(clock_text), "%02d:%02d", t.tm_hour, t.tm_min);
    label_set_if_changed(g_lock_time_label, clock_text);
    label_set_if_changed(g_lock_label, GW_STR_TAP_TO_UNLOCK);
    snprintf(date, sizeof(date), "%s, %d %s %d", gw_str_weekday[t.tm_wday],
             t.tm_mday, gw_str_month[t.tm_mon], t.tm_year + 1900);
    label_set_if_changed(g_lock_date_label, date);
    container_set_hidden(g_lock_time_label, !g_lock_show_clock);
    container_set_hidden(g_lock_date_label, !g_lock_show_date);
}

GW_SHELL_INTERNAL int lock_config_read(gw_lock_config_t *cfg) {
    struct stat st;
    FILE *file;
    char line[256];
    if (!cfg) return -1;
    gw_lock_config_defaults(cfg);
#if GW_UI_HOST_SIM
    return -1;
#else
    if (stat(LOCK_CONF, &st) != 0) return -1;
    file = fopen(LOCK_CONF, "r");
    if (!file) return -1;
    while (fgets(line, sizeof(line), file))
        gw_lock_config_apply_line(cfg, line);
    fclose(file);
    return 0;
#endif
}

GW_SHELL_INTERNAL int lock_config_write(const gw_lock_config_t *cfg) {
    char data[512];
    int length;
    if (!cfg) return -1;
    length = snprintf(data, sizeof(data),
                      "version=1\n"
                      "use_image=%d\n"
                      "image=%s\n"
                      "show_panel=%d\n"
                      "show_clock=%d\n"
                      "show_date=%d\n"
                      "message=%s\n",
                      cfg->use_image, LOCK_RAW, cfg->show_panel,
                      cfg->show_clock, cfg->show_date, cfg->message);
    if (length <= 0 || (size_t)length >= sizeof(data)) return -1;
    return gw_write_atomic(LOCK_CONF, data, (size_t)length, 0600);
}

GW_SHELL_INTERNAL void lock_config_load(void) {
    gw_lock_config_t cfg;
    gw_lock_config_defaults(&cfg);
    struct stat st;

#if GW_UI_HOST_SIM
    g_lock_config_mtime = 0;
#else
    if (stat(LOCK_CONF, &st) == 0) g_lock_config_mtime = st.st_mtime;
    else g_lock_config_mtime = 0;
    if (lock_config_read(&cfg) < 0) gw_lock_config_defaults(&cfg);
#endif

    g_lock_show_panel = cfg.show_panel;
    g_lock_show_clock = cfg.show_clock;
    g_lock_show_date = cfg.show_date;
    if (g_lock_panel) container_set_hidden(g_lock_panel, !g_lock_show_panel);

    if (cfg.use_image && lock_image_load()) {
        if (g_lock_image) {
            lv_img_set_src(g_lock_image, &g_lock_dsc);
            lv_obj_set_size(g_lock_image, GW_UI_PANEL_W, GW_UI_PANEL_H);
            lv_obj_clear_flag(g_lock_image, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (g_lock_image) {
        lv_img_set_src(g_lock_image, NULL);
        lv_obj_add_flag(g_lock_image, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_lock_message_label) {
        label_set_if_changed(g_lock_message_label, cfg.message);
        container_set_hidden(g_lock_message_label, cfg.message[0] == '\0');
    }
    lock_refresh_time();
}

GW_SHELL_INTERNAL void lock_config_poll(void) {
#if !GW_UI_HOST_SIM
    struct stat st;
    if (stat(LOCK_CONF, &st) != 0) {
        /* Removing the config is a reset, not a reason to keep the old
         * image and message. */
        if (g_lock_config_mtime != 0) lock_config_load();
    } else if (st.st_mtime != g_lock_config_mtime) {
        lock_config_load();
    }
#endif
}

GW_SHELL_INTERNAL void build_lock_screen(void) {
    g_lock_screen = make_screen();

    g_lock_image = lv_img_create(g_lock_screen);
    lv_obj_set_size(g_lock_image, GW_UI_PANEL_W, GW_UI_PANEL_H);
    lv_obj_set_pos(g_lock_image, 0, 0);
    lv_obj_add_flag(g_lock_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_lock_image, LV_OBJ_FLAG_CLICKABLE);

    /* A white card keeps the clock readable over an arbitrary image. */
    g_lock_panel = lv_obj_create(g_lock_screen);
    lv_obj_set_size(g_lock_panel, 860, 460);
    lv_obj_align(g_lock_panel, LV_ALIGN_CENTER, 0, 0);
    gw_ui_style_plain_panel(g_lock_panel);
    lv_obj_set_style_border_width(g_lock_panel, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_lock_panel, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_clear_flag(g_lock_panel, LV_OBJ_FLAG_CLICKABLE);

    g_lock_time_label = gw_ui_label(g_lock_panel, "00:00", &gw_font_72_bold,
                                    gw_ui_c_text());
    lv_obj_align(g_lock_time_label, LV_ALIGN_TOP_MID, 0, 40);

    g_lock_date_label = gw_ui_label(g_lock_panel, "", &gw_font_36,
                                    gw_ui_c_secondary());
    lv_obj_align(g_lock_date_label, LV_ALIGN_TOP_MID, 0, 140);

    g_lock_message_label = gw_ui_label(g_lock_panel, "", &gw_font_28,
                                       gw_ui_c_secondary());
    lv_obj_set_width(g_lock_message_label, 760);
    lv_obj_set_style_text_align(g_lock_message_label, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);
    lv_label_set_long_mode(g_lock_message_label, LV_LABEL_LONG_DOT);
    lv_obj_align(g_lock_message_label, LV_ALIGN_TOP_MID, 0, 210);
    container_set_hidden(g_lock_message_label, 1);

    /* One unlock affordance: the whole surface taps to unlock. */
    g_lock_label = gw_ui_label(g_lock_panel, GW_STR_TAP_TO_UNLOCK,
                               &gw_font_32, gw_ui_c_secondary());
    lv_obj_align(g_lock_label, LV_ALIGN_BOTTOM_MID, 0, -40);

    gw_ui_style_screen(g_lock_screen);
    lv_obj_add_flag(g_lock_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_lock_screen, unlock_screen_cb, LV_EVENT_CLICKED, NULL);
    lock_config_load();
}

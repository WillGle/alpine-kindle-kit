/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_shell_internal.h"

/* ── Settings ───────────────────────────────────────────────────────────── */

GW_SHELL_INTERNAL lv_obj_t *settings_section(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = gw_ui_kit_section_label_create(parent, text);
    lv_obj_set_style_pad_top(label, GW_UI_SPACE_2, LV_PART_MAIN);
    return label;
}

GW_SHELL_INTERNAL lv_obj_t *settings_row(lv_obj_t *parent, const char *title,
                              const char *subtitle, const char *button_text,
                              lv_event_cb_t cb, lv_obj_t **value_out) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *col;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_SETTINGS_ROW_H);
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

    (void)button_text;
    if (cb) {
        set_clickable(row);
        lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *chevron = gw_ui_label(row, GW_ICON_CHEVRON_RIGHT,
                                        &gw_font_28, gw_ui_c_muted());
        lv_obj_set_width(chevron, 36);
        lv_obj_set_style_text_align(chevron, LV_TEXT_ALIGN_RIGHT,
                                    LV_PART_MAIN);
    }
    return row;
}

GW_SHELL_INTERNAL lv_obj_t *settings_info_row(lv_obj_t *parent, const char *title,
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

GW_SHELL_INTERNAL void format_size(double bytes, char *buf, size_t size) {
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const double mib = 1024.0 * 1024.0;
    if (bytes >= gib)
        snprintf(buf, size, "%.1f GB", bytes / gib);
    else if (bytes >= mib)
        snprintf(buf, size, "%.0f MB", bytes / mib);
    else
        snprintf(buf, size, "%.0f KB", bytes / 1024.0);
}

GW_SHELL_INTERNAL void format_storage_value(double free_bytes, double total_bytes,
                                 char *buf, size_t size) {
    char free_text[24], total_text[24];
    format_size(free_bytes, free_text, sizeof(free_text));
    format_size(total_bytes, total_text, sizeof(total_text));
    snprintf(buf, size, "%s free of %s", free_text, total_text);
}

GW_SHELL_INTERNAL void settings_storage_value(char *buf, size_t size) {
#if GW_UI_HOST_SIM
    snprintf(buf, size, "20.1 GB free of 32.0 GB");
#else
    /* Settings' "Storage" means user storage, not the read-only OS image.
     * /mnt/us holds documents/app data and is much larger than the rootfs. */
    static const char *const paths[] = { "/mnt/us", "/var/lib/gleworks", "/" };
    size_t i;
    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        struct statvfs vfs;
        double free_bytes, total_bytes;
        if (statvfs(paths[i], &vfs) != 0) continue;
        free_bytes = (double)vfs.f_bavail * (double)vfs.f_frsize;
        total_bytes = (double)vfs.f_blocks * (double)vfs.f_frsize;
        format_storage_value(free_bytes, total_bytes, buf, size);
        return;
    }
    buf[0] = '\0';
#endif
}

GW_SHELL_INTERNAL void settings_uptime_value(char *buf, size_t size) {
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

static lv_obj_t *g_settings_device_name_label;
static lv_obj_t *g_settings_time_label;
static lv_obj_t *g_settings_timezone_label;
static lv_obj_t *g_settings_edit_modal;
static lv_obj_t *g_settings_edit_area;
static int g_settings_edit_mode;

enum {
    SETTINGS_EDIT_DEVICE_NAME = 1,
    SETTINGS_EDIT_TIME,
    SETTINGS_EDIT_TIMEZONE
};

static void settings_name_refresh(void) {
    char response[GW_IPC_MAX_LINE];
    if (ipc_cmd(GW_CMD_DEVICE_NAME_GET, response, sizeof(response)) == 0 &&
        !strncmp(response, "DEVICE NAME ", 12)) {
        char *name = response + 12;
        char *nl = strchr(name, '\n');
        if (nl) *nl = '\0';
        label_set_if_changed(g_settings_device_name_label, name);
    }
}

static void settings_time_refresh(void) {
    char response[GW_IPC_MAX_LINE];
    long epoch;
    char tz[64];
    int format, used = 0;
    struct tm t;
    if (ipc_cmd(GW_CMD_TIME_GET, response, sizeof(response)) == 0 &&
        sscanf(response, "TIME %ld %63s %d %n", &epoch, tz, &format,
               &used) >= 3) {
        char *formatted = response + used;
        char *nl = strchr(formatted, '\n');
        (void)epoch;
        (void)format;
        if (nl) *nl = '\0';
        label_set_if_changed(g_settings_time_label, formatted);
        label_set_if_changed(g_settings_timezone_label, tz);
        gw_ui_apply_timezone(tz);
    }
    gw_ui_now(&t);
    {
        char date[96];
        snprintf(date, sizeof(date), "%s, %d %s %d", gw_str_weekday[t.tm_wday],
                 t.tm_mday, gw_str_month[t.tm_mon], t.tm_year + 1900);
        label_set_if_changed(g_settings_date_label, date);
    }
}

static void settings_edit_close(void) {
    if (!g_settings_edit_modal) return;
    lv_obj_del(g_settings_edit_modal);
    g_settings_edit_modal = NULL;
    g_settings_edit_area = NULL;
    g_text_input_active = 0;
    gw_eink_set_text_input(0);
    gw_eink_set_keyboard_rect(0, 0, 0, 0, 0);
}

static void settings_edit_save(void) {
    const char *text = g_settings_edit_area ? lv_textarea_get_text(g_settings_edit_area)
                                            : "";
    char command[160], response[GW_IPC_MAX_LINE];
    int ok = 0;
    if (!text) text = "";
    if (g_settings_edit_mode == SETTINGS_EDIT_DEVICE_NAME)
        snprintf(command, sizeof(command), GW_CMD_DEVICE_NAME_SET " %s", text);
    else if (g_settings_edit_mode == SETTINGS_EDIT_TIME)
        snprintf(command, sizeof(command), GW_CMD_TIME_SET_LOCAL " %s", text);
    else
        snprintf(command, sizeof(command), GW_CMD_TIMEZONE_SET " %s", text);
    if (ipc_cmd(command, response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2))
        ok = 1;
    if (ok) {
        settings_name_refresh();
        settings_time_refresh();
        settings_facts_update();
        status_refresh_now();
        recovery_set(GW_STR_SETTING_SAVED);
    } else if (g_settings_edit_mode == SETTINGS_EDIT_DEVICE_NAME) {
        recovery_set(GW_STR_DEVICE_NAME_FAILED);
    } else if (g_settings_edit_mode == SETTINGS_EDIT_TIME) {
        recovery_set(GW_STR_TIME_SET_FAILED);
    } else {
        recovery_set(GW_STR_TIMEZONE_SET_FAILED);
    }
    settings_edit_close();
    gw_ui_request_full(NULL);
}

static void settings_edit_save_cb(lv_event_t *event) {
    (void)event;
    settings_edit_save();
}

static void settings_edit_cancel_cb(lv_event_t *event) {
    (void)event;
    settings_edit_close();
    gw_ui_request_full(NULL);
}

static void settings_edit_keyboard_cb(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY) settings_edit_save();
    else if (code == LV_EVENT_CANCEL) settings_edit_cancel_cb(event);
}

static void settings_edit_open(const char *title, const char *initial, int mode) {
    lv_obj_t *modal, *label, *row, *keyboard;
    if (g_settings_edit_modal) return;
    g_settings_edit_mode = mode;
    modal = lv_obj_create(lv_layer_top());
    g_settings_edit_modal = modal;
    lv_obj_set_size(modal, GW_UI_PANEL_W, GW_UI_PANEL_H);
    lv_obj_set_pos(modal, 0, 0);
    lv_obj_set_style_bg_color(modal, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(modal, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(modal, 0, LV_PART_MAIN);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);

    label = gw_ui_label(modal, title, &gw_font_44_bold, gw_ui_c_text());
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 40);

    g_settings_edit_area = lv_textarea_create(modal);
    lv_obj_set_size(g_settings_edit_area, GW_UI_CONTENT_W, 120);
    lv_obj_align(g_settings_edit_area, LV_ALIGN_TOP_MID, 0, 140);
    lv_textarea_set_one_line(g_settings_edit_area, true);
    lv_textarea_set_max_length(g_settings_edit_area, mode == SETTINGS_EDIT_TIME
                                                        ? 16
                                                        : 63);
    lv_textarea_set_text(g_settings_edit_area, initial ? initial : "");
    lv_obj_set_style_text_font(g_settings_edit_area, &gw_font_32, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_settings_edit_area, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_settings_edit_area, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_settings_edit_area, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_settings_edit_area, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_radius(g_settings_edit_area, 0, LV_PART_MAIN);

    row = lv_obj_create(modal);
    lv_obj_set_size(row, GW_UI_CONTENT_W, GW_UI_TOUCH_MIN);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 290);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *cancel = gw_ui_kit_button_create(
            row, GW_STR_CANCEL, 440, GW_UI_TOUCH_MIN, GW_UI_BUTTON_SECONDARY);
        lv_obj_t *save = gw_ui_kit_button_create(
            row, GW_STR_SAVE, 440, GW_UI_TOUCH_MIN, GW_UI_BUTTON_PRIMARY);
        lv_obj_add_event_cb(cancel, settings_edit_cancel_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(save, settings_edit_save_cb, LV_EVENT_CLICKED, NULL);
    }

    keyboard = lv_keyboard_create(modal);
    lv_obj_set_size(keyboard, GW_UI_PANEL_W, 480);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    gw_ui_kit_style_keyboard(keyboard);
    lv_keyboard_set_textarea(keyboard, g_settings_edit_area);
    lv_obj_add_event_cb(keyboard, settings_edit_keyboard_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(keyboard, settings_edit_keyboard_cb, LV_EVENT_CANCEL, NULL);

    g_text_input_active = 1;
    gw_eink_set_text_input(1);
    gw_eink_set_keyboard_rect(1, 0, GW_UI_PANEL_H - 480,
                              GW_UI_PANEL_W - 1, GW_UI_PANEL_H - 1);
    gw_ui_request_full(NULL);
}

static void settings_device_name_cb(lv_event_t *event) {
    (void)event;
    settings_edit_open(GW_STR_DEVICE_NAME,
                       g_settings_device_name_label
                           ? lv_label_get_text(g_settings_device_name_label)
                           : "",
                       SETTINGS_EDIT_DEVICE_NAME);
}

static void settings_time_edit_cb(lv_event_t *event) {
    const char *current = g_settings_time_label
                              ? lv_label_get_text(g_settings_time_label)
                              : "";
    char fallback[32];
    struct tm t;
    (void)event;
    if (!current || strlen(current) < 10 ||
        !strchr(current, '-') || !strchr(current, ':')) {
        gw_ui_now(&t);
        snprintf(fallback, sizeof(fallback), "%04d-%02d-%02d %02d:%02d",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
                 t.tm_min);
        current = fallback;
    }
    settings_edit_open(GW_STR_DATE_TIME, current, SETTINGS_EDIT_TIME);
}

static void settings_timezone_edit_cb(lv_event_t *event) {
    (void)event;
    settings_edit_open(GW_STR_TIME_ZONE,
                       g_settings_timezone_label
                           ? lv_label_get_text(g_settings_timezone_label)
                           : "",
                       SETTINGS_EDIT_TIMEZONE);
}

static void settings_sync_timer_cb(lv_timer_t *timer) {
    settings_time_refresh();
    settings_facts_update();
    status_refresh_now();
    lv_timer_del(timer);
}

static void settings_sync_now_cb(lv_event_t *event) {
    char response[GW_IPC_MAX_LINE];
    (void)event;
    if (ipc_cmd(GW_CMD_TIME_SYNC, response, sizeof(response)) == 0 &&
        !strncmp(response, "OK", 2)) {
        recovery_set(GW_STR_TIME_SYNC_STARTED);
        lv_timer_create(settings_sync_timer_cb, 6000, NULL);
    } else {
        recovery_set(GW_STR_TIME_SYNC_FAILED);
    }
}

GW_SHELL_INTERNAL void settings_facts_update(void) {
    char text[64];
    settings_storage_value(text, sizeof(text));
    if (text[0]) label_set_if_changed(g_settings_storage_label, text);
    container_set_hidden(g_settings_storage_row, !text[0]);
    settings_uptime_value(text, sizeof(text));
    if (text[0]) label_set_if_changed(g_settings_uptime_label, text);
    container_set_hidden(g_settings_uptime_row, !text[0]);
    label_set_if_changed(g_settings_version_label, GW_STR_OS_VERSION);
    settings_name_refresh();
    settings_time_refresh();
}

static lv_obj_t *settings_toggle_row(lv_obj_t *parent, const char *title,
                                     const char *subtitle, int checked,
                                     lv_event_cb_t cb) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *col;
    lv_obj_t *sw;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_SETTINGS_ROW_H);
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
    }
    sw = lv_switch_create(row);
    lv_obj_set_size(sw, 100, 52);
    lv_obj_set_style_bg_color(sw, gw_ui_c_border(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sw, GW_UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(sw, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, gw_ui_c_text(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw, gw_ui_c_white(), LV_PART_KNOB);
    lv_obj_set_style_border_width(sw, GW_UI_BORDER_W, LV_PART_KNOB);
    lv_obj_set_style_border_color(sw, gw_ui_c_text(), LV_PART_KNOB);
    if (checked) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return sw;
}

static void lock_toggle_write(int field, lv_obj_t *sw) {
    gw_lock_config_t cfg;
    int on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (lock_config_read(&cfg) < 0) gw_lock_config_defaults(&cfg);
    if (field == 0) cfg.show_panel = on;
    else if (field == 1) cfg.show_clock = on;
    else cfg.show_date = on;
    (void)lock_config_write(&cfg);
}

static void lock_panel_cb(lv_event_t *event) {
    lock_toggle_write(0, lv_event_get_target(event));
}
static void lock_clock_cb(lv_event_t *event) {
    lock_toggle_write(1, lv_event_get_target(event));
}
static void lock_date_cb(lv_event_t *event) {
    lock_toggle_write(2, lv_event_get_target(event));
}
static lv_obj_t *g_lock_msg_modal;
static lv_obj_t *g_lock_msg_area;

GW_SHELL_INTERNAL void lock_message_modal_destroy(void) {
    if (!g_lock_msg_modal) return;
    lv_obj_del(g_lock_msg_modal);
    g_lock_msg_modal = NULL;
    g_lock_msg_area = NULL;
    g_text_input_active = 0;
    gw_eink_set_text_input(0);
    gw_eink_set_keyboard_rect(0, 0, 0, 0, 0);
}

static void lock_msg_close(void) {
    lock_message_modal_destroy();
    gw_ui_request_full(NULL);
}

static void lock_msg_save(void) {
    gw_lock_config_t cfg;
    const char *text = g_lock_msg_area ? lv_textarea_get_text(g_lock_msg_area)
                                       : "";
    if (lock_config_read(&cfg) < 0) gw_lock_config_defaults(&cfg);
    snprintf(cfg.message, sizeof(cfg.message), "%s", text ? text : "");
    (void)lock_config_write(&cfg);
    lock_msg_close();
}

static void lock_msg_save_cb(lv_event_t *event) { (void)event; lock_msg_save(); }
static void lock_msg_cancel_cb(lv_event_t *event) { (void)event; lock_msg_close(); }
static void lock_msg_clear_cb(lv_event_t *event) {
    (void)event;
    if (g_lock_msg_area) lv_textarea_set_text(g_lock_msg_area, "");
    lock_msg_save();
}
static void lock_msg_keyboard_cb(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY) lock_msg_save();
    else if (code == LV_EVENT_CANCEL) lock_msg_close();
}

static void lock_message_edit_cb(lv_event_t *event) {
    lv_obj_t *modal, *title, *row, *keyboard;
    gw_lock_config_t cfg;
    (void)event;
    if (g_lock_msg_modal) return;
    if (lock_config_read(&cfg) < 0) gw_lock_config_defaults(&cfg);

    modal = lv_obj_create(lv_layer_top());
    g_lock_msg_modal = modal;
    lv_obj_set_size(modal, GW_UI_PANEL_W, GW_UI_PANEL_H);
    lv_obj_set_pos(modal, 0, 0);
    lv_obj_set_style_bg_color(modal, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(modal, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(modal, 0, LV_PART_MAIN);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);

    title = gw_ui_label(modal, GW_STR_LOCK_MESSAGE_TITLE, &gw_font_44_bold,
                        gw_ui_c_text());
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    g_lock_msg_area = lv_textarea_create(modal);
    lv_obj_set_size(g_lock_msg_area, GW_UI_CONTENT_W, 120);
    lv_obj_align(g_lock_msg_area, LV_ALIGN_TOP_MID, 0, 140);
    lv_textarea_set_one_line(g_lock_msg_area, true);
    lv_textarea_set_max_length(g_lock_msg_area, GW_LOCK_MESSAGE_MAX - 1);
    lv_textarea_set_text(g_lock_msg_area, cfg.message);
    lv_obj_set_style_text_font(g_lock_msg_area, &gw_font_32, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_lock_msg_area, gw_ui_c_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lock_msg_area, gw_ui_c_text(), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_lock_msg_area, GW_UI_BORDER_W,
                                  LV_PART_MAIN);
    lv_obj_set_style_border_color(g_lock_msg_area, gw_ui_c_text(),
                                  LV_PART_MAIN);
    lv_obj_set_style_radius(g_lock_msg_area, 0, LV_PART_MAIN);

    row = lv_obj_create(modal);
    lv_obj_set_size(row, GW_UI_CONTENT_W, GW_UI_TOUCH_MIN);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 290);
    gw_ui_style_plain_panel(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    {
        lv_obj_t *cancel = gw_ui_kit_button_create(
            row, GW_STR_CANCEL, 280, GW_UI_TOUCH_MIN, GW_UI_BUTTON_SECONDARY);
        lv_obj_t *clear = gw_ui_kit_button_create(
            row, GW_STR_CLEAR, 280, GW_UI_TOUCH_MIN, GW_UI_BUTTON_SECONDARY);
        lv_obj_t *save = gw_ui_kit_button_create(
            row, GW_STR_SAVE, 280, GW_UI_TOUCH_MIN, GW_UI_BUTTON_PRIMARY);
        lv_obj_add_event_cb(cancel, lock_msg_cancel_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(clear, lock_msg_clear_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(save, lock_msg_save_cb, LV_EVENT_CLICKED, NULL);
    }

    keyboard = lv_keyboard_create(modal);
    lv_obj_set_size(keyboard, GW_UI_PANEL_W, 480);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    gw_ui_kit_style_keyboard(keyboard);
    lv_keyboard_set_textarea(keyboard, g_lock_msg_area);
    lv_obj_add_event_cb(keyboard, lock_msg_keyboard_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(keyboard, lock_msg_keyboard_cb, LV_EVENT_CANCEL, NULL);

    g_text_input_active = 1;
    gw_eink_set_text_input(1);
    gw_eink_set_keyboard_rect(1, 0, GW_UI_PANEL_H - 480,
                              GW_UI_PANEL_W - 1, GW_UI_PANEL_H - 1);
    gw_ui_request_full(NULL);
}

GW_SHELL_INTERNAL void build_settings_screen(void) {
    lv_obj_t *content, *row;
    g_settings_screen = make_screen();
    gw_ui_kit_app_bar_create(g_settings_screen, GW_STR_SETTINGS, gw_back_cb);
    content = gw_ui_kit_scroll_content_create(g_settings_screen,
                                              GW_UI_APP_BAR_H + GW_UI_GAP,
                                              GW_UI_GAP);
    g_settings_content = content;
    lv_obj_set_style_pad_bottom(content, GW_UI_SPACE_6 + GW_UI_SPACE_1,
                                LV_PART_MAIN);
    lv_obj_set_style_pad_right(content, GW_UI_SPACE_3, LV_PART_MAIN);
    lv_obj_set_style_width(content, GW_UI_SETTINGS_SCROLLBAR_W,
                            LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(content, gw_ui_c_border(), LV_PART_SCROLLBAR);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_ON);
    attach_scroll_events(content);

    settings_section(content, GW_STR_DISPLAY);
    row = lv_obj_create(content);
    g_settings_light_row = row;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, GW_UI_SETTINGS_ROW_H);
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
        /* D08: real slider with a visible knob; value is committed on
         * release, not on every drag step. */
        g_light_bar = lv_slider_create(track);
        lv_obj_set_size(g_light_bar, 190, 40);
        lv_obj_set_ext_click_area(g_light_bar, 28);
        lv_obj_center(g_light_bar);
        lv_slider_set_range(g_light_bar, 0, 100);
        lv_slider_set_value(g_light_bar, g_light_value, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(g_light_bar, gw_ui_c_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_light_bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_light_bar, GW_UI_BORDER_W, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_light_bar, gw_ui_c_text(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_light_bar, gw_ui_c_text(), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(g_light_bar, gw_ui_c_white(), LV_PART_KNOB);
        lv_obj_set_style_border_width(g_light_bar, GW_UI_BORDER_W, LV_PART_KNOB);
        lv_obj_set_style_border_color(g_light_bar, gw_ui_c_text(), LV_PART_KNOB);
        lv_obj_set_style_pad_all(g_light_bar, 8, LV_PART_KNOB);
        lv_obj_set_style_radius(g_light_bar, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(g_light_bar, 2, LV_PART_INDICATOR);
        lv_obj_set_style_radius(g_light_bar, 0, LV_PART_KNOB);
        lv_obj_add_event_cb(g_light_bar, light_slider_pressed_cb,
                            LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_light_bar, light_slider_released_cb,
                            LV_EVENT_RELEASED, NULL);
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
        light_apply_ui();
    }

    settings_section(content, GW_STR_DEVICE);
    settings_row(content, GW_STR_DEVICE_NAME, GW_STR_DEVICE_NAME_SUB, NULL,
                 settings_device_name_cb, &g_settings_device_name_label);

    settings_section(content, GW_STR_TIME_DATE_SECTION);
    settings_row(content, GW_STR_TIME_ZONE, GW_STR_TIME_ZONE_SUB, NULL,
                 settings_timezone_edit_cb, &g_settings_timezone_label);
    settings_row(content, GW_STR_DATE_TIME, GW_STR_DATE_TIME_SUB, NULL,
                 settings_time_edit_cb, &g_settings_time_label);
    settings_row(content, GW_STR_SYNC_NOW, GW_STR_SYNC_NOW_SUB, NULL,
                 settings_sync_now_cb, NULL);

    settings_section(content, GW_STR_LOCK_SCREEN);
    {
        gw_lock_config_t cfg;
        if (lock_config_read(&cfg) < 0) gw_lock_config_defaults(&cfg);
        settings_toggle_row(content, GW_STR_LOCK_INFO_CARD,
                            GW_STR_LOCK_INFO_CARD_SUB, cfg.show_panel,
                            lock_panel_cb);
        settings_toggle_row(content, GW_STR_LOCK_CLOCK, NULL, cfg.show_clock,
                            lock_clock_cb);
        settings_toggle_row(content, GW_STR_DATE, NULL, cfg.show_date,
                            lock_date_cb);
        settings_row(content, GW_STR_LOCK_MESSAGE, GW_STR_LOCK_MESSAGE_SUB,
                     NULL, lock_message_edit_cb, NULL);
    }

    settings_section(content, GW_STR_CONNECTIVITY);
    settings_row(content, GW_STR_WIFI, GW_STR_NETWORKS_CONNECTION, NULL,
                 network_open_cb, &g_settings_wifi_label);

    settings_section(content, GW_STR_ABOUT_DEVICE);
    g_settings_battery_row = settings_info_row(content, GW_STR_BATTERY,
                                               GW_UI_SETTINGS_INFO_ROW_H,
                                               &g_settings_battery_label);
    g_settings_storage_row = settings_info_row(content, GW_STR_STORAGE,
                                               GW_UI_SETTINGS_INFO_ROW_H,
                                               &g_settings_storage_label);
    g_settings_uptime_row = settings_info_row(content, GW_STR_UPTIME,
                                               GW_UI_SETTINGS_INFO_ROW_H,
                                              &g_settings_uptime_label);
    settings_info_row(content, GW_STR_VERSION,
                                               GW_UI_SETTINGS_INFO_ROW_H, &g_settings_version_label);
    settings_info_row(content, GW_STR_DATE,
                                               GW_UI_SETTINGS_INFO_ROW_H, &g_settings_date_label);

    settings_section(content, GW_STR_ACTIONS);
    {
        lv_obj_t *reboot_button = make_button(content, GW_STR_REBOOT,
                                              GW_UI_CONTENT_W, GW_UI_TOUCH_MIN,
                                              1);
        lv_obj_add_event_cb(reboot_button, reboot_cb, LV_EVENT_CLICKED, NULL);
    }

    settings_facts_update();
}


/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host-test surface for gw-shell.c.  The shell is compiled as its own
 * translation unit; this header declares the internals the geometry suite
 * exercises.  It replaces the old "#include \"gw-shell.c\"" seam. */
#ifndef GW_SHELL_TEST_H
#define GW_SHELL_TEST_H

#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <lvgl/lvgl.h>
#include "lv_port_epdc.h"
#include "lv_port_input.h"
#include "gw_ui_style.h"
#include "gw_ui_strings.h"
#include "gw_ui_kit.h"
#include "gw_shell_internal.h"
#include "gw_fonts.h"
#include "gw_icons.h"
#include "gw_shell_types.h"
#include "gw_shell_state.h"
#include "../system/gw-ipc.h"

extern void gw_shell_apply_profile(gw_screen_t screen);

extern const char * battery_icon(int percent, int charging);
extern const char * gw_screen_name(gw_screen_t screen);
extern gw_todo_t * todo_find(int id);
extern int apps_fetch(gw_app_entry_t *out, int max);
extern int apps_slot_for(const char *id);
extern int days_in_month(int year, int month);
extern int gw_read_trimmed(const char *path, char *buffer, size_t size);
extern int hex_decode(const char *hex, char *out, size_t size);
extern int ipc_cmd(const char *cmd, char *resp, size_t resp_size);
extern int net_result_parse(const char *response, gw_net_ap_t *ap);
extern int net_status_fetch(void);
extern int signal_bars_for(int dbm);
extern int todo_add(const char *text, const char *due);
extern int todo_commit(void);
extern int todo_due_on(int year, int month, int day);
extern int todo_parse_line(const char *line, gw_todo_t *out);
extern int todo_store_save(void);
extern lv_obj_t * gw_diag_find_scroll(lv_obj_t *obj);
extern lv_obj_t * home_card(lv_obj_t *parent, lv_event_cb_t cb);
extern lv_obj_t * make_button(lv_obj_t *parent, const char *text, int width, int height, int secondary);
extern lv_obj_t * make_content(lv_obj_t *scr, int top);
extern lv_obj_t * make_hairline(lv_obj_t *parent, int width);
extern lv_obj_t * make_screen(void);
extern lv_obj_t * settings_info_row(lv_obj_t *parent, const char *title, int height, lv_obj_t **value_out);
extern lv_obj_t * settings_row(lv_obj_t *parent, const char *title, const char *subtitle, const char *button_text, lv_event_cb_t cb, lv_obj_t **value_out);
extern lv_obj_t * settings_section(lv_obj_t *parent, const char *text);
extern lv_obj_t * status_target_create(lv_obj_t *bar, const char *icon, int x, int width, lv_event_cb_t cb);
extern uint32_t gw_waveform_from_name(const char *value);
extern uint32_t lv_tick_get_cb(void);
extern void apply_model(void);
extern void apps_apply_entries(gw_app_entry_t *entries, int count);
extern void apps_delete_slot(int slot);
extern void apps_make_row(int index, const gw_app_entry_t *entry);
extern void apps_message_set(const char *text);
extern void apps_open_cb(lv_event_t *event);
extern void apps_rebuild_list(void);
extern void apps_refresh(void);
extern void apps_refresh_cb(lv_event_t *event);
extern void apps_remove_action(void *user_data);
extern void apps_row_cb(lv_event_t *event);
extern void attach_scroll_events(lv_obj_t *obj);
extern void build_all_screens(void);
extern void build_apps_screen(void);
extern void build_calendar_screen(void);
extern void build_calendar_widget(lv_obj_t *parent);
extern void build_continue_card(lv_obj_t *parent);
extern void build_home_screen(void);
extern void build_lock_screen(void);
extern void build_network_screen(void);
extern void build_notes_screen(void);
extern void build_notes_widget(lv_obj_t *parent);
extern void build_settings_screen(void);
extern void build_status_bar(lv_obj_t *scr);
extern void calendar_day_cb(lv_event_t *event);
extern void calendar_detail_ensure(int index);
extern void calendar_detail_render(void);
extern void calendar_month_shift(int delta);
extern void calendar_next_cb(lv_event_t *event);
extern void calendar_open_cb(lv_event_t *event);
extern void calendar_open_todos_cb(lv_event_t *event);
extern void calendar_prev_cb(lv_event_t *event);
extern void calendar_render(void);
extern void calendar_select_day(int day);
extern void calendar_today_cb(lv_event_t *event);
extern void container_set_hidden(lv_obj_t *obj, int hidden);
extern void crash_handler(int sig, siginfo_t *info, void *ctx);
extern void evt_close(void);
extern void evt_connect(void);
extern void evt_handle_line(const char *line);
extern void evt_poll(void);
extern void format_due_label(const char *iso, char *buf, size_t size);
extern void format_hero_date(char *buf, size_t size);
extern void format_iso_date(int year, int month, int day, char *buf, size_t size);
extern void format_month_title(int year, int month, char *buf, size_t size);
extern void format_size(double bytes, char *buf, size_t size);
extern void format_storage_value(double free_bytes, double total_bytes, char *buf, size_t size);
extern void gw_back_cb(lv_event_t *event);
extern void gw_diag_handle(const char *cmd);
extern void gw_diag_init(void);
extern void gw_diag_poll(void);
extern void gw_open_secondary(gw_screen_t screen, gw_screen_t back);
extern void gw_paint_policy_load(void);
extern void gw_scene_show(gw_screen_t screen, int deep_clean);
extern void gw_ui_build(void);
extern void gw_ui_now(struct tm *out);
extern void gw_ui_apply_timezone(const char *tz);
extern void gw_ui_request_full(void *user_data);
extern void gw_ui_show(gw_screen_t screen);
extern void home_empty_ensure(void);
extern void home_todo_ensure_row(int index);
extern void home_todo_remove_row(int index);
extern void host_ipc_fail_next(const char *prefix);
extern void host_ipc_app_list_set(const char *response);
extern void install_crash_handler(void);
extern void iso_relative_days(int days, char *buf, size_t size);
extern void keyboard_dismiss(void);
extern void label_set_if_changed(lv_obj_t *label, const char *text);
extern void light_apply_ui(void);
extern void light_minus_cb(lv_event_t *event);
extern void light_open_cb(lv_event_t *event);
extern void light_plus_cb(lv_event_t *event);
extern void light_set_abs(int value);
extern void light_slider_pressed_cb(lv_event_t *event);
extern void light_slider_released_cb(lv_event_t *event);
extern void light_step(int delta);
extern void lock_apply_status(const char *status);
extern void lock_refresh_time(void);
extern void lock_screen_cb(lv_event_t *event);
extern void reboot_cb(lv_event_t *event);
extern void lock_sync_now(void);
extern void model_load(void);
extern void nav_settings_cb(lv_event_t *event);
extern void network_build_rows(void);
extern void network_delete_slot(int slot);
extern void network_disconnect_action(void *user_data);
extern void network_forget_action(void *user_data);
extern void network_make_row(int slot, const gw_net_ap_t *ap);
extern void network_open_cb(lv_event_t *event);
extern void network_row_update(int slot, const gw_net_ap_t *ap);
extern void network_scan_apply(void);
extern void network_select_cb(lv_event_t *event);
extern void network_update_ui(void);
extern void notes_focus_event(lv_event_t *event);
extern void notes_keyboard_event(lv_event_t *event);
extern void notes_open_cb(lv_event_t *event);
extern void app_launch_id(const char *id);
extern void reader_launch(lv_event_t *event);
extern void recovery_set(const char *text);
extern void scroll_activity_cb(lv_event_t *event);
extern void set_clickable(lv_obj_t *obj);
extern void settings_facts_update(void);
extern void settings_storage_value(char *buf, size_t size);
extern void settings_uptime_value(char *buf, size_t size);
extern void status_apply(const char *status);
extern void status_refresh_now(void);
extern void status_update(lv_timer_t *timer);
extern void status_wifi_bars(int level);
extern void strip_newline(char *s);
extern void today_iso(char *buf, size_t size);
extern void todo_add_cb(lv_event_t *event);
extern void todo_add_from_input(void);
extern void todo_check_cb(lv_event_t *event);
extern void todo_copy_due(char *dst, size_t dst_size, const char *src);
extern void todo_delete(int id);
extern void todo_delete_button(lv_obj_t *row, int id);
extern void todo_delete_cb(lv_event_t *event);
extern void todo_due_cb(lv_event_t *event);
extern void todo_due_style(void);
extern void todo_filter_cb(lv_event_t *event);
extern void todo_filter_style(void);
extern void todo_rebuild_list(void);
extern void todo_restore(int id);
extern void todo_restore_action(void *user_data);
extern void todo_restore_cb(lv_event_t *event);
extern void todo_row_make(int id, int history);
extern void todo_row_update(int slot, const gw_todo_t *t, int history);
extern void todo_sanitize(char *dst, size_t dst_size, const char *src);
extern void todo_store_load(void);
extern void todo_toggle(int id);
extern void ui_tick_cb(lv_timer_t *timer);
extern void unlock_screen_cb(lv_event_t *event);
extern void update_install_cb(lv_event_t *event);
extern void update_package_cb(lv_event_t *event);
extern void update_scan(void);
extern void update_scan_cb(lv_event_t *event);
extern void update_selection_refresh(void);
extern void wifi_connect(lv_event_t *event);
extern void wifi_disconnect(lv_event_t *event);
extern void wifi_eye_cb(lv_event_t *event);
extern void wifi_focus(lv_event_t *event);
extern void wifi_forget(lv_event_t *event);
extern void wifi_keyboard_event(lv_event_t *event);
extern void wifi_scan(lv_event_t *event);

#endif /* GW_SHELL_TEST_H */

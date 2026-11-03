/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cross-translation-unit declarations for the gw-shell UI.
 *
 * Each screen owns one .c file.  The shell context lives in
 * gw_shell_state.c; these declarations are the only cross-file interface.
 */
#ifndef GW_SHELL_INTERNAL_H
#define GW_SHELL_INTERNAL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <sys/resource.h>
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
#include "gw_paint_profile.h"
#include "gw_paint_rules.h"
#include "gw_shell_state.h"
#include "../system/gw-ipc.h"
#include "../system/gw-file.h"
#include "../system/gw-ipc-client.h"

/* Functions cross translation units; production no longer hides them behind
 * file-local static linkage. */
#define GW_SHELL_INTERNAL

uint32_t gw_waveform_from_name (const char *value);;
int gw_read_trimmed (const char *path, char *buffer, size_t size);;
void gw_shell_apply_profile (gw_screen_t screen);;
void gw_paint_policy_load (void);;
void host_ipc_fail_next (const char *prefix);;
int ipc_cmd (const char *cmd, char *resp, size_t resp_size);;
int ipc_cmd_all (const char *cmd, char *resp, size_t resp_size);;
void evt_handle_line (const char *line);;
void evt_close (void);;
void evt_connect (void);;
void evt_poll (void);;
uint32_t lv_tick_get_cb (void);;
void label_set_if_changed (lv_obj_t *label, const char *text);;
void container_set_hidden (lv_obj_t *obj, int hidden);;
const char *gw_screen_name (gw_screen_t screen);;
void gw_ui_now (struct tm *out);;
void format_hero_date (char *buf, size_t size);;
void format_month_title (int year, int month, char *buf, size_t size);;
void format_iso_date (int year, int month, int day, char *buf, size_t size);;
void today_iso (char *buf, size_t size);;
void iso_relative_days (int days, char *buf, size_t size);;
void format_due_label (const char *iso, char *buf, size_t size);;
int days_in_month (int year, int month);;
void todo_sanitize (char *dst, size_t dst_size, const char *src);;
void todo_copy_due (char *dst, size_t dst_size, const char *src);;
int todo_store_save (void);;
int todo_parse_line (const char *line, gw_todo_t *out);;
void todo_store_load (void);;
gw_todo_t *todo_find (int id);;
int todo_due_on (int year, int month, int day);;
int todo_add (const char *text, const char *due);;
void todo_toggle (int id);;
void todo_delete (int id);;
void todo_restore (int id);;
int todo_commit (void);;
void lock_apply_status (const char *status);;
void lock_sync_now (void);;
int hex_decode (const char *hex, char *out, size_t size);;
int net_result_parse (const char *response, gw_net_ap_t *ap);;
int signal_bars_for (int dbm);;
const char *battery_icon (int percent, int charging);;
void status_wifi_bars (int level);;
int net_status_fetch (void);;
void network_scan_apply (void);;
void status_apply (const char *status);;
void status_refresh_now (void);;
void status_update (lv_timer_t *timer);;
void ui_tick_cb (lv_timer_t *timer);;
void reader_launch (lv_event_t *event);;
void lock_screen_cb (lv_event_t *event);;
void unlock_screen_cb (lv_event_t *event);;
void light_apply_ui (void);;
void light_step (int delta);;
void light_minus_cb (lv_event_t *event);;
void light_plus_cb (lv_event_t *event);;
void light_set_abs (int value);;
void light_slider_pressed_cb (lv_event_t *event);;
void light_slider_released_cb (lv_event_t *event);;
void light_open_cb (lv_event_t *event);;
void nav_home_cb (lv_event_t *event);;
void nav_library_cb (lv_event_t *event);;
void nav_calendar_cb (lv_event_t *event);;
void nav_todos_cb (lv_event_t *event);;
void nav_settings_cb (lv_event_t *event);;
void calendar_open_cb (lv_event_t *event);;
void notes_open_cb (lv_event_t *event);;
void gw_open_secondary (gw_screen_t screen, gw_screen_t back);;
void gw_back_cb (lv_event_t *event);;
void network_open_cb (lv_event_t *event);;
void apps_open_cb (lv_event_t *event);;
lv_obj_t *make_screen (void);;
lv_obj_t *make_hairline (lv_obj_t *parent, int width);;
lv_obj_t *make_button (lv_obj_t *parent, const char *text, int width, int height, int secondary);;
lv_obj_t *make_bottom_nav (lv_obj_t *scr, int active);;
lv_obj_t *make_content (lv_obj_t *scr, int top);;
void gw_ui_request_full (void *user_data);;
void scroll_activity_cb (lv_event_t *event);;
void attach_scroll_events (lv_obj_t *obj);;
void set_clickable (lv_obj_t *obj);;
lv_obj_t *status_target_create (lv_obj_t *bar, const char *icon, int x, int width, lv_event_cb_t cb);;
void build_status_bar (lv_obj_t *scr);;
lv_obj_t *home_card (lv_obj_t *parent, lv_event_cb_t cb);;
void build_continue_card (lv_obj_t *parent);;
void build_calendar_widget (lv_obj_t *parent);;
void build_notes_widget (lv_obj_t *parent);;
void build_app_launcher (lv_obj_t *parent);;
lv_obj_t *home_app_tile (lv_obj_t *parent, const char *icon, const char *title, lv_event_cb_t cb);;
void home_hub_open_cb (lv_event_t *event);;
void build_home_screen (void);;
void recovery_set (const char *text);;
lv_obj_t *settings_section (lv_obj_t *parent, const char *text);;
lv_obj_t *settings_row (lv_obj_t *parent, const char *title, const char *subtitle, const char *button_text, lv_event_cb_t cb, lv_obj_t **value_out);;
lv_obj_t *settings_info_row (lv_obj_t *parent, const char *title, int height, lv_obj_t **value_out);;
void format_size (double bytes, char *buf, size_t size);;
void format_storage_value (double free_bytes, double total_bytes, char *buf, size_t size);;
void settings_storage_value (char *buf, size_t size);;
void settings_uptime_value (char *buf, size_t size);;
void settings_facts_update (void);;
void build_settings_screen (void);;
void calendar_select_day (int day);;
void calendar_day_cb (lv_event_t *event);;
void calendar_month_shift (int delta);;
void calendar_prev_cb (lv_event_t *event);;
void calendar_next_cb (lv_event_t *event);;
void calendar_today_cb (lv_event_t *event);;
void calendar_open_todos_cb (lv_event_t *event);;
void calendar_render (void);;
void calendar_detail_ensure (int index);;
void calendar_detail_render (void);;
void build_calendar_screen (void);;
void todo_due_style (void);;
void todo_due_cb (lv_event_t *event);;
void todo_filter_style (void);;
void todo_add_from_input (void);;
void todo_add_cb (lv_event_t *event);;
void todo_check_cb (lv_event_t *event);;
void todo_restore_action (void *user_data);;
void todo_delete_cb (lv_event_t *event);;
void todo_restore_cb (lv_event_t *event);;
void todo_filter_cb (lv_event_t *event);;
void todo_delete_button (lv_obj_t *row, int id);;
void todo_row_make (int id, int history);;
void todo_row_update (int slot, const gw_todo_t *t, int history);;
void todo_rebuild_list (void);;
void keyboard_dismiss (void);;
void notes_keyboard_event (lv_event_t *event);;
void notes_focus_event (lv_event_t *event);;
void build_notes_screen (void);;
void network_row_update (int slot, const gw_net_ap_t *ap);;
void network_make_row (int slot, const gw_net_ap_t *ap);;
void network_delete_slot (int slot);;
void network_build_rows (void);;
void network_select_cb (lv_event_t *event);;
void network_update_ui (void);;
void wifi_scan (lv_event_t *event);;
void wifi_connect (lv_event_t *event);;
void network_disconnect_action (void *user_data);;
void network_forget_action (void *user_data);;
void wifi_disconnect (lv_event_t *event);;
void wifi_forget (lv_event_t *event);;
void wifi_eye_cb (lv_event_t *event);;
void wifi_focus (lv_event_t *event);;
void wifi_keyboard_event (lv_event_t *event);;
void build_network_screen (void);;
void strip_newline (char *s);;
int apps_fetch (gw_app_entry_t *out, int max);;
void apps_make_row (int index, const gw_app_entry_t *entry);;
int apps_slot_for (const char *id);;
void apps_delete_slot (int slot);;
void apps_apply_entries (gw_app_entry_t *entries, int count);;
void apps_message_set (const char *text);;
void apps_rebuild_list (void);;
void apps_remove_action (void *user_data);;
void apps_row_cb (lv_event_t *event);;
void apps_refresh (void);;
void apps_refresh_cb (lv_event_t *event);;
void update_selection_refresh (void);;
void update_package_cb (lv_event_t *event);;
void update_scan (void);;
void update_scan_cb (lv_event_t *event);;
void update_install_cb (lv_event_t *event);;
void build_apps_screen (void);;
void build_update_screen (void);;
void build_hub_screen (void);;
void hub_open_cb (lv_event_t *event);;
void hub_touch_cb (lv_event_t *event);;
void hub_exit_cb (lv_event_t *event);;
void gw_hub_init (void);;
void gw_hub_attach (lv_obj_t *img, lv_obj_t *status);;
void gw_hub_set_view (int active);;
void gw_hub_send_touch (const char *action, int x, int y);;
void gw_hub_send_key (const char *key);;
int gw_hub_has_frame (void);;
void lock_refresh_time (void);;
void lock_config_load (void);;
void lock_config_poll (void);;
int lock_config_read (gw_lock_config_t *cfg);;
int lock_config_write (const gw_lock_config_t *cfg);;
void lock_message_modal_destroy (void);;
void build_lock_screen (void);;
void home_todo_ensure_row (int index);;
void home_todo_remove_row (int index);;
void home_empty_ensure (void);;
void apply_model (void);;
void model_load (void);;
void build_all_screens (void);;
void gw_scene_show (gw_screen_t screen, int deep_clean);;
void gw_ui_show (gw_screen_t screen);;
void gw_ui_build (void);;
void crash_handler (int sig, siginfo_t *info, void *ctx);;
void install_crash_handler (void);;

#endif /* GW_SHELL_INTERNAL_H */

/* SPDX-License-Identifier: GPL-3.0-or-later
 * One shell context object.  All mutable module state lives here instead of
 * as separate file-scope globals; the macros keep the existing screen code
 * readable while there is exactly one owner of the state. */
#ifndef GW_SHELL_STATE_H
#define GW_SHELL_STATE_H

#include <stdint.h>
#include "../system/gw-ipc.h"
#include "gw_shell_types.h"

typedef struct {
    uint32_t g_scene_waveform;
    gw_todo_t g_todos[GW_TODO_MAX];
    int g_todo_count;
    int g_todo_next_id;
    int g_todo_history;
    lv_obj_t * g_todo_list;
    lv_obj_t * g_todo_empty;
    lv_obj_t * g_todo_filter_active;
    lv_obj_t * g_todo_filter_history;
    gw_todo_row_t g_todo_rows[GW_TODO_MAX];
    int g_todo_row_count;
    gw_ui_model_t g_model;
    int g_exit_for_reader;
    int g_active_screen;
    gw_screen_t g_back_screen;
    unsigned g_deep_clean_requests;
    lv_obj_t * g_home_screen;
    lv_obj_t * g_settings_screen;
    lv_obj_t * g_calendar_screen;
    lv_obj_t * g_notes_screen;
    lv_obj_t * g_network_screen;
    lv_obj_t * g_apps_screen;
    lv_obj_t * g_update_screen;
    lv_obj_t * g_hub_screen;
    lv_obj_t * g_hub_img;
    lv_obj_t * g_hub_status;
    lv_obj_t * g_hub_exit;
    lv_obj_t * g_lock_screen;
    lv_obj_t * g_lock_image;
    lv_obj_t * g_lock_panel;
    lv_obj_t * g_lock_message_label;
    int g_lock_show_panel;
    int g_lock_show_clock;
    int g_lock_show_date;
    gw_app_entry_t g_app_entries[GW_APP_MAX];
    int g_app_count;
    lv_obj_t * g_apps_list;
    lv_obj_t * g_apps_action_row;
    lv_obj_t * g_apps_refresh_button;
    lv_obj_t * g_apps_message;
    lv_obj_t * g_apps_empty_label;
    lv_obj_t * g_app_rows[GW_APP_MAX];
    char * g_app_row_ids[GW_APP_MAX];
    lv_obj_t * g_app_name_labels[GW_APP_MAX];
    lv_obj_t * g_app_version_labels[GW_APP_MAX];
    lv_obj_t * g_app_desc_labels[GW_APP_MAX];
    unsigned g_apps_full_rebuilds;
    unsigned g_apps_row_updates;
    lv_obj_t * g_update_list;
    lv_obj_t * g_update_status;
    lv_obj_t * g_update_scan_button;
    lv_obj_t * g_update_install_button;
    lv_obj_t * g_update_rows[6];
    char g_update_packages[6][128];
    int g_update_package_count;
    int g_update_selected;
    lv_obj_t * g_clock_label;
    lv_obj_t * g_usb_label;
    lv_obj_t * g_wifi_bars;
    lv_obj_t * g_light_status_label;
    lv_obj_t * g_battery_label;
    lv_obj_t * g_date_label;
    lv_obj_t * g_wifi_button;
    lv_obj_t * g_light_button;
    lv_obj_t * g_lock_button;
    lv_obj_t * g_battery_button;
    lv_obj_t * g_home_reading_card;
    lv_obj_t * g_home_calendar_card;
    lv_obj_t * g_home_notes_card;
    lv_obj_t * g_home_apps;
    lv_obj_t * g_reading_header;
    lv_obj_t * g_reading_body;
    lv_obj_t * g_reading_empty;
    lv_obj_t * g_book_title_label;
    lv_obj_t * g_book_author_label;
    lv_obj_t * g_book_progress_label;
    lv_obj_t * g_book_bar;
    lv_obj_t * g_widget_day_label;
    lv_obj_t * g_widget_month_label;
    lv_obj_t * g_widget_weekday_label;
    lv_obj_t * g_widget_week_labels[7];
    lv_obj_t * g_widget_event_rows[2];
    lv_obj_t * g_widget_event_labels[2];
    lv_obj_t * g_widget_todo_count;
    lv_obj_t * g_widget_todo_rows[5];
    lv_obj_t * g_settings_battery_label;
    lv_obj_t * g_settings_storage_label;
    lv_obj_t * g_settings_version_label;
    lv_obj_t * g_settings_uptime_label;
    lv_obj_t * g_settings_date_label;
    lv_obj_t * g_settings_content;
    lv_obj_t * g_settings_wifi_label;
    lv_obj_t * g_settings_battery_row;
    lv_obj_t * g_settings_storage_row;
    lv_obj_t * g_settings_uptime_row;
    lv_obj_t * g_light_label;
    lv_obj_t * g_light_bar;
    lv_obj_t * g_settings_light_row;
    int g_light_last;
    int g_light_value;
    lv_obj_t * g_calendar_title_label;
    lv_obj_t * g_calendar_cells[GW_CAL_CELLS];
    lv_obj_t * g_calendar_day_labels[GW_CAL_CELLS];
    lv_obj_t * g_calendar_weekday_labels[7];
    lv_obj_t * g_calendar_dots[GW_CAL_CELLS];
    lv_obj_t * g_calendar_detail_title;
    lv_obj_t * g_calendar_detail_rows[5];
    lv_obj_t * g_calendar_detail_empty;
    lv_obj_t * g_calendar_detail_more;
    int g_calendar_year;
    int g_calendar_month;
    int g_calendar_selected_day;
    lv_obj_t * g_notes_area;
    lv_obj_t * g_notes_status;
    lv_obj_t * g_notes_keyboard;
    lv_obj_t * g_todo_due_buttons[5];
    int g_todo_due_index;
    gw_net_ap_t g_net_aps[GW_NET_MAX];
    int g_net_ap_count;
    char g_net_selected_ssid[65];
    char g_net_ssid[65];
    char g_net_ip[32];
    int g_net_state;
    int g_net_signal;
    lv_obj_t * g_net_card;
    lv_obj_t * g_net_connection_label;
    lv_obj_t * g_net_signal_bars;
    lv_obj_t * g_net_section_row;
    lv_obj_t * g_net_list;
    lv_obj_t * g_net_rows[GW_NET_MAX];
    int g_net_row_count;
    char g_net_row_ssid[GW_NET_MAX][65];
    /* Owned SSID strings used as each row's user data.  Pointers move with
     * the row; they are never re-pointed at another network. */
    char * g_net_row_ids[GW_NET_MAX];
    lv_obj_t * g_net_ssid_labels[GW_NET_MAX];
    lv_obj_t * g_net_dbm_labels[GW_NET_MAX];
    lv_obj_t * g_net_lock_labels[GW_NET_MAX];
    lv_obj_t * g_net_check_labels[GW_NET_MAX];
    lv_obj_t * g_net_signal_widgets[GW_NET_MAX];
    lv_obj_t * g_net_empty_label;
    lv_obj_t * g_net_password_row;
    lv_obj_t * g_net_password;
    lv_obj_t * g_net_eye_button;
    lv_obj_t * g_net_message;
    lv_obj_t * g_net_connect_button;
    lv_obj_t * g_net_scan_button;
    lv_obj_t * g_net_disconnect_button;
    lv_obj_t * g_net_forget_button;
    lv_obj_t * g_net_keyboard;
    int g_net_password_visible;
    int g_wifi_scan_pending;
    uint32_t g_wifi_scan_started_ms;
    uint32_t g_net_poll_deadline_ms;
    uint32_t g_net_poll_next_ms;
    int g_text_input_active;
    lv_obj_t * g_lock_label;
    lv_obj_t * g_lock_time_label;
    lv_obj_t * g_lock_date_label;
    unsigned g_full_paint_requests;
    int g_evt_fd;
    char g_evt_line[GW_IPC_MAX_LINE];
    size_t g_evt_used;
    uint32_t g_evt_retry_ms;
    uint32_t g_ignore_lock_until_ms;
    unsigned g_ui_text_updates;
    int g_todo_last_deleted;
    int g_diag_fd;
    const char * g_host_ipc_fail_prefix;
    char g_host_last_ipc[128];
} gw_shell_state_t;

#ifdef GW_SHELL_STATE_IMPLEMENTATION
gw_shell_state_t g_shell = {
    .g_scene_waveform = WAVEFORM_MODE_GC16_FAST,
    .g_todo_next_id = 1,
    .g_active_screen = GW_SCREEN_HOME,
    .g_back_screen = GW_SCREEN_HOME,
    .g_update_selected = -1,
    .g_light_last = -1,
    .g_light_value = 37,
    .g_net_state = -1,
    .g_evt_fd = -1,
    .g_todo_last_deleted = -1,
    .g_diag_fd = -1
};
#endif

extern gw_shell_state_t g_shell;

#define g_scene_waveform (g_shell.g_scene_waveform)
#define g_todos (g_shell.g_todos)
#define g_todo_count (g_shell.g_todo_count)
#define g_todo_next_id (g_shell.g_todo_next_id)
#define g_todo_history (g_shell.g_todo_history)
#define g_todo_list (g_shell.g_todo_list)
#define g_todo_empty (g_shell.g_todo_empty)
#define g_todo_filter_active (g_shell.g_todo_filter_active)
#define g_todo_filter_history (g_shell.g_todo_filter_history)
#define g_todo_rows (g_shell.g_todo_rows)
#define g_todo_row_count (g_shell.g_todo_row_count)
#define g_model (g_shell.g_model)
#define g_exit_for_reader (g_shell.g_exit_for_reader)
#define g_active_screen (g_shell.g_active_screen)
#define g_back_screen (g_shell.g_back_screen)
#define g_deep_clean_requests (g_shell.g_deep_clean_requests)
#define g_home_screen (g_shell.g_home_screen)
#define g_settings_screen (g_shell.g_settings_screen)
#define g_calendar_screen (g_shell.g_calendar_screen)
#define g_notes_screen (g_shell.g_notes_screen)
#define g_network_screen (g_shell.g_network_screen)
#define g_apps_screen (g_shell.g_apps_screen)
#define g_update_screen (g_shell.g_update_screen)
#define g_hub_screen (g_shell.g_hub_screen)
#define g_hub_img (g_shell.g_hub_img)
#define g_hub_status (g_shell.g_hub_status)
#define g_hub_exit (g_shell.g_hub_exit)
#define g_lock_screen (g_shell.g_lock_screen)
#define g_lock_image (g_shell.g_lock_image)
#define g_lock_panel (g_shell.g_lock_panel)
#define g_lock_message_label (g_shell.g_lock_message_label)
#define g_lock_show_panel (g_shell.g_lock_show_panel)
#define g_lock_show_clock (g_shell.g_lock_show_clock)
#define g_lock_show_date (g_shell.g_lock_show_date)
#define g_app_entries (g_shell.g_app_entries)
#define g_app_count (g_shell.g_app_count)
#define g_apps_list (g_shell.g_apps_list)
#define g_apps_action_row (g_shell.g_apps_action_row)
#define g_apps_refresh_button (g_shell.g_apps_refresh_button)
#define g_apps_message (g_shell.g_apps_message)
#define g_apps_empty_label (g_shell.g_apps_empty_label)
#define g_app_rows (g_shell.g_app_rows)
#define g_app_row_ids (g_shell.g_app_row_ids)
#define g_app_name_labels (g_shell.g_app_name_labels)
#define g_app_version_labels (g_shell.g_app_version_labels)
#define g_app_desc_labels (g_shell.g_app_desc_labels)
#define g_apps_full_rebuilds (g_shell.g_apps_full_rebuilds)
#define g_apps_row_updates (g_shell.g_apps_row_updates)
#define g_update_list (g_shell.g_update_list)
#define g_update_status (g_shell.g_update_status)
#define g_update_scan_button (g_shell.g_update_scan_button)
#define g_update_install_button (g_shell.g_update_install_button)
#define g_update_rows (g_shell.g_update_rows)
#define g_update_packages (g_shell.g_update_packages)
#define g_update_package_count (g_shell.g_update_package_count)
#define g_update_selected (g_shell.g_update_selected)
#define g_clock_label (g_shell.g_clock_label)
#define g_usb_label (g_shell.g_usb_label)
#define g_wifi_bars (g_shell.g_wifi_bars)
#define g_light_status_label (g_shell.g_light_status_label)
#define g_battery_label (g_shell.g_battery_label)
#define g_date_label (g_shell.g_date_label)
#define g_wifi_button (g_shell.g_wifi_button)
#define g_light_button (g_shell.g_light_button)
#define g_lock_button (g_shell.g_lock_button)
#define g_battery_button (g_shell.g_battery_button)
#define g_home_reading_card (g_shell.g_home_reading_card)
#define g_home_calendar_card (g_shell.g_home_calendar_card)
#define g_home_notes_card (g_shell.g_home_notes_card)
#define g_home_apps (g_shell.g_home_apps)
#define g_reading_header (g_shell.g_reading_header)
#define g_reading_body (g_shell.g_reading_body)
#define g_reading_empty (g_shell.g_reading_empty)
#define g_book_title_label (g_shell.g_book_title_label)
#define g_book_author_label (g_shell.g_book_author_label)
#define g_book_progress_label (g_shell.g_book_progress_label)
#define g_book_bar (g_shell.g_book_bar)
#define g_widget_day_label (g_shell.g_widget_day_label)
#define g_widget_month_label (g_shell.g_widget_month_label)
#define g_widget_weekday_label (g_shell.g_widget_weekday_label)
#define g_widget_week_labels (g_shell.g_widget_week_labels)
#define g_widget_event_rows (g_shell.g_widget_event_rows)
#define g_widget_event_labels (g_shell.g_widget_event_labels)
#define g_widget_todo_count (g_shell.g_widget_todo_count)
#define g_widget_todo_rows (g_shell.g_widget_todo_rows)
#define g_settings_battery_label (g_shell.g_settings_battery_label)
#define g_settings_storage_label (g_shell.g_settings_storage_label)
#define g_settings_version_label (g_shell.g_settings_version_label)
#define g_settings_uptime_label (g_shell.g_settings_uptime_label)
#define g_settings_date_label (g_shell.g_settings_date_label)
#define g_settings_content (g_shell.g_settings_content)
#define g_settings_wifi_label (g_shell.g_settings_wifi_label)
#define g_settings_battery_row (g_shell.g_settings_battery_row)
#define g_settings_storage_row (g_shell.g_settings_storage_row)
#define g_settings_uptime_row (g_shell.g_settings_uptime_row)
#define g_light_label (g_shell.g_light_label)
#define g_light_bar (g_shell.g_light_bar)
#define g_settings_light_row (g_shell.g_settings_light_row)
#define g_light_last (g_shell.g_light_last)
#define g_light_value (g_shell.g_light_value)
#define g_calendar_title_label (g_shell.g_calendar_title_label)
#define g_calendar_cells (g_shell.g_calendar_cells)
#define g_calendar_day_labels (g_shell.g_calendar_day_labels)
#define g_calendar_weekday_labels (g_shell.g_calendar_weekday_labels)
#define g_calendar_dots (g_shell.g_calendar_dots)
#define g_calendar_detail_title (g_shell.g_calendar_detail_title)
#define g_calendar_detail_rows (g_shell.g_calendar_detail_rows)
#define g_calendar_detail_empty (g_shell.g_calendar_detail_empty)
#define g_calendar_detail_more (g_shell.g_calendar_detail_more)
#define g_calendar_year (g_shell.g_calendar_year)
#define g_calendar_month (g_shell.g_calendar_month)
#define g_calendar_selected_day (g_shell.g_calendar_selected_day)
#define g_notes_area (g_shell.g_notes_area)
#define g_notes_status (g_shell.g_notes_status)
#define g_notes_keyboard (g_shell.g_notes_keyboard)
#define g_todo_due_buttons (g_shell.g_todo_due_buttons)
#define g_todo_due_index (g_shell.g_todo_due_index)
#define g_net_aps (g_shell.g_net_aps)
#define g_net_ap_count (g_shell.g_net_ap_count)
#define g_net_selected_ssid (g_shell.g_net_selected_ssid)
#define g_net_ssid (g_shell.g_net_ssid)
#define g_net_ip (g_shell.g_net_ip)
#define g_net_state (g_shell.g_net_state)
#define g_net_signal (g_shell.g_net_signal)
#define g_net_card (g_shell.g_net_card)
#define g_net_connection_label (g_shell.g_net_connection_label)
#define g_net_signal_bars (g_shell.g_net_signal_bars)
#define g_net_section_row (g_shell.g_net_section_row)
#define g_net_list (g_shell.g_net_list)
#define g_net_rows (g_shell.g_net_rows)
#define g_net_row_count (g_shell.g_net_row_count)
#define g_net_row_ssid (g_shell.g_net_row_ssid)
#define g_net_row_ids (g_shell.g_net_row_ids)
#define g_net_ssid_labels (g_shell.g_net_ssid_labels)
#define g_net_dbm_labels (g_shell.g_net_dbm_labels)
#define g_net_lock_labels (g_shell.g_net_lock_labels)
#define g_net_check_labels (g_shell.g_net_check_labels)
#define g_net_signal_widgets (g_shell.g_net_signal_widgets)
#define g_net_empty_label (g_shell.g_net_empty_label)
#define g_net_password_row (g_shell.g_net_password_row)
#define g_net_password (g_shell.g_net_password)
#define g_net_eye_button (g_shell.g_net_eye_button)
#define g_net_message (g_shell.g_net_message)
#define g_net_connect_button (g_shell.g_net_connect_button)
#define g_net_scan_button (g_shell.g_net_scan_button)
#define g_net_disconnect_button (g_shell.g_net_disconnect_button)
#define g_net_forget_button (g_shell.g_net_forget_button)
#define g_net_keyboard (g_shell.g_net_keyboard)
#define g_net_password_visible (g_shell.g_net_password_visible)
#define g_wifi_scan_pending (g_shell.g_wifi_scan_pending)
#define g_wifi_scan_started_ms (g_shell.g_wifi_scan_started_ms)
#define g_net_poll_deadline_ms (g_shell.g_net_poll_deadline_ms)
#define g_net_poll_next_ms (g_shell.g_net_poll_next_ms)
#define g_text_input_active (g_shell.g_text_input_active)
#define g_lock_label (g_shell.g_lock_label)
#define g_lock_time_label (g_shell.g_lock_time_label)
#define g_lock_date_label (g_shell.g_lock_date_label)
#define g_full_paint_requests (g_shell.g_full_paint_requests)
#define g_evt_fd (g_shell.g_evt_fd)
#define g_evt_line (g_shell.g_evt_line)
#define g_evt_used (g_shell.g_evt_used)
#define g_evt_retry_ms (g_shell.g_evt_retry_ms)
#define g_ignore_lock_until_ms (g_shell.g_ignore_lock_until_ms)
#define g_ui_text_updates (g_shell.g_ui_text_updates)
#define g_todo_last_deleted (g_shell.g_todo_last_deleted)
#define g_diag_fd (g_shell.g_diag_fd)
#define g_host_ipc_fail_prefix (g_shell.g_host_ipc_fail_prefix)
#define g_host_last_ipc (g_shell.g_host_last_ipc)

#endif /* GW_SHELL_STATE_H */

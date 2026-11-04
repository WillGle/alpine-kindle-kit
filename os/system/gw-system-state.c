/* SPDX-License-Identifier: GPL-3.0-or-later
 * Storage for the gw-system shared state. */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#ifndef GW_LIFECYCLE_SPIKE
#define GW_LIFECYCLE_SPIKE 1
#endif

#include "gw-system-state.h"

volatile sig_atomic_t g_quit = 0;
int g_cmd_fd = -1;
int g_evt_fd = -1;
int g_lock_fd = -1;
int g_btn_fd = -1;
int g_sigchld_fd = -1;
int g_sigchld_wr = -1;
int g_light_pct = -1;
int g_battery_pct = -1;
int g_charging = -1;
int g_net_connected = -1;
char g_foreground_app[64] = "";
char g_boot_id[37] = "";
char g_timezone[GW_TIME_TZ_MAX] = "UTC";
char g_device_name[GW_DEVICE_NAME_MAX] = "kindle-pw3";
int g_time_format = GW_TIME_FORMAT_24;
int g_time_sync_pending = 0;
pid_t g_time_sync_pid = -1;
int g_light_target = -1;
int g_wifi_enabled = 0;
gw_wifi_state_t g_wifi_state = GW_WIFI_STATE_OFF;
int g_evt_clients[GW_IPC_MAX_CLIENTS];
int g_evt_clients_ready = 0;

#if GW_LIFECYCLE_SPIKE
int g_supervise = 0;
int g_pending_reader = 0;
char g_pending_app[64];
char g_pending_app_path[256];
time_t g_pending_deadline;
int g_last_reader_status = 0;
int g_shell_failures = 0;
int g_locked = 0;
int g_display_on = 1;
int g_last_light = -1;
int g_button_pressed = 0;
struct timespec g_button_down;
pid_t g_shell_pid = -1;
pid_t g_reader_pid = -1;
pid_t g_wifi_pid = -1;
int g_wifi_error = 0;
int g_suspended = 0;
struct timespec g_sleep_at = { 0, 0 };
int g_usb_state = -2;
int g_charging_state = -2;
struct timespec g_power_poll_at = { 0, 0 };
struct timespec g_wifi_poll_at = { 0, 0 };
struct timespec g_wifi_retry_at = { 0, 0 };
int g_wifi_retry_delay = 5;
#endif

/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared state for the gw-system service.  Extracted so handlers can move
 * into their own translation units without changing storage. */
#ifndef GW_SYSTEM_STATE_H
#define GW_SYSTEM_STATE_H

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <sys/types.h>
#include <time.h>

#include "gw-ipc.h"
#include "gw-network.h"
#include "gw-stage3.h"

extern volatile sig_atomic_t g_quit;
extern int g_cmd_fd;
extern int g_evt_fd;
extern int g_lock_fd;
extern int g_btn_fd;
extern int g_sigchld_fd;
extern int g_sigchld_wr;
extern int g_light_pct;
extern int g_battery_pct;
extern int g_charging;
extern int g_net_connected;
extern char g_foreground_app[64];
extern char g_boot_id[37];
extern char g_timezone[GW_TIME_TZ_MAX];
extern int g_time_format;
extern int g_time_sync_pending;
extern pid_t g_time_sync_pid;
extern int g_light_target;
extern int g_wifi_enabled;
extern gw_wifi_state_t g_wifi_state;
extern int g_evt_clients[GW_IPC_MAX_CLIENTS];
extern int g_evt_clients_ready;
void event_broadcast(const char *msg);

#if GW_LIFECYCLE_SPIKE
extern int g_supervise;
extern int g_pending_reader;
extern time_t g_pending_deadline;
extern int g_last_reader_status;
extern int g_shell_failures;
extern int g_locked;
extern int g_display_on;
extern int g_last_light;
extern int g_button_pressed;
extern struct timespec g_button_down;
extern pid_t g_shell_pid;
extern pid_t g_reader_pid;
extern pid_t g_wifi_pid;
extern int g_wifi_error;
extern int g_suspended;
extern struct timespec g_sleep_at;
extern int g_usb_state;
extern int g_charging_state;
extern struct timespec g_power_poll_at;
extern struct timespec g_wifi_poll_at;
extern struct timespec g_wifi_retry_at;
extern int g_wifi_retry_delay;
#endif

#endif /* GW_SYSTEM_STATE_H */

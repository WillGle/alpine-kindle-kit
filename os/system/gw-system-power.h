/* SPDX-License-Identifier: GPL-3.0-or-later
 * Power/display/suspend helpers for gw-system. */
#ifndef GW_SYSTEM_POWER_H
#define GW_SYSTEM_POWER_H

#if GW_LIFECYCLE_SPIKE

#define GW_DISPLAY_DEFAULT_LIGHT 13
#define GW_SUSPEND_DELAY_MS 1500

#include "gw-system-state.h"
#include "gw-power.h"
#include "gw-power-policy.h"

pid_t start_child(const char *path, int reader);
void display_light(int pct);
void display_off(void);
void display_wake(void);
long gw_ms_until(const struct timespec *when);
void gw_schedule_sleep(int delay_ms);
void gw_cancel_sleep(void);
void gw_fill_power_state(gw_power_state_t *state);
void gw_do_suspend(int force, int fallback_seconds);
#if GW_LIFECYCLE_SPIKE
void gw_power_sample(int startup);
#endif

extern int g_suspend_fallback;
void reap_children(void);

#if GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED
pid_t start_wifi(const char *action);
void gw_wifi_supervise(void);
#endif

#endif /* GW_LIFECYCLE_SPIKE */
#endif /* GW_SYSTEM_POWER_H */

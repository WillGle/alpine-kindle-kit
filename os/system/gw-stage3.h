/* SPDX-License-Identifier: GPL-3.0-or-later
 * Stage 3 helpers: time/date, frontlight and Wi-Fi state/persistence.
 *
 * These functions are intentionally free of device-specific side effects so
 * they can be unit tested on the host. gw-system.c owns all hardware access.
 *
 * STATUS: CORE (in use).
 */
#ifndef GW_STAGE3_H
#define GW_STAGE3_H

#include <stddef.h>
#include <time.h>

/* ── Frontlight ─────────────────────────────────────────────────────────── */
#define GW_LIGHT_RAW_MAX 4095

int gw_light_clamp_pct(int pct);
int gw_light_pct_to_raw(int pct);
int gw_light_raw_to_pct(int raw);
int gw_light_should_write(int current_pct, int requested_pct);

/* ── Time/date ──────────────────────────────────────────────────────────── */
#define GW_TIME_TZ_MAX 64
#define GW_TIME_FORMAT_12 12
#define GW_TIME_FORMAT_24 24

int gw_time_timezone_valid(const char *tz);
int gw_time_format_valid(int format);
int gw_time_format_string(time_t epoch, const char *tz, int format24,
                          char *buf, size_t size);

/* ── Persistent configuration (0600 files, atomic rename) ──────────────── */
int gw_config_save_int(const char *path, const char *key, int value);
int gw_config_load_int(const char *path, const char *key, int *value);
int gw_config_save_string(const char *path, const char *key, const char *value);
int gw_config_load_string(const char *path, const char *key, char *value, size_t size);

/* ── Wi-Fi state machine ────────────────────────────────────────────────── */
typedef enum {
    GW_WIFI_STATE_OFF = 0,
    GW_WIFI_STATE_SCANNING,
    GW_WIFI_STATE_CONNECTING,
    GW_WIFI_STATE_ASSOCIATED,
    GW_WIFI_STATE_OBTAINING_IP,
    GW_WIFI_STATE_CONNECTED,
    GW_WIFI_STATE_FAILED
} gw_wifi_state_t;

typedef enum {
    GW_WIFI_EVENT_ENABLE = 0,
    GW_WIFI_EVENT_DISABLE,
    GW_WIFI_EVENT_SCAN_START,
    GW_WIFI_EVENT_SCAN_DONE,
    GW_WIFI_EVENT_CONNECT_START,
    GW_WIFI_EVENT_ASSOCIATED,
    GW_WIFI_EVENT_OBTAIN_IP,
    GW_WIFI_EVENT_GOT_IP,
    GW_WIFI_EVENT_FAIL,
    GW_WIFI_EVENT_DISCONNECT,
    GW_WIFI_EVENT_TIMEOUT
} gw_wifi_event_t;

const char *gw_wifi_state_name(gw_wifi_state_t state);
int gw_wifi_transition(gw_wifi_state_t current, gw_wifi_event_t event,
                       gw_wifi_state_t *next);

#endif /* GW_STAGE3_H */
/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pure parser for the Gleworks lock-screen configuration.  No LVGL, no I/O:
 * callers feed one text line at a time. */
#ifndef GW_LOCK_CONFIG_H
#define GW_LOCK_CONFIG_H

#define GW_LOCK_MESSAGE_MAX 160

typedef struct {
    int use_image;
    int show_clock;
    int show_date;
    char message[GW_LOCK_MESSAGE_MAX];
} gw_lock_config_t;

void gw_lock_config_defaults(gw_lock_config_t *cfg);
void gw_lock_config_apply_line(gw_lock_config_t *cfg, const char *line);

#endif /* GW_LOCK_CONFIG_H */

/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pure parser for the Gleworks lock-screen configuration. */
#include "gw_lock_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void gw_lock_config_defaults(gw_lock_config_t *cfg) {
    if (!cfg) return;
    cfg->use_image = 0;
    cfg->show_clock = 1;
    cfg->show_date = 1;
    cfg->message[0] = '\0';
}

void gw_lock_config_apply_line(gw_lock_config_t *cfg, const char *line) {
    char buf[256];
    char *newline;
    if (!cfg || !line) return;
    /* Work on a mutable copy so callers may pass read-only storage. */
    snprintf(buf, sizeof(buf), "%s", line);
    newline = strchr(buf, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buf, '\r');
    if (newline) *newline = '\0';

    if (!strncmp(buf, "use_image=", 10)) {
        cfg->use_image = atoi(buf + 10) != 0;
    } else if (!strncmp(buf, "show_clock=", 11)) {
        cfg->show_clock = atoi(buf + 11) != 0;
    } else if (!strncmp(buf, "show_date=", 10)) {
        cfg->show_date = atoi(buf + 10) != 0;
    } else if (!strncmp(buf, "message=", 8)) {
        snprintf(cfg->message, sizeof(cfg->message), "%s", buf + 8);
    }
}

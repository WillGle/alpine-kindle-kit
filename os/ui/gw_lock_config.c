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

static int parse_bool(const char *value) {
    char *end = NULL;
    long parsed;
    if (!value || !*value) return 0;
    parsed = strtol(value, &end, 10);
    if (!end || *end != '\0') return -1;   /* reject trailing junk */
    return parsed != 0 ? 1 : 0;
}

void gw_lock_config_apply_line(gw_lock_config_t *cfg, const char *line) {
    char buf[256];
    char *newline;
    char *value;
    int parsed;
    if (!cfg || !line) return;
    /* Work on a mutable copy so callers may pass read-only storage. */
    snprintf(buf, sizeof(buf), "%s", line);
    newline = strchr(buf, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buf, '\r');
    if (newline) *newline = '\0';

    if (!strncmp(buf, "use_image=", 10)) {
        value = buf + 10;
        parsed = parse_bool(value);
        if (parsed >= 0) cfg->use_image = parsed;
    } else if (!strncmp(buf, "show_clock=", 11)) {
        value = buf + 11;
        parsed = parse_bool(value);
        if (parsed >= 0) cfg->show_clock = parsed;
    } else if (!strncmp(buf, "show_date=", 10)) {
        value = buf + 10;
        parsed = parse_bool(value);
        if (parsed >= 0) cfg->show_date = parsed;
    } else if (!strncmp(buf, "message=", 8)) {
        const char *value = buf + 8;
        size_t length = strlen(value);
        if (length >= sizeof(cfg->message))
            length = sizeof(cfg->message) - 1;
        memcpy(cfg->message, value, length);
        cfg->message[length] = '\0';
    }
}

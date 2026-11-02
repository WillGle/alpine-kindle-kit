/* SPDX-License-Identifier: GPL-3.0-or-later
 * Command descriptors and authorization flags shared by the service and its
 * host tests.  The dispatcher applies these flags once. */
#ifndef GW_COMMAND_POLICY_H
#define GW_COMMAND_POLICY_H

#include <stddef.h>
#include <string.h>

#define GW_CMD_PRIV     (1u << 0)   /* root only */
#define GW_CMD_SHELL    (1u << 1)   /* supervised shell PID only */
#define GW_CMD_LOCKED   (1u << 2)   /* rejected while the OS lock screen is up */

typedef struct {
    const char *prefix;
    unsigned flags;
} gw_command_policy_t;

/* Longest/most specific prefixes come first.  Adding a command means adding
 * one entry here plus its handler in gw-system.c. */
static const gw_command_policy_t gw_command_policies[] = {

    { "APP INSTALL USB", GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED },
    { "APP UPDATE USB",  GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED },
    { "APP LAUNCH",      GW_CMD_PRIV },
    { "APP REMOVE ",     GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED },
    { "APP EXIT",        GW_CMD_PRIV },
    { "APP CRASH",       GW_CMD_PRIV },
    { "APP COUNT",       GW_CMD_PRIV },
    { "APP LIST",        GW_CMD_PRIV },
    { "APP INFO ",       GW_CMD_PRIV },
    { "APP SCAN",        GW_CMD_PRIV },
    { "APP USB ",        GW_CMD_PRIV },
    { "NET CONNECT ",    GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED },
    { "NET RESULT ",     GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED },
    { "NET SCAN RESULTS", GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED },
    { "NET SCAN",        GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED },
    { "NET DISCO",       GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED },
    { "NET STATUS",      GW_CMD_PRIV },
    { "NET ENABLE",      GW_CMD_PRIV },
    { "NET DISABLE",     GW_CMD_PRIV },
    { "NET FORGET",      GW_CMD_PRIV },
    { "NET STATE",       0 },
    { "SUSPEND",         GW_CMD_PRIV },
    { "LIGHT SET ",      GW_CMD_PRIV },
    { "LIGHT GET",       0 },
    { "LOCK",            GW_CMD_PRIV },
    { "UNLOCK",          GW_CMD_PRIV },
    { "TIME SYNC",       GW_CMD_PRIV },
    { "TIME SET ",       GW_CMD_PRIV },
    { "TIME GET",        0 },
    { "TIMEZONE SET ",   GW_CMD_PRIV },
    { "TIMEZONE GET",    0 },
    { "TIME FORMAT ",    GW_CMD_PRIV },
    { "POWEROFF",        GW_CMD_PRIV },
    { "REBOOT",          GW_CMD_PRIV },
    { "RESCUE",          GW_CMD_PRIV },
    { "STATUS",          0 },
    { "CAPS",            0 },
};

static inline const gw_command_policy_t *gw_command_policy(const char *line) {
    size_t i;
    for (i = 0; i < sizeof(gw_command_policies)/sizeof(gw_command_policies[0]); i++)
        if (!strncmp(line, gw_command_policies[i].prefix,
                     strlen(gw_command_policies[i].prefix)))
            return &gw_command_policies[i];
    return NULL;
}

#endif /* GW_COMMAND_POLICY_H */

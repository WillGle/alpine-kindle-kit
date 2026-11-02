/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host tests for the command policy table: authorization matrix inputs and
 * prefix parsing. */
#include "gw-command-policy.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned flags_for(const char *line) {
    const gw_command_policy_t *p = gw_command_policy(line);
    return p ? p->flags : 0;
}

int main(void) {
    /* Read-only commands are open to any client that can connect. */
    CHECK(flags_for("STATUS") == 0);
    CHECK(flags_for("CAPS") == 0);
    CHECK(flags_for("LIGHT GET") == 0);

    /* State changes require root. */
    CHECK(flags_for("LIGHT SET 75") & GW_CMD_PRIV);
    CHECK(flags_for("TIME SET 100") & GW_CMD_PRIV);
    CHECK(flags_for("SUSPEND FORCE") & GW_CMD_PRIV);
    CHECK(!strcmp(gw_command_policy("SUSPEND FORCE")->prefix, "SUSPEND"));

    /* Destructive and interactive commands also require the supervised shell
     * and are rejected while locked. */
    CHECK(flags_for("APP INSTALL USB 0") ==
          (GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED));
    CHECK(flags_for("APP INSTALL USB ALL") ==
          (GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED));
    CHECK(flags_for("APP UPDATE USB 1") ==
          (GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED));
    CHECK(flags_for("APP REMOVE notes") ==
          (GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED));
    CHECK(flags_for("NET SCAN") ==
          (GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED));
    CHECK(flags_for("NET RESULT 0") ==
          (GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED));
    CHECK(flags_for("NET SCAN RESULTS") ==
          (GW_CMD_PRIV | GW_CMD_SHELL | GW_CMD_LOCKED));

    /* Prefix commands still parse without matching a longer command. */
    CHECK(!strncmp(gw_command_policy("NET RESULT 3")->prefix,
                   "NET RESULT ", 11));
    CHECK(!strncmp(gw_command_policy("APP INFO 3")->prefix, "APP INFO ", 9));
    CHECK(gw_command_policy("BOGUS") == NULL);

    if (failures) {
        printf("Command policy tests: %d failures\n", failures);
        return 1;
    }
    printf("Command policy tests: PASS\n");
    return 0;
}

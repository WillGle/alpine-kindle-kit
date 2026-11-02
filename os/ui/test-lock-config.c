/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>

#include "gw_lock_config.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want) {
    if (got != want) {
        printf("FAIL %s: got %d want %d\n", name, got, want);
        failures++;
    }
}

static void expect_str(const char *name, const char *got, const char *want) {
    if (strcmp(got, want) != 0) {
        printf("FAIL %s: got '%s' want '%s'\n", name, got, want);
        failures++;
    }
}

int main(void) {
    gw_lock_config_t cfg;

    gw_lock_config_defaults(&cfg);
    expect_int("default use_image", cfg.use_image, 0);
    expect_int("default show_clock", cfg.show_clock, 1);
    expect_int("default show_date", cfg.show_date, 1);
    expect_str("default message", cfg.message, "");

    /* parse a full config */
    gw_lock_config_defaults(&cfg);
    gw_lock_config_apply_line(&cfg, "use_image=1\n");
    gw_lock_config_apply_line(&cfg, "show_clock=0\r\n");
    gw_lock_config_apply_line(&cfg, "show_date=0\n");
    gw_lock_config_apply_line(&cfg, "message=hello world\n");
    expect_int("use_image", cfg.use_image, 1);
    expect_int("show_clock", cfg.show_clock, 0);
    expect_int("show_date", cfg.show_date, 0);
    expect_str("message", cfg.message, "hello world");

    /* message with equals sign preserved */
    gw_lock_config_defaults(&cfg);
    gw_lock_config_apply_line(&cfg, "message=a=b=c");
    expect_str("message equals", cfg.message, "a=b=c");

    /* unknown keys ignored */
    gw_lock_config_defaults(&cfg);
    gw_lock_config_apply_line(&cfg, "foo=bar");
    expect_int("unknown use_image", cfg.use_image, 0);

    /* boolean atoi: non-zero => true, "0" => false */
    gw_lock_config_defaults(&cfg);
    gw_lock_config_apply_line(&cfg, "use_image=42");
    expect_int("truthy", cfg.use_image, 1);

    /* empty message */
    gw_lock_config_defaults(&cfg);
    gw_lock_config_apply_line(&cfg, "message=");
    expect_str("empty message", cfg.message, "");

    /* CR-only line does not crash and keeps defaults */
    gw_lock_config_defaults(&cfg);
    gw_lock_config_apply_line(&cfg, "\r");
    expect_int("cr-only use_image", cfg.use_image, 0);
    expect_int("cr-only show_clock", cfg.show_clock, 1);

    /* strange values are ignored for booleans */
    gw_lock_config_defaults(&cfg);
    cfg.show_clock = 0;
    gw_lock_config_apply_line(&cfg, "use_image=abc");
    gw_lock_config_apply_line(&cfg, "show_clock=1abc");
    expect_int("strange use_image", cfg.use_image, 0);
    expect_int("strange show_clock", cfg.show_clock, 0);

    /* message truncation */
    gw_lock_config_defaults(&cfg);
    {
        char longmsg[GW_LOCK_MESSAGE_MAX + 20];
        int i;
        for (i = 0; i < GW_LOCK_MESSAGE_MAX + 10; i++) longmsg[i] = 'a';
        longmsg[GW_LOCK_MESSAGE_MAX + 10] = '\0';
        /* build config line */
        char line[GW_LOCK_MESSAGE_MAX + 32];
        snprintf(line, sizeof(line), "message=%s", longmsg);
        gw_lock_config_apply_line(&cfg, line);
        expect_int("truncated message length", (int)strlen(cfg.message),
                   GW_LOCK_MESSAGE_MAX - 1);
        expect_int("truncated message first", cfg.message[0], 'a');
        expect_int("truncated message last",
                   cfg.message[GW_LOCK_MESSAGE_MAX - 2], 'a');
    }

    if (failures == 0) {
        printf("lock config tests: PASS\n");
        return 0;
    }
    printf("lock config tests: %d FAILURES\n", failures);
    return 1;
}

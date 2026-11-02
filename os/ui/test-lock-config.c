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

    if (failures == 0) {
        printf("lock config tests: PASS\n");
        return 0;
    }
    printf("lock config tests: %d FAILURES\n", failures);
    return 1;
}

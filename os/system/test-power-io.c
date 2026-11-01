/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host tests for the PW3 power I/O helpers (RTC alarm, wakeup_count
 * handshake and button drain).  Paths point at temporary files so the live
 * kernel is never touched. */
#define _GNU_SOURCE
#ifndef GW_SYSFS_RTC_WAKEALARM
#define GW_SYSFS_RTC_WAKEALARM "/tmp/gw-power-test-wakealarm"
#endif
#ifndef GW_SYSFS_RTC_EPOCH
#define GW_SYSFS_RTC_EPOCH     "/tmp/gw-power-test-epoch"
#endif
#ifndef GW_SYSFS_WAKEUP_COUNT
#define GW_SYSFS_WAKEUP_COUNT  "/tmp/gw-power-test-wakeup-count"
#endif
#ifndef GW_SYSFS_PM_STATE
#define GW_SYSFS_PM_STATE      "/tmp/gw-power-test-state"
#endif
#define GW_POWER_SUSPEND_ACCEPTED 1
#include "gw-power.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    assert(f);
    fputs(text, f);
    fclose(f);
}

static void read_text(const char *path, char *buf, size_t size) {
    FILE *f = fopen(path, "r");
    size_t n = 0;
    assert(f);
    n = fread(buf, 1, size - 1, f);
    fclose(f);
    buf[n] = '\0';
}

int main(void) {
    char buf[64];
    int pipefd[2];

    /* RTC epoch + one-shot alarm, with readback verification. */
    write_text(GW_SYSFS_RTC_EPOCH, "1000\n");
    write_text(GW_SYSFS_RTC_WAKEALARM, "0\n");
    CHECK(gw_power_rtc_arm(10) == 0);
    read_text(GW_SYSFS_RTC_WAKEALARM, buf, sizeof(buf));
    CHECK(!strcmp(buf, "1010\n"));
    /* sysfs files ignore byte offsets; the temporary regular file must be
     * truncated explicitly to model a two-byte "0\n" clear. */
    truncate(GW_SYSFS_RTC_WAKEALARM, 0);
    CHECK(gw_power_rtc_clear() == 0);
    read_text(GW_SYSFS_RTC_WAKEALARM, buf, sizeof(buf));
    CHECK(!strcmp(buf, "0\n"));
    CHECK(gw_power_rtc_arm(0) == -1);
    CHECK(gw_power_rtc_arm(24 * 60 * 60 + 1) == -1);
    /* A missing RTC epoch is a clean failure, not a write to a guessed time. */
    unlink(GW_SYSFS_RTC_EPOCH);
    CHECK(gw_power_rtc_arm(5) == -1);

    /* wakeup_count handshake: the exact sampled string is written back
     * before "mem" is requested. */
    write_text(GW_SYSFS_WAKEUP_COUNT, "7\n");
    write_text(GW_SYSFS_PM_STATE, "");
    CHECK(gw_power_suspend() == 0);
    read_text(GW_SYSFS_PM_STATE, buf, sizeof(buf));
    CHECK(!strcmp(buf, "mem\n"));
    read_text(GW_SYSFS_WAKEUP_COUNT, buf, sizeof(buf));
    CHECK(!strcmp(buf, "7\n"));
    unlink(GW_SYSFS_WAKEUP_COUNT);
    CHECK(gw_power_suspend() == -1);

    /* Button drain: KEY_POWER down+up is detected, other events are not. */
    CHECK(pipe(pipefd) == 0);
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    {
        int pressed = -1;
        CHECK(gw_power_drain_button(pipefd[0], &pressed) == 0);
        CHECK(pressed == 0);
    }
    {
        struct input_event ev;
        int pressed = 0;
        memset(&ev, 0, sizeof(ev));
        ev.type = EV_KEY; ev.code = KEY_POWER; ev.value = 1;
        write(pipefd[1], &ev, sizeof(ev));
        ev.value = 0;
        write(pipefd[1], &ev, sizeof(ev));
        ev.type = EV_SYN; ev.code = SYN_REPORT; ev.value = 0;
        write(pipefd[1], &ev, sizeof(ev));
        CHECK(gw_power_drain_button(pipefd[0], &pressed) == 3);
        CHECK(pressed == 1);
    }
    {
        struct input_event ev;
        int pressed = -1;
        memset(&ev, 0, sizeof(ev));
        ev.type = EV_KEY; ev.code = KEY_VOLUMEUP; ev.value = 1;
        write(pipefd[1], &ev, sizeof(ev));
        CHECK(gw_power_drain_button(pipefd[0], &pressed) == 1);
        CHECK(pressed == 0);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    CHECK(gw_power_drain_button(-1, NULL) == -1);

    unlink(GW_SYSFS_RTC_WAKEALARM);
    unlink(GW_SYSFS_PM_STATE);
    if (failures) {
        printf("Power I/O tests: %d failures\n", failures);
        return 1;
    }
    printf("Power I/O tests: PASS\n");
    return 0;
}

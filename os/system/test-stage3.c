/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host tests for Stage 3 helpers. */
#include "gw-stage3.h"
#include "gw-ipc.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void) {
    /* Frontlight */
    CHECK(gw_light_clamp_pct(-5) == 0);
    CHECK(gw_light_clamp_pct(150) == 100);
    CHECK(gw_light_clamp_pct(42) == 42);
    CHECK(gw_light_pct_to_raw(0) == 0);
    CHECK(gw_light_pct_to_raw(100) == 4095);
    CHECK(gw_light_raw_to_pct(0) == 0);
    CHECK(gw_light_raw_to_pct(4095) == 100);
    CHECK(gw_light_raw_to_pct(4096) == -1);
    CHECK(gw_light_should_write(10, 10) == 0);
    CHECK(gw_light_should_write(10, 11) == 1);

    /* Timezone */
    CHECK(gw_time_timezone_valid("UTC") == 1);
    CHECK(gw_time_timezone_valid("America/New_York") == 1);
    CHECK(gw_time_timezone_valid("") == 0);
    CHECK(gw_time_timezone_valid("bad tz") == 0);
    CHECK(gw_time_format_valid(12) == 1);
    CHECK(gw_time_format_valid(24) == 1);
    CHECK(gw_time_format_valid(13) == 0);

    char buf[64];
    time_t epoch = 1700000000;
    CHECK(gw_time_format_string(epoch, "UTC", 24, buf, sizeof(buf)) == 0);
    CHECK(strstr(buf, ":") != NULL);

    /* Config persistence */
    char path[] = "/tmp/opencode/stage3-test.conf";
    unlink(path);
    CHECK(gw_config_save_int(path, "brightness", 77) == 0);
    int v = -1;
    CHECK(gw_config_load_int(path, "brightness", &v) == 0);
    CHECK(v == 77);
    CHECK(gw_config_save_string(path, "timezone", "UTC") == 0);
    char tz[32] = {0};
    CHECK(gw_config_load_string(path, "timezone", tz, sizeof(tz)) == 0);
    CHECK(strcmp(tz, "UTC") == 0);
    CHECK(gw_config_load_int(path, "missing", &v) != 0);

    /* Wi-Fi state machine */
    gw_wifi_state_t next;
    CHECK(gw_wifi_transition(GW_WIFI_STATE_OFF, GW_WIFI_EVENT_ENABLE, &next) == 0);
    CHECK(gw_wifi_transition(GW_WIFI_STATE_OFF, GW_WIFI_EVENT_SCAN_START, &next) == 0);
    CHECK(next == GW_WIFI_STATE_SCANNING);
    CHECK(gw_wifi_transition(GW_WIFI_STATE_SCANNING, GW_WIFI_EVENT_SCAN_DONE, &next) == 0);
    CHECK(gw_wifi_transition(GW_WIFI_STATE_OFF, GW_WIFI_EVENT_CONNECT_START, &next) == 0);
    CHECK(next == GW_WIFI_STATE_CONNECTING);
    CHECK(gw_wifi_transition(GW_WIFI_STATE_CONNECTING, GW_WIFI_EVENT_ASSOCIATED, &next) == 0);
    CHECK(next == GW_WIFI_STATE_ASSOCIATED);
    CHECK(gw_wifi_transition(GW_WIFI_STATE_ASSOCIATED, GW_WIFI_EVENT_OBTAIN_IP, &next) == 0);
    CHECK(next == GW_WIFI_STATE_OBTAINING_IP);
    CHECK(gw_wifi_transition(GW_WIFI_STATE_OBTAINING_IP, GW_WIFI_EVENT_GOT_IP, &next) == 0);
    CHECK(next == GW_WIFI_STATE_CONNECTED);
    CHECK(gw_wifi_transition(GW_WIFI_STATE_CONNECTED, GW_WIFI_EVENT_DISCONNECT, &next) == 0);
    CHECK(next == GW_WIFI_STATE_OFF);
    CHECK(gw_wifi_transition(GW_WIFI_STATE_OFF, GW_WIFI_EVENT_DISABLE, &next) != 0);

    /* Authorisation and event-write policy. */
    CHECK(gw_ipc_uid_privileged(0) == 1);
    CHECK(gw_ipc_uid_privileged(1000) == 0);
    CHECK(gw_ipc_uid_privileged((unsigned long)-1) == 0);
    CHECK(gw_ipc_drop_listener(10, 10, 0) == 0);
    CHECK(gw_ipc_drop_listener(4, 10, 0) == 1);
    CHECK(gw_ipc_drop_listener(-1, 10, EAGAIN) == 1);
    CHECK(gw_ipc_drop_listener(-1, 10, EPIPE) == 1);

    /* Malformed persisted values are rejected, not truncated. */
    {
        FILE *bad = fopen(path, "w");
        CHECK(bad != NULL);
        if (bad) {
            fputs("brightness=12junk\nformat=99999999999999999999\n", bad);
            fclose(bad);
        }
        CHECK(gw_config_load_int(path, "brightness", &v) != 0);
        CHECK(gw_config_load_int(path, "format", &v) != 0);
    }
    /* Repeated saves use private temporary names and leave no debris. */
    CHECK(gw_config_save_int(path, "brightness", 55) == 0);
    CHECK(gw_config_save_int(path, "brightness", 56) == 0);
    CHECK(gw_config_load_int(path, "brightness", &v) == 0 && v == 56);
    {
        char tmp[128];
        int leftovers = 0;
        for (int i = 0; i < 8; i++) {
            snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, i);
            leftovers += access(tmp, F_OK) == 0;
        }
        CHECK(leftovers == 0);
    }
    /* A managed save failure must be visible to the caller. */
    CHECK(gw_config_save_int("/tmp/opencode", "x", 1) != 0);
    CHECK(gw_config_save_string(path, "bad\nkey", "v") != 0);
    CHECK(gw_config_save_string(path, "key", "bad\nvalue") != 0);
    CHECK(strcmp(gw_wifi_state_name(GW_WIFI_STATE_FAILED), "FAILED") == 0);

    if (failures) {
        printf("Stage 3 tests: %d failures\n", failures);
        return 1;
    }
    printf("Stage 3 tests: PASS\n");
    return 0;
}
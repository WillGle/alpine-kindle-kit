/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host tests for Stage 3 helpers. */
#define _GNU_SOURCE
#include "gw-stage3.h"
#include "gw-file.h"
#include <dirent.h>
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
    CHECK(gw_time_timezone_posix("Asia/Ho_Chi_Minh") != NULL);
    CHECK(gw_time_timezone_posix("Not/AZone") == NULL);
    CHECK(gw_time_timezone_valid("") == 0);
    CHECK(gw_time_timezone_valid("bad tz") == 0);
    CHECK(gw_time_format_valid(12) == 1);
    CHECK(gw_time_format_valid(24) == 1);
    CHECK(gw_time_format_valid(13) == 0);

    char buf[64];
    time_t epoch = 1700000000;
    CHECK(gw_time_format_string(epoch, "UTC", 24, buf, sizeof(buf)) == 0);
    CHECK(strstr(buf, ":") != NULL);

    /* Config persistence: a private directory so parallel test runs cannot
     * collide and no fixed file is left in /tmp. */
    char tmpdir[] = "/tmp/gw-stage3-XXXXXX";
    char path[128];
    if (!mkdtemp(tmpdir)) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(path, sizeof(path), "%s/gw-stage3-test.conf", tmpdir);
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
        char dir[128], base[128];
        const char *slash;
        DIR *entries;
        struct dirent *entry;
        int leftovers = 0;
        snprintf(dir, sizeof(dir), "%s", path);
        slash = strrchr(dir, '/');
        CHECK(slash != NULL);
        if (slash) {
            snprintf(base, sizeof(base), "%s", slash + 1);
            *((char *)slash) = '\0';
            entries = opendir(dir);
            CHECK(entries != NULL);
            while (entries && (entry = readdir(entries)) != NULL) {
                size_t n = strlen(base);
                if (!strncmp(entry->d_name, base, n) &&
                    !strncmp(entry->d_name + n, ".tmp.", 5))
                    leftovers++;
            }
            if (entries) closedir(entries);
        }
        CHECK(leftovers == 0);
    }
    /* A symlink at the target path is replaced, never followed. */
    {
        char victim[128], link[128];
        const char *dir = strrchr(path, '/');
        snprintf(victim, sizeof(victim), "%s", path);
        if (dir) {
            size_t n = (size_t)(dir - path);
            snprintf(victim, sizeof(victim), "%.*s/victim.txt", (int)n, path);
            snprintf(link, sizeof(link), "%s", path);
            unlink(link);
            {
                FILE *seed = fopen(victim, "w");
                CHECK(seed != NULL);
                if (seed) { fputs("keep", seed); fclose(seed); }
            }
            CHECK(symlink(victim, link) == 0);
            CHECK(gw_write_atomic(link, "new", 3, 0600) == 0);
            {
                char text[16] = "";
                FILE *read = fopen(victim, "r");
                if (read) { fgets(text, sizeof(text), read); fclose(read); }
                CHECK(strcmp(text, "keep") == 0);
            }
            unlink(victim);
        }
    }

    /* A managed save failure must be visible to the caller. */
    CHECK(gw_config_save_int("/tmp", "x", 1) != 0);
    CHECK(gw_config_save_string(path, "bad\nkey", "v") != 0);
    CHECK(gw_config_save_string(path, "key", "bad\nvalue") != 0);
    CHECK(strcmp(gw_wifi_state_name(GW_WIFI_STATE_FAILED), "FAILED") == 0);

    unlink(path);
    rmdir(tmpdir);

    if (failures) {
        printf("Stage 3 tests: %d failures\n", failures);
        return 1;
    }
    printf("Stage 3 tests: PASS\n");
    return 0;
}
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host tests for the WPA supplicant configuration writer. */
#define _GNU_SOURCE
#include "gw-network.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static char *read_file(const char *path) {
    static char buffer[4096];
    int fd = open(path, O_RDONLY);
    ssize_t n;
    if (fd < 0) return NULL;
    n = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (n < 0) return NULL;
    buffer[n] = '\0';
    return buffer;
}

int main(void) {
    const char *config = GW_WIFI_DIR "/wifi.conf";
    struct stat st;
    char *text;

    unlink(config);
    rmdir(GW_WIFI_DIR);
    if (mkdir(GW_WIFI_DIR, 0700) != 0) {
        perror("mkdir");
        return 1;
    }

    /* Open network: empty password must produce key_mgmt=NONE and no psk. */
    CHECK(gw_network_save("4f70656e4e6574", 14, NULL, 0) == 0);
    text = read_file(config);
    CHECK(text != NULL);
    if (text) {
        CHECK(strstr(text, "ssid=4f70656e4e6574") != NULL);
        CHECK(strstr(text, "key_mgmt=NONE") != NULL);
        CHECK(strstr(text, "psk") == NULL);
    }
    CHECK(stat(config, &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);

    /* WPA2: a password is written as psk and key_mgmt stays automatic. */
    CHECK(gw_network_save("5770414e6574", 12,
                          "70617373776f7264", 16) == 0);
    text = read_file(config);
    CHECK(text != NULL);
    if (text) {
        CHECK(strstr(text, "ssid=5770414e6574") != NULL);
        CHECK(strstr(text, "psk=\"password\"") != NULL);
        CHECK(strstr(text, "key_mgmt=NONE") == NULL);
    }

    /* Too-short passwords are rejected (1..15 hex characters). */
    CHECK(gw_network_save("4f70656e4e6574", 14, "61", 2) != 0);
    CHECK(gw_network_save("4f70656e4e6574", 14, "616263646566676", 15) != 0);
    /* Odd-length and over-long values are rejected too. */
    CHECK(gw_network_save("4f70656e4e6574", 14, "61626364656667686", 17) != 0);

    unlink(config);
    rmdir(GW_WIFI_DIR);
    if (failures) {
        fprintf(stderr, "Network config tests: %d failures\n", failures);
        return 1;
    }
    printf("Network config tests: PASS\n");
    return 0;
}

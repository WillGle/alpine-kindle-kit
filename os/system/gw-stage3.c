/* SPDX-License-Identifier: GPL-3.0-or-later
 * STATUS: CORE (in use). */
#define _GNU_SOURCE
#include "gw-stage3.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int gw_light_clamp_pct(int pct) {
    if (pct < 0) return 0;
    if (pct > 100) return 100;
    return pct;
}

int gw_light_pct_to_raw(int pct) {
    pct = gw_light_clamp_pct(pct);
    return (pct * GW_LIGHT_RAW_MAX + 50) / 100;
}

int gw_light_raw_to_pct(int raw) {
    if (raw < 0 || raw > GW_LIGHT_RAW_MAX) return -1;
    return (raw * 100 + (GW_LIGHT_RAW_MAX / 2)) / GW_LIGHT_RAW_MAX;
}

int gw_light_should_write(int current_pct, int requested_pct) {
    requested_pct = gw_light_clamp_pct(requested_pct);
    current_pct = gw_light_clamp_pct(current_pct);
    return current_pct != requested_pct;
}

int gw_time_timezone_valid(const char *tz) {
    size_t n;
    if (!tz || !*tz) return 0;
    n = strlen(tz);
    if (n >= GW_TIME_TZ_MAX) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)tz[i];
        if (!(isalnum(c) || c == '/' || c == '_' || c == '-' || c == '+' ||
              c == ':'))
            return 0;
    }
    return 1;
}

int gw_time_format_valid(int format) {
    return format == GW_TIME_FORMAT_12 || format == GW_TIME_FORMAT_24;
}

int gw_time_format_string(time_t epoch, const char *tz, int format24,
                          char *buf, size_t size) {
    struct tm tm;
    const char *fmt;
    if (!buf || size == 0) return -1;
    if (tz && *tz && gw_time_timezone_valid(tz)) {
        setenv("TZ", tz, 1);
        tzset();
    }
    if (!localtime_r(&epoch, &tm)) return -1;
    fmt = format24 == GW_TIME_FORMAT_24 ? "%Y-%m-%d %H:%M" : "%Y-%m-%d %I:%M %p";
    if (strftime(buf, size, fmt, &tm) == 0) return -1;
    return 0;
}

static int config_write_atomic(const char *path, const char *content) {
    char tmp[256], parent[256];
    int fd, dirfd;
    FILE *f;
    size_t len = strlen(content);
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return -1;
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        mkdir(parent, 0700);
    }
    fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) return -1;
    f = fdopen(fd, "w");
    if (!f) { close(fd); unlink(tmp); return -1; }
    if (fwrite(content, 1, len, f) != len || fflush(f) || fsync(fileno(f))) {
        fclose(f); unlink(tmp); return -1;
    }
    if (fclose(f)) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    /* Best-effort directory sync. */
    char dir[256];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash2 = strrchr(dir, '/');
    if (slash2) {
        *slash2 = '\0';
        dirfd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dirfd >= 0) { fsync(dirfd); close(dirfd); }
    }
    return 0;
}

int gw_config_save_int(const char *path, const char *key, int value) {
    char content[512], line[128];
    FILE *f;
    size_t used = 0;
    int found = 0;
    if (!path || !key || !*key) return -1;
    /* Read existing keys to preserve them. */
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char existing[64];
            char *eq = strchr(line, '=');
            if (eq) {
                size_t klen = (size_t)(eq - line);
                if (klen < sizeof(existing)) {
                    memcpy(existing, line, klen);
                    existing[klen] = '\0';
                    if (strcmp(existing, key) == 0) { found = 1; continue; }
                }
            }
            size_t len = strlen(line);
            if (used + len + 1 < sizeof(content)) {
                memcpy(content + used, line, len);
                used += len;
                content[used] = '\0';
            }
        }
        fclose(f);
    }
    /* Append or replace the requested key. */
    {
        char addition[128];
        int n = snprintf(addition, sizeof(addition), "%s=%d\n", key, value);
        if (n < 0 || (size_t)n >= sizeof(addition)) return -1;
        if (used + (size_t)n + 1 >= sizeof(content)) return -1;
        memcpy(content + used, addition, (size_t)n);
        used += (size_t)n;
        content[used] = '\0';
    }
    (void)found;
    return config_write_atomic(path, content);
}

int gw_config_load_int(const char *path, const char *key, int *value) {
    FILE *f;
    char line[128], found[64];
    if (!path || !key || !value) return -1;
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        size_t klen = (size_t)(eq - line);
        if (klen >= sizeof(found)) continue;
        memcpy(found, line, klen);
        found[klen] = '\0';
        if (strcmp(found, key) == 0) {
            char *end = NULL;
            long v = strtol(eq + 1, &end, 10);
            fclose(f);
            if (end == eq + 1) return -1;
            *value = (int)v;
            return 0;
        }
    }
    fclose(f);
    return -1;
}

int gw_config_save_string(const char *path, const char *key, const char *value) {
    char content[512], line[256];
    FILE *f;
    size_t used = 0;
    if (!path || !key || !*key || !value) return -1;
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char existing[64];
            char *eq = strchr(line, '=');
            if (eq) {
                size_t klen = (size_t)(eq - line);
                if (klen < sizeof(existing)) {
                    memcpy(existing, line, klen);
                    existing[klen] = '\0';
                    if (strcmp(existing, key) == 0) continue;
                }
            }
            size_t len = strlen(line);
            if (used + len + 1 < sizeof(content)) {
                memcpy(content + used, line, len);
                used += len;
                content[used] = '\0';
            }
        }
        fclose(f);
    }
    {
        char addition[256];
        int n = snprintf(addition, sizeof(addition), "%s=%s\n", key, value);
        if (n < 0 || (size_t)n >= sizeof(addition)) return -1;
        if (used + (size_t)n + 1 >= sizeof(content)) return -1;
        memcpy(content + used, addition, (size_t)n);
        used += (size_t)n;
        content[used] = '\0';
    }
    return config_write_atomic(path, content);
}

int gw_config_load_string(const char *path, const char *key, char *value, size_t size) {
    FILE *f;
    char line[256], found[64];
    if (!path || !key || !value || size == 0) return -1;
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        size_t klen = (size_t)(eq - line);
        if (klen >= sizeof(found)) continue;
        memcpy(found, line, klen);
        found[klen] = '\0';
        if (strcmp(found, key) == 0) {
            char *nl = strchr(eq + 1, '\n');
            size_t vlen = nl ? (size_t)(nl - (eq + 1)) : strlen(eq + 1);
            if (vlen >= size) { fclose(f); return -1; }
            memcpy(value, eq + 1, vlen);
            value[vlen] = '\0';
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -1;
}

const char *gw_wifi_state_name(gw_wifi_state_t state) {
    switch (state) {
        case GW_WIFI_STATE_OFF: return "OFF";
        case GW_WIFI_STATE_SCANNING: return "SCANNING";
        case GW_WIFI_STATE_CONNECTING: return "CONNECTING";
        case GW_WIFI_STATE_ASSOCIATED: return "ASSOCIATED";
        case GW_WIFI_STATE_OBTAINING_IP: return "OBTAINING_IP";
        case GW_WIFI_STATE_CONNECTED: return "CONNECTED";
        case GW_WIFI_STATE_FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

int gw_wifi_transition(gw_wifi_state_t current, gw_wifi_event_t event,
                       gw_wifi_state_t *next) {
    gw_wifi_state_t result = current;
    int ok = 1;
    switch (current) {
        case GW_WIFI_STATE_OFF:
            if (event == GW_WIFI_EVENT_ENABLE) result = GW_WIFI_STATE_OFF; /* stays off until scan/connect */
            else if (event == GW_WIFI_EVENT_SCAN_START) result = GW_WIFI_STATE_SCANNING;
            else if (event == GW_WIFI_EVENT_CONNECT_START) result = GW_WIFI_STATE_CONNECTING;
            else ok = 0;
            break;
        case GW_WIFI_STATE_SCANNING:
            if (event == GW_WIFI_EVENT_SCAN_DONE) result = GW_WIFI_STATE_OFF;
            else if (event == GW_WIFI_EVENT_CONNECT_START) result = GW_WIFI_STATE_CONNECTING;
            else if (event == GW_WIFI_EVENT_DISABLE) result = GW_WIFI_STATE_OFF;
            else if (event == GW_WIFI_EVENT_FAIL) result = GW_WIFI_STATE_FAILED;
            else ok = 0;
            break;
        case GW_WIFI_STATE_CONNECTING:
            if (event == GW_WIFI_EVENT_ASSOCIATED) result = GW_WIFI_STATE_ASSOCIATED;
            else if (event == GW_WIFI_EVENT_GOT_IP) result = GW_WIFI_STATE_CONNECTED;
            else if (event == GW_WIFI_EVENT_FAIL || event == GW_WIFI_EVENT_TIMEOUT) result = GW_WIFI_STATE_FAILED;
            else if (event == GW_WIFI_EVENT_DISABLE) result = GW_WIFI_STATE_OFF;
            else ok = 0;
            break;
        case GW_WIFI_STATE_ASSOCIATED:
            if (event == GW_WIFI_EVENT_OBTAIN_IP) result = GW_WIFI_STATE_OBTAINING_IP;
            else if (event == GW_WIFI_EVENT_GOT_IP) result = GW_WIFI_STATE_CONNECTED;
            else if (event == GW_WIFI_EVENT_FAIL || event == GW_WIFI_EVENT_TIMEOUT) result = GW_WIFI_STATE_FAILED;
            else if (event == GW_WIFI_EVENT_DISCONNECT) result = GW_WIFI_STATE_OFF;
            else ok = 0;
            break;
        case GW_WIFI_STATE_OBTAINING_IP:
            if (event == GW_WIFI_EVENT_GOT_IP) result = GW_WIFI_STATE_CONNECTED;
            else if (event == GW_WIFI_EVENT_FAIL || event == GW_WIFI_EVENT_TIMEOUT) result = GW_WIFI_STATE_FAILED;
            else if (event == GW_WIFI_EVENT_DISCONNECT) result = GW_WIFI_STATE_OFF;
            else ok = 0;
            break;
        case GW_WIFI_STATE_CONNECTED:
            if (event == GW_WIFI_EVENT_DISCONNECT) result = GW_WIFI_STATE_OFF;
            else if (event == GW_WIFI_EVENT_FAIL) result = GW_WIFI_STATE_FAILED;
            else if (event == GW_WIFI_EVENT_SCAN_START) result = GW_WIFI_STATE_SCANNING;
            else ok = 0;
            break;
        case GW_WIFI_STATE_FAILED:
            if (event == GW_WIFI_EVENT_ENABLE || event == GW_WIFI_EVENT_SCAN_START) result = GW_WIFI_STATE_SCANNING;
            else if (event == GW_WIFI_EVENT_CONNECT_START) result = GW_WIFI_STATE_CONNECTING;
            else if (event == GW_WIFI_EVENT_DISABLE) result = GW_WIFI_STATE_OFF;
            else ok = 0;
            break;
        default:
            ok = 0;
            break;
    }
    if (next) *next = result;
    return ok ? 0 : -1;
}
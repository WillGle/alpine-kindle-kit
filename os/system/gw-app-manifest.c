/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw-app-manifest.h"
#include <string.h>

/* STATUS: SPIKE (accepted=0) — app manager not accepted yet. */

static int copy_field(char *dst, size_t capacity, const char *src) {
    size_t n = strlen(src);
    if (!n || n >= capacity) return -1;
    memcpy(dst, src, n + 1);
    return 0;
}

static int lower_id(const char *s) {
    const char *p = s;
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) return 0;
    while (*p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
              *p == '-' || *p == '.')) return 0;
        p++;
    }
    return 1;
}

static int version_id(const char *s) {
    if (!*s) return 0;
    while (*s) {
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
              (*s >= '0' && *s <= '9') || *s == '.' || *s == '-' || *s == '_')) return 0;
        s++;
    }
    return 1;
}

static int relative_path(const char *s) {
    const char *part = s, *p = s;
    if (!*s || *s == '/') return 0;
    for (;;) {
        if (*p == '/' || *p == '\0') {
            size_t n = (size_t)(p - part);
            if (!n || (n == 1 && part[0] == '.') ||
                (n == 2 && part[0] == '.' && part[1] == '.')) return 0;
            if (!*p) return 1;
            part = ++p;
        } else {
            if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                  (*p >= '0' && *p <= '9') || *p == '.' || *p == '-' ||
                  *p == '_' || *p == '+')) return 0;
            p++;
        }
    }
}

static int known(const char *s, const char *const *allowed) {
    while (*allowed) if (!strcmp(s, *allowed++)) return 1;
    return 0;
}

static int parse_list(char *value, char *storage, size_t width, int limit,
                      int *count, const char *const *allowed) {
    char *item = value;
    *count = 0;
    if (!*item) return 0;
    for (;;) {
        char *comma = strchr(item, ',');
        int i;
        if (comma) *comma = '\0';
        if (!*item || *count == limit || !known(item, allowed) || strlen(item) >= width)
            return -1;
        for (i = 0; i < *count; i++)
            if (!strcmp(storage + (size_t)i * width, item)) return -1;
        strcpy(storage + (size_t)*count * width, item);
        (*count)++;
        if (!comma) return 0;
        item = comma + 1;
    }
}

static int numeric_version(const char *s, unsigned parts[3]) {
    int index = 0;
    memset(parts, 0, 3 * sizeof(*parts));
    if (!*s) return -1;
    for (;;) {
        unsigned value = 0;
        if (*s < '0' || *s > '9') return -1;
        do {
            value = value * 10 + (unsigned)(*s++ - '0');
            if (value > 65535) return -1;
        } while (*s >= '0' && *s <= '9');
        parts[index++] = value;
        if (!*s) return 0;
        if (*s++ != '.' || index == 3) return -1;
    }
}

int gw_app_manifest_parse(const char *data, size_t length, gw_app_manifest_t *out) {
    static const char *const formats[] = { "epub", "pdf", "cbz", "txt", "html", NULL };
    static const char *const permissions[] = { "display", "touch", "frontlight", "network", NULL };
    char buffer[GW_APP_MANIFEST_MAX + 1], *line, *next;
    unsigned seen = 0, version[3];
    size_t i;
    if (!data || !out || !length || length > GW_APP_MANIFEST_MAX) return -1;
    for (i = 0; i < length; i++)
        if (data[i] != '\n' && ((unsigned char)data[i] < 32 || (unsigned char)data[i] > 126)) return -1;
    memcpy(buffer, data, length); buffer[length] = '\0';
    memset(out, 0, sizeof(*out));
    line = buffer;
    while (*line) {
        char *equals;
        unsigned bit;
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        equals = strchr(line, '=');
        if (!equals || equals == line) return -1;
        *equals++ = '\0';
        if (*equals && (*equals == ' ' || equals[strlen(equals) - 1] == ' ')) return -1;
        if (!strcmp(line, "id")) {
            bit = 1U << 0;
            if (!lower_id(equals) || copy_field(out->id, sizeof(out->id), equals)) return -1;
        } else if (!strcmp(line, "version")) {
            bit = 1U << 1;
            if (!version_id(equals) || copy_field(out->version, sizeof(out->version), equals)) return -1;
        } else if (!strcmp(line, "entrypoint")) {
            bit = 1U << 2;
            if (!relative_path(equals) || copy_field(out->entrypoint, sizeof(out->entrypoint), equals)) return -1;
        } else if (!strcmp(line, "formats")) {
            bit = 1U << 3;
            if (parse_list(equals, &out->formats[0][0], GW_APP_FORMAT_MAX,
                           GW_APP_FORMAT_COUNT, &out->format_count, formats)) return -1;
        } else if (!strcmp(line, "permissions")) {
            bit = 1U << 4;
            if (parse_list(equals, &out->permissions[0][0], GW_APP_PERM_MAX,
                           GW_APP_PERM_COUNT, &out->perm_count, permissions)) return -1;
        } else if (!strcmp(line, "arch")) {
            bit = 1U << 5;
            if (copy_field(out->arch, sizeof(out->arch), equals)) return -1;
        } else if (!strcmp(line, "profile")) {
            bit = 1U << 6;
            if (copy_field(out->profile, sizeof(out->profile), equals)) return -1;
        } else if (!strcmp(line, "min_os")) {
            bit = 1U << 7;
            if (numeric_version(equals, version) || copy_field(out->min_os, sizeof(out->min_os), equals)) return -1;
        } else if (!strcmp(line, "description")) {
            bit = 1U << 8;
            if (copy_field(out->description, sizeof(out->description), equals)) return -1;
        } else return -1;
        if (seen & bit) return -1;
        seen |= bit;
        if (!next) break;
        line = next;
    }
    return (seen & 0xffU) == 0xffU ? 0 : -1;
}

int gw_app_manifest_compatible(const gw_app_manifest_t *app, const char *os_version) {
    unsigned minimum[3], current[3];
    int i;
    if (!app || !os_version || strcmp(app->arch, "armv7") ||
        strcmp(app->profile, "pw3-0kb-5.16.2.1.1") ||
        numeric_version(app->min_os, minimum) || numeric_version(os_version, current)) return 0;
    for (i = 0; i < 3; i++) {
        if (current[i] != minimum[i]) return current[i] > minimum[i];
    }
    return 1;
}

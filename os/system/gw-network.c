/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * STATUS: CORE (in use). */
#define _GNU_SOURCE
#include "gw-network.h"
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define GW_WIFI_DIR "/var/lib/gleworks"
#define GW_WIFI_CONFIG GW_WIFI_DIR "/wifi.conf"

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void erase(char *s, size_t n) {
    volatile char *p = s;
    while (n--) *p++ = 0;
}

int gw_network_enable_hotplug(void) {
    static const char helper[] = "/sbin/hotplug\n";
    int fd = open("/proc/sys/kernel/hotplug", O_WRONLY | O_CLOEXEC);
    ssize_t n;
    if (fd < 0) return -1;
    n = write(fd, helper, sizeof(helper) - 1);
    close(fd);
    return n == (ssize_t)(sizeof(helper) - 1) ? 0 : -1;
}

int gw_network_save(const char *ssid, size_t ssid_len, const char *password, size_t password_len) {
    char secret[64], temporary[128];
    struct stat st;
    FILE *file = NULL;
    size_t i, count;
    int fd = -1, dir = -1, result = -1;
    memset(secret, 0, sizeof(secret));
    if (!ssid || !password || ssid_len < 2 || ssid_len > 64 || (ssid_len & 1) ||
        password_len < 16 || password_len > 126 || (password_len & 1)) goto done;
    for (i = 0; i < ssid_len; i++) if (hex_digit(ssid[i]) < 0) goto done;
    count = password_len / 2;
    for (i = 0; i < count; i++) {
        int hi = hex_digit(password[2 * i]), lo = hex_digit(password[2 * i + 1]);
        if (hi < 0 || lo < 0) goto done;
        secret[i] = (char)((hi << 4) | lo);
        if ((unsigned char)secret[i] < 32 || (unsigned char)secret[i] > 126) goto done;
    }
    if (mkdir(GW_WIFI_DIR, 0700) < 0 && errno != EEXIST) goto done;
    if (lstat(GW_WIFI_DIR, &st) || !S_ISDIR(st.st_mode) || st.st_uid != 0 || (st.st_mode & 077)) goto done;
    dir = open(GW_WIFI_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (dir < 0) goto done;
    snprintf(temporary, sizeof(temporary), ".wifi-%ld.tmp", (long)getpid());
    fd = openat(dir, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) goto done;
    file = fdopen(fd, "w");
    if (!file) goto done;
    fd = -1;
    if (fprintf(file, "ctrl_interface=/run/wpa_supplicant\nupdate_config=0\nnetwork={\n    ssid=%.*s\n    psk=\"", (int)ssid_len, ssid) < 0) goto done;
    for (i = 0; i < count; i++) {
        if ((secret[i] == '"' || secret[i] == '\\') && fputc('\\', file) == EOF) goto done;
        if (fputc(secret[i], file) == EOF) goto done;
    }
    if (fputs("\"\n}\n", file) == EOF || fflush(file) || fsync(fileno(file))) goto done;
    if (fclose(file)) { file = NULL; goto done; }
    file = NULL;
    if (renameat(dir, temporary, dir, "wifi.conf")) goto done;
    if (fsync(dir) && errno != EINVAL && errno != EROFS) goto done;
    result = 0;
done:
    if (file) fclose(file);
    if (fd >= 0) close(fd);
    if (dir >= 0) { if (result) unlinkat(dir, temporary, 0); close(dir); }
    erase(secret, sizeof(secret));
    return result;
}

int gw_network_status(void) {
    struct ifreq request;
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0), result = -1;
    if (fd < 0) return -1;
    memset(&request, 0, sizeof(request));
    strcpy(request.ifr_name, "wlan0");
    if (ioctl(fd, SIOCGIFFLAGS, &request) < 0) {
        if (errno == ENODEV) result = 0;
    } else if (!(request.ifr_flags & IFF_UP) || !(request.ifr_flags & IFF_RUNNING)) result = 0;
    else if (ioctl(fd, SIOCGIFADDR, &request) == 0 &&
             ((struct sockaddr_in *)&request.ifr_addr)->sin_addr.s_addr != 0) result = 1;
    else result = 0;
    close(fd);
    return result;
}

int gw_network_scan_result(unsigned index, char *ssid_hex, size_t size,
                           int *signal_dbm, int *secure) {
    static const char digits[] = "0123456789abcdef";
    char line[512];
    FILE *file = fopen("/run/gw-wifi-scan", "r");
    unsigned found = 0;
    int result = -1;
    if (!file || !ssid_hex || size < 65) { if (file) fclose(file); return -1; }
    if (!fgets(line, sizeof(line), file)) goto done;
    while (fgets(line, sizeof(line), file)) {
        char *fields[6];
        char *p = line, *ssid;
        int nf = 0, i, flagged = 0;
        size_t n;
        fields[nf++] = p;
        for (; *p && nf < 6; p++) {
            if (*p == '\t') { *p = '\0'; fields[nf++] = p + 1; }
        }
        /* wpa_cli scan_results: bssid, frequency, signal, flags, ssid. */
        if (nf < 5) continue;
        ssid = fields[4];
        n = strcspn(ssid, "\r\n");
        if (!n || n > 32) continue;
        for (i = 0; i < (int)n; i++)
            if ((unsigned char)ssid[i] < 32 || (unsigned char)ssid[i] > 126 ||
                ssid[i] == '\\')
                break;
        if (i != (int)n) continue;
        if (found++ != index) continue;
        for (i = 0; i < (int)n; i++) {
            unsigned char c = (unsigned char)ssid[i];
            ssid_hex[2 * i] = digits[c >> 4];
            ssid_hex[2 * i + 1] = digits[c & 15];
        }
        ssid_hex[2 * n] = '\0';
        if (signal_dbm) *signal_dbm = atoi(fields[2]);
        if (secure) {
            flagged = strstr(fields[3], "WPA") != NULL ||
                      strstr(fields[3], "RSN") != NULL ||
                      strstr(fields[3], "WEP") != NULL;
            *secure = flagged ? 1 : 0;
        }
        result = 0;
        break;
    }
done:
    fclose(file);
    return result;
}

int gw_network_saved_ssid(char *ssid_hex, size_t size) {
    char line[256];
    FILE *file = fopen(GW_WIFI_CONFIG, "r");
    if (!file || !ssid_hex || size < 3) { if (file) fclose(file); return -1; }
    while (fgets(line, sizeof(line), file)) {
        char *value = line;
        size_t n;
        while (*value == ' ' || *value == '\t') value++;
        if (strncmp(value, "ssid=", 5)) continue;
        value += 5;
        n = strcspn(value, "\r\n");
        value[n] = '\0';
        if (n < 2 || n > 64 || (n & 1)) { fclose(file); return -1; }
        for (n = 0; value[n]; n++) if (hex_digit(value[n]) < 0) { fclose(file); return -1; }
        if (strlen(value) + 1 > size) { fclose(file); return -1; }
        strcpy(ssid_hex, value);
        fclose(file);
        return 0;
    }
    fclose(file);
    return -1;
}

int gw_network_ip(char *buf, size_t size) {
    struct ifreq request;
    int fd;
    if (!buf || size < 16) return -1;
    fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    memset(&request, 0, sizeof(request));
    strcpy(request.ifr_name, "wlan0");
    if (ioctl(fd, SIOCGIFADDR, &request) < 0) { close(fd); return -1; }
    if (!inet_ntop(AF_INET, &((struct sockaddr_in *)&request.ifr_addr)->sin_addr, buf, (socklen_t)size)) {
        close(fd); return -1;
    }
    close(fd);
    return 0;
}

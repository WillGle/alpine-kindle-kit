/* SPDX-License-Identifier: GPL-3.0-or-later
 * Fixed-command client for applications managed by gw-system.
 *
 * STATUS: CORE (in use). */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "gw-ipc.h"

static int ready(int fd, short events) {
    struct pollfd pfd = { .fd = fd, .events = events };
    int result;
    do { result = poll(&pfd, 1, 2000); } while (result < 0 && errno == EINTR);
    return result > 0 && (pfd.revents & events) ? 0 : -1;
}

int main(int argc, char **argv) {
    char request[GW_IPC_MAX_LINE], response[GW_IPC_MAX_LINE + 1];
    struct sockaddr_un addr;
    size_t used = 0, sent = 0, length;
    int fd, error = 0;
    socklen_t error_size = sizeof(error);

    int ui_mode = 0;
    if (argc == 3 && !strcmp(argv[1], "ui-nav")) {
        snprintf(request, sizeof(request), "NAV %s\n", argv[2]);
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/run/gw-shell.diag", sizeof(addr.sun_path) - 1);
        ui_mode = 1;
        goto ui_socket;
    } else if (argc == 4 && !strcmp(argv[1], "ui-tap")) {
        snprintf(request, sizeof(request), "TAP %s %s\n", argv[2], argv[3]);
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/run/gw-shell.diag", sizeof(addr.sun_path) - 1);
        ui_mode = 1;
        goto ui_socket;
    } else if (argc == 3 && !strcmp(argv[1], "ui-drag")) {
        snprintf(request, sizeof(request), "DRAG %s\n", argv[2]);
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/run/gw-shell.diag", sizeof(addr.sun_path) - 1);
        ui_mode = 1;
        goto ui_socket;
    } else if (argc == 3 && !strcmp(argv[1], "ui-scroll")) {
        snprintf(request, sizeof(request), "SCROLL %s\n", argv[2]);
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/run/gw-shell.diag", sizeof(addr.sun_path) - 1);
        ui_mode = 1;
        goto ui_socket;
    } else if (argc == 5 && !strcmp(argv[1], "ui-swipe")) {
        snprintf(request, sizeof(request), "SWIPE %s %s %s\n",
                 argv[2], argv[3], argv[4]);
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/run/gw-shell.diag", sizeof(addr.sun_path) - 1);
        ui_mode = 1;
        goto ui_socket;
    } else if (argc == 2 && !strcmp(argv[1], "ui-screen")) {
        strcpy(request, "SCREEN\n");
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/run/gw-shell.diag", sizeof(addr.sun_path) - 1);
        ui_mode = 1;
        goto ui_socket;
    }

    if (argc == 2 && !strcmp(argv[1], "status"))
        strcpy(request, GW_CMD_STATUS "\n");
    else if (argc == 2 && !strcmp(argv[1], "caps"))
        strcpy(request, GW_CMD_CAPS "\n");
    else if (argc == 2 && !strcmp(argv[1], "light-get"))
        strcpy(request, GW_CMD_LIGHT_GET "\n");
    else if (argc == 2 && !strcmp(argv[1], "time-get"))
        strcpy(request, GW_CMD_TIME_GET "\n");
    else if (argc == 2 && !strcmp(argv[1], "timezone-get"))
        strcpy(request, GW_CMD_TIMEZONE_GET "\n");
    else if (argc == 2 && !strcmp(argv[1], "time-sync"))
        strcpy(request, GW_CMD_TIME_SYNC "\n");
    else if (argc == 2 && !strcmp(argv[1], "wifi-state"))
        strcpy(request, GW_CMD_NET_STATE "\n");
    else if (argc == 2 && !strcmp(argv[1], "wifi-enable"))
        strcpy(request, GW_CMD_NET_ENABLE "\n");
    else if (argc == 2 && !strcmp(argv[1], "wifi-disable"))
        strcpy(request, GW_CMD_NET_DISABLE "\n");
    else if (argc == 2 && !strcmp(argv[1], "wifi-forget"))
        strcpy(request, GW_CMD_NET_FORGET "\n");
    else if (argc == 2 && !strcmp(argv[1], "lock"))
        strcpy(request, GW_CMD_LOCK "\n");
    else if (argc == 2 && !strcmp(argv[1], "unlock"))
        strcpy(request, GW_CMD_UNLOCK "\n");
    else if (argc == 2 && !strcmp(argv[1], "suspend"))
        strcpy(request, GW_CMD_SUSPEND "\n");
    else if (argc == 2 && !strcmp(argv[1], "suspend-force"))
        strcpy(request, "SUSPEND FORCE\n");
    else if (argc == 2 && !strcmp(argv[1], "suspend-test"))
        strcpy(request, "SUSPEND TEST\n");
    else if (argc == 3 && !strcmp(argv[1], "light-set")) {
        char *end;
        long value;
        if (!argv[2][0] || strspn(argv[2], "0123456789") != strlen(argv[2])) return 2;
        errno = 0;
        value = strtol(argv[2], &end, 10);
        if (errno || *end || value < 0 || value > 100) return 2;
        snprintf(request, sizeof(request), GW_CMD_LIGHT_SET " %ld\n", value);
    } else if (argc == 3 && !strcmp(argv[1], "time-set")) {
        char *end;
        long value;
        if (!argv[2][0] || strspn(argv[2], "0123456789") != strlen(argv[2])) return 2;
        errno = 0;
        value = strtol(argv[2], &end, 10);
        if (errno || *end || value < 0) return 2;
        snprintf(request, sizeof(request), GW_CMD_TIME_SET " %ld\n", value);
    } else if (argc == 3 && !strcmp(argv[1], "timezone-set")) {
        snprintf(request, sizeof(request), GW_CMD_TIMEZONE_SET " %s\n", argv[2]);
    } else if (argc == 3 && !strcmp(argv[1], "time-format")) {
        if (strcmp(argv[2], "12") && strcmp(argv[2], "24")) return 2;
        snprintf(request, sizeof(request), GW_CMD_TIME_FORMAT " %s\n", argv[2]);
    } else return 2;

ui_socket:
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return 1;
    if (!ui_mode) {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, GW_IPC_CMD_SOCKET, sizeof(GW_IPC_CMD_SOCKET));
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS || ready(fd, POLLOUT) < 0 ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_size) < 0 || error) {
            close(fd); return 1;
        }
    }
    length = strlen(request);
    while (sent < length) {
        ssize_t n;
        if (ready(fd, POLLOUT) < 0) { close(fd); return 1; }
        n = send(fd, request + sent, length - sent, MSG_NOSIGNAL);
        if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (n <= 0) { close(fd); return 1; }
        sent += (size_t)n;
    }
    while (used < GW_IPC_MAX_LINE) {
        ssize_t n;
        if (ready(fd, POLLIN) < 0) { close(fd); return 1; }
        n = read(fd, response + used, GW_IPC_MAX_LINE - used);
        if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (n <= 0) { close(fd); return 1; }
        used += (size_t)n;
        if (response[used - 1] == '\n') break;
    }
    close(fd);
    if (!used || response[used - 1] != '\n') return 1;
    response[used] = '\0';
    if (fputs(response, stdout) == EOF) return 1;
    return strncmp(response, GW_RESP_ERR " ", sizeof(GW_RESP_ERR)) == 0;
}

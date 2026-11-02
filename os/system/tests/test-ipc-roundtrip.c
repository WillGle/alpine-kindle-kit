/* SPDX-License-Identifier: GPL-3.0-or-later
 * Round-trip measurement: eight commands over one persistent connection must
 * produce one accept(), not eight one-shot connections. */
#define _GNU_SOURCE
#include "gw-ipc-client.h"
#include "gw-ipc.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define SOCKET_PATH "test-roundtrip.cmd"
#define COMMANDS 8

static int failures;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int read_line(int fd, char *buffer, size_t size) {
    size_t used = 0;
    while (used + 1 < size) {
        ssize_t n = read(fd, buffer + used, size - 1 - used);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        used += (size_t)n;
        if (memchr(buffer, '\n', used)) {
            buffer[used] = '\0';
            return 0;
        }
    }
    return -1;
}

static int reply(int fd, int index) {
    char text[32];
    int length = snprintf(text, sizeof(text), "OK %d\n", index);
    return write(fd, text, (size_t)length) == length ? 0 : -1;
}

static int server(int listener, int report_fd) {
    char request[GW_IPC_MAX_LINE];
    int client, accepts = 0, i;
    client = accept(listener, NULL, NULL);
    if (client < 0) return 1;
    accepts++;
    for (i = 1; i <= COMMANDS; i++) {
        if (read_line(client, request, sizeof(request)) < 0 ||
            reply(client, i) < 0)
            return 1;
    }
    close(client);
    if (write(report_fd, &accepts, sizeof(accepts)) != sizeof(accepts))
        return 1;
    close(report_fd);
    return 0;
}

int main(void) {
    struct sockaddr_un addr;
    gw_ipc_conn_t conn;
    char response[GW_IPC_MAX_LINE];
    int listener, status, report[2], accepts = 0;
    pid_t pid;

    signal(SIGPIPE, SIG_IGN);
    unlink(SOCKET_PATH);
    CHECK(pipe(report) == 0);
    listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    CHECK(listener >= 0);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    CHECK(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(listen(listener, 2) == 0);

    pid = fork();
    if (pid == 0) {
        close(report[0]);
        _exit(server(listener, report[1]));
    }
    close(report[1]);
    CHECK(pid > 0);

    gw_ipc_conn_init(&conn);
    for (int i = 1; i <= COMMANDS; i++) {
        CHECK(gw_ipc_conn_cmd(&conn, "STATUS", response,
                              sizeof(response)) == 0);
        CHECK(!strncmp(response, "OK ", 3));
        CHECK(atoi(response + 3) == i);
    }
    gw_ipc_conn_close(&conn);

    CHECK(read(report[0], &accepts, sizeof(accepts)) == sizeof(accepts));
    close(report[0]);
    CHECK(accepts == 1);
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    close(listener);
    unlink(SOCKET_PATH);

    printf("IPC round trips: %d commands / %d connection\n", COMMANDS, accepts);
    if (failures) {
        printf("IPC round-trip tests: %d failures\n", failures);
        return 1;
    }
    printf("IPC round-trip tests: PASS\n");
    return 0;
}

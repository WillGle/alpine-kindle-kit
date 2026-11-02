/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host test for the reconnecting IPC client.  A fake server keeps one
 * connection for two commands, closes it, then accepts a replacement
 * connection for the third command.  It then serves a multi-line batch
 * response while keeping the connection open, so the client must stop at the
 * protocol terminator instead of waiting for EOF or the socket timeout. */
#define _GNU_SOURCE
#include "gw-ipc-client.h"
#include "gw-ipc.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define SOCKET_PATH "test-client.cmd"

static int failures;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int read_request(int fd, char *buffer, size_t size) {
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

static int reply(int fd, const char *text) {
    size_t len = strlen(text), sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, text + sent, len - sent);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int server(int listener) {
    char request[GW_IPC_MAX_LINE];
    int client;
    client = accept(listener, NULL, NULL);
    if (client < 0) return 1;
    if (read_request(client, request, sizeof(request)) < 0 ||
        reply(client, "OK 1\n") < 0) return 1;
    if (read_request(client, request, sizeof(request)) < 0 ||
        reply(client, "OK 2\n") < 0) return 1;
    close(client);   /* break the connection before the third command */

    client = accept(listener, NULL, NULL);
    if (client < 0) return 1;
    if (read_request(client, request, sizeof(request)) < 0 ||
        reply(client, "OK 3\n") < 0) return 1;

    /* A bounded multi-line batch response over the still-open connection. */
    if (read_request(client, request, sizeof(request)) < 0 ||
        reply(client, "MULTI 2\nROW 1\nROW 2\nEND\n") < 0) return 1;
    /* The same connection must remain usable after the terminator. */
    if (read_request(client, request, sizeof(request)) < 0 ||
        reply(client, "OK 4\n") < 0) return 1;
    close(client);
    return 0;
}

int main(void) {
    struct sockaddr_un addr;
    gw_ipc_conn_t conn;
    char response[GW_IPC_MAX_LINE];
    int listener, status;
    pid_t pid;

    signal(SIGPIPE, SIG_IGN);
    unlink(SOCKET_PATH);
    listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    CHECK(listener >= 0);
    if (listener < 0) return 1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    CHECK(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(listen(listener, 2) == 0);

    pid = fork();
    if (pid == 0) _exit(server(listener));
    CHECK(pid > 0);

    gw_ipc_conn_init(&conn);
    CHECK(gw_ipc_conn_cmd(&conn, "FIRST", response, sizeof(response)) == 0);
    CHECK(!strcmp(response, "OK 1\n"));
    CHECK(gw_ipc_conn_cmd(&conn, "SECOND", response, sizeof(response)) == 0);
    CHECK(!strcmp(response, "OK 2\n"));
    /* The server closed after SECOND; the client must reconnect and retry. */
    CHECK(gw_ipc_conn_cmd(&conn, "THIRD", response, sizeof(response)) == 0);
    CHECK(!strcmp(response, "OK 3\n"));
    {
        struct timespec start, end;
        long elapsed_ms;
        CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
        CHECK(gw_ipc_conn_cmd_until(&conn, "MULTI", "END", response,
                                    sizeof(response)) == 0);
        CHECK(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
        elapsed_ms = (end.tv_sec - start.tv_sec) * 1000L +
                     (end.tv_nsec - start.tv_nsec) / 1000000L;
        CHECK(strstr(response, "ROW 2\n") != NULL);
        CHECK(strstr(response, "END\n") != NULL);
        CHECK(elapsed_ms < 1500);   /* not the 2 s socket timeout */
    }
    /* No leftover multi-line data may be parsed as the next response. */
    CHECK(gw_ipc_conn_cmd(&conn, "FOURTH", response, sizeof(response)) == 0);
    CHECK(!strcmp(response, "OK 4\n"));
    gw_ipc_conn_close(&conn);
    CHECK(conn.fd == -1);

    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    close(listener);
    unlink(SOCKET_PATH);

    if (failures) {
        printf("IPC client reconnect tests: %d failures\n", failures);
        return 1;
    }
    printf("IPC client reconnect tests: PASS\n");
    return 0;
}

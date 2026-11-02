/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "gw-ipc-client.h"
#include "gw-ipc.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

void gw_ipc_conn_init(gw_ipc_conn_t *conn) {
    if (conn) conn->fd = -1;
}

void gw_ipc_conn_close(gw_ipc_conn_t *conn) {
    if (!conn) return;
    if (conn->fd >= 0) close(conn->fd);
    conn->fd = -1;
}

int gw_ipc_conn_open(gw_ipc_conn_t *conn) {
    struct sockaddr_un addr;
    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    if (!conn) return -1;
    gw_ipc_conn_close(conn);
    conn->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (conn->fd < 0) return -1;
    if (setsockopt(conn->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   sizeof(timeout)) < 0 ||
        setsockopt(conn->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                   sizeof(timeout)) < 0) {
        gw_ipc_conn_close(conn);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, GW_IPC_CMD_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(conn->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gw_ipc_conn_close(conn);
        return -1;
    }
    return 0;
}

static int send_command(gw_ipc_conn_t *conn, const char *cmd) {
    char line[GW_IPC_MAX_LINE];
    size_t len = strlen(cmd);
    if (len + 1 > sizeof(line)) return -1;
    memcpy(line, cmd, len);
    line[len++] = '\n';
    if (send(conn->fd, line, len, MSG_NOSIGNAL) != (ssize_t)len) {
        gw_ipc_conn_close(conn);
        return -1;
    }
    return 0;
}

static int exchange(gw_ipc_conn_t *conn, const char *cmd,
                    char *resp, size_t resp_size) {
    size_t used = 0;
    if (!cmd || !resp || resp_size == 0) return -1;
    resp[0] = '\0';
    if (conn->fd < 0 && gw_ipc_conn_open(conn) < 0) return -1;
    if (send_command(conn, cmd) < 0) return -1;
    while (used + 1 < resp_size) {
        ssize_t n = read(conn->fd, resp + used, resp_size - 1 - used);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            gw_ipc_conn_close(conn);
            return -1;
        }
        used += (size_t)n;
        if (resp[used - 1] == '\n') break;
    }
    if (!used || resp[used - 1] != '\n') {
        gw_ipc_conn_close(conn);
        return -1;
    }
    resp[used] = '\0';
    return 0;
}

static int line_is(const char *line, size_t length, const char *terminator) {
    size_t expected = strlen(terminator);
    while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
        length--;
    return length == expected && !memcmp(line, terminator, expected);
}

static int has_terminator(const char *buffer, size_t used,
                          const char *terminator) {
    size_t start = 0, i;
    for (i = 0; i < used; i++) {
        if (buffer[i] == '\n') {
            if (line_is(buffer + start, i - start + 1, terminator)) return 1;
            start = i + 1;
        }
    }
    return 0;
}

static int exchange_until(gw_ipc_conn_t *conn, const char *cmd,
                          const char *terminator,
                          char *resp, size_t resp_size) {
    size_t used = 0;
    if (!cmd || !terminator || !resp || resp_size == 0) return -1;
    resp[0] = '\0';
    if (conn->fd < 0 && gw_ipc_conn_open(conn) < 0) return -1;
    if (send_command(conn, cmd) < 0) return -1;
    for (;;) {
        ssize_t n;
        if (used + 1 >= resp_size) {
            /* Keep the response usable but never reuse a connection with an
             * unread tail: the next command would parse it as its own. */
            resp[used] = '\0';
            gw_ipc_conn_close(conn);
            return 1;
        }
        n = read(conn->fd, resp + used, resp_size - 1 - used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) {
            gw_ipc_conn_close(conn);
            return -1;
        }
        if (n == 0) {
            /* An older service closes the connection after the handler.
             * Preserve that contract without waiting for a timeout. */
            resp[used] = '\0';
            gw_ipc_conn_close(conn);
            return used ? 0 : -1;
        }
        used += (size_t)n;
        resp[used] = '\0';
        if (has_terminator(resp, used, terminator)) return 0;
    }
}

int gw_ipc_conn_cmd(gw_ipc_conn_t *conn, const char *cmd,
                    char *resp, size_t resp_size) {
    if (exchange(conn, cmd, resp, resp_size) == 0) return 0;
    if (gw_ipc_conn_open(conn) < 0) return -1;
    return exchange(conn, cmd, resp, resp_size);
}

int gw_ipc_conn_cmd_until(gw_ipc_conn_t *conn, const char *cmd,
                          const char *terminator,
                          char *resp, size_t resp_size) {
    int result = exchange_until(conn, cmd, terminator, resp, resp_size);
    if (result >= 0) return result;
    if (gw_ipc_conn_open(conn) < 0) return -1;
    return exchange_until(conn, cmd, terminator, resp, resp_size);
}

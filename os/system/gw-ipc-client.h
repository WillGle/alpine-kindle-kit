/* SPDX-License-Identifier: GPL-3.0-or-later
 * Small persistent client for the gw-system command socket.
 *
 * One connection carries many newline-delimited commands.  On any transport
 * error the connection is closed and the command is retried once on a fresh
 * connection, so a restarted service does not wedge the shell. */
#ifndef GW_IPC_CLIENT_H
#define GW_IPC_CLIENT_H

#include <stddef.h>

typedef struct {
    int fd;
} gw_ipc_conn_t;

void gw_ipc_conn_init(gw_ipc_conn_t *conn);
int gw_ipc_conn_open(gw_ipc_conn_t *conn);
void gw_ipc_conn_close(gw_ipc_conn_t *conn);
/* Send one command and read one newline-terminated response.  Reconnects and
 * retries once when the transport was closed.  Returns 0 on success. */
int gw_ipc_conn_cmd(gw_ipc_conn_t *conn, const char *cmd,
                    char *resp, size_t resp_size);

/* Send one command and read lines until a line equal to `terminator` arrives
 * (for example "APP END" or "NET END").  The service keeps the connection
 * open after a batch response, so EOF is not a terminator.  Reconnects and
 * retries once when the transport was closed.
 *
 * Returns 0 when the terminator was received (or an older one-shot service
 * closed cleanly after sending data), 1 when the response did not fit in
 * `resp`, and -1 on transport/argument error.  A truncated response closes
 * the connection so the next command cannot consume leftover data. */
int gw_ipc_conn_cmd_until(gw_ipc_conn_t *conn, const char *cmd,
                          const char *terminator,
                          char *resp, size_t resp_size);

#endif /* GW_IPC_CLIENT_H */

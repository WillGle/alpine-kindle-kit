/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host integration tests for the gw-system command socket:
 *   - several newline-delimited commands on one connection
 *   - idle timeout closes a persistent connection
 *   - reconnecting after a broken client works
 *   - one client's partial line cannot leak into another client
 *
 * The gw-system test build is compiled with GW_IPC_* paths pointing at files
 * in the current directory and GW_IPC_IDLE_SECONDS=1.
 */
#define _GNU_SOURCE
#include "gw-ipc.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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

#define CMD_SOCKET "test-ipc.cmd"
#define EVT_SOCKET "test-ipc.evt"
#define PID_FILE   "test-ipc.pid"

static int failures;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int connect_once(void) {
    struct sockaddr_un addr;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CMD_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int connect_wait(int seconds) {
    int i;
    for (i = 0; i < seconds * 20; i++) {
        int fd = connect_once();
        if (fd >= 0) return fd;
        usleep(50000);
    }
    return -1;
}

static int send_all(int fd, const char *text) {
    size_t len = strlen(text), sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, text + sent, len - sent);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

typedef struct {
    int fd;
    char buffer[2048];
    size_t used, pos;
} reader_t;

static void reader_init(reader_t *r, int fd) {
    r->fd = fd;
    r->used = r->pos = 0;
}

/* Read one newline-terminated response. Returns bytes, 0 on clean EOF, -1 on
 * timeout/error.  Data after the first newline stays buffered for the next
 * call, so several responses sent in one read are all preserved. */
static ssize_t reader_line(reader_t *r, char *out, size_t size) {
    for (;;) {
        char *newline;
        size_t available = r->used - r->pos;
        if (available) {
            newline = memchr(r->buffer + r->pos, '\n', available);
            if (newline) {
                size_t length = (size_t)(newline - (r->buffer + r->pos)) + 1;
                if (length >= size) length = size - 1;
                memcpy(out, r->buffer + r->pos, length);
                out[length] = '\0';
                r->pos += (size_t)(newline - (r->buffer + r->pos)) + 1;
                return (ssize_t)length;
            }
        }
        if (r->pos && r->pos == r->used) r->pos = r->used = 0;
        if (r->used == sizeof(r->buffer)) return -1;
        {
            struct pollfd pfd = { .fd = r->fd, .events = POLLIN };
            int ready = poll(&pfd, 1, 3000);
            ssize_t n;
            if (ready < 0 && errno == EINTR) continue;
            if (ready <= 0) return -1;
            n = read(r->fd, r->buffer + r->used,
                     sizeof(r->buffer) - r->used);
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) return -1;
            if (n == 0) return 0;
            r->used += (size_t)n;
        }
    }
}

static void stop_service(pid_t pid) {
    int status;
    kill(pid, SIGTERM);
    if (waitpid(pid, &status, 0) < 0) {
        /* Best effort. */
    }
}

int main(int argc, char **argv) {
    const char *binary = argc > 1 ? argv[1] : "./gw-system-test";
    pid_t pid;
    int fd, fd2, fd3;
    char line[GW_IPC_MAX_LINE];
    reader_t reader_a, reader_b, reader_c;

    unlink(CMD_SOCKET);
    unlink(EVT_SOCKET);
    unlink(PID_FILE);

    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        execl(binary, binary, (char *)NULL);
        _exit(127);
    }

    fd = connect_wait(5);
    CHECK(fd >= 0);
    if (fd < 0) { stop_service(pid); return 1; }
    reader_init(&reader_a, fd);

    /* Several commands over one socket, including QUIT. */
    CHECK(send_all(fd, "STATUS\nCAPS\nQUIT\n") == 0);
    CHECK(reader_line(&reader_a, line, sizeof(line)) > 0);
    CHECK(!strncmp(line, "STATUS ", 7));
    CHECK(reader_line(&reader_a, line, sizeof(line)) > 0);
    CHECK(!strncmp(line, "CAPS ", 5));
    CHECK(reader_line(&reader_a, line, sizeof(line)) == 0);   /* server closed after QUIT */
    close(fd);

    /* A broken client can be replaced by a fresh connection. */
    fd = connect_wait(2);
    CHECK(fd >= 0);
    if (fd >= 0) {
        reader_init(&reader_a, fd);
        CHECK(send_all(fd, "STATUS\n") == 0);
        CHECK(reader_line(&reader_a, line, sizeof(line)) > 0);
        CHECK(!strncmp(line, "STATUS ", 7));
        close(fd);   /* abrupt close */
    }
    fd = connect_wait(2);
    CHECK(fd >= 0);
    if (fd >= 0) {
        reader_init(&reader_a, fd);
        CHECK(send_all(fd, "CAPS\n") == 0);
        CHECK(reader_line(&reader_a, line, sizeof(line)) > 0);
        CHECK(!strncmp(line, "CAPS ", 5));
        close(fd);
    }

    /* One client's partial line must stay in that client's buffer. */
    fd = connect_wait(2);    /* A */
    fd2 = connect_wait(2);   /* B */
    CHECK(fd >= 0 && fd2 >= 0);
    if (fd >= 0 && fd2 >= 0) {
        reader_init(&reader_a, fd);
        reader_init(&reader_b, fd2);
        CHECK(send_all(fd, "STA") == 0);
        CHECK(send_all(fd2, "STATUS\n") == 0);
        CHECK(reader_line(&reader_b, line, sizeof(line)) > 0);
        CHECK(!strncmp(line, "STATUS ", 7));
        CHECK(send_all(fd, "TUS\n") == 0);
        CHECK(reader_line(&reader_a, line, sizeof(line)) > 0);
        CHECK(!strncmp(line, "STATUS ", 7));
    }
    if (fd >= 0) close(fd);
    if (fd2 >= 0) close(fd2);

    /* Idle timeout closes the connection (test build uses 1 second). */
    fd3 = connect_wait(2);
    CHECK(fd3 >= 0);
    if (fd3 >= 0) {
        reader_init(&reader_c, fd3);
        struct pollfd pfd = { .fd = fd3, .events = POLLIN };
        int ready = poll(&pfd, 1, 4000);
        ssize_t n;
        CHECK(ready > 0);
        n = read(fd3, line, sizeof(line));
        CHECK(n == 0);
        close(fd3);
    }

    stop_service(pid);
    unlink(CMD_SOCKET);
    unlink(EVT_SOCKET);
    unlink(PID_FILE);

    if (failures) {
        printf("IPC service integration tests: %d failures\n", failures);
        return 1;
    }
    printf("IPC service integration tests: PASS\n");
    return 0;
}

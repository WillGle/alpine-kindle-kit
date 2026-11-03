/* SPDX-License-Identifier: GPL-3.0-or-later
 * gw-agent: thin Kindle-side client for a kindle-hub.
 *
 * The agent owns the network connection, fetches raw 8 bpp frames and writes
 * them to a tmpfs file.  It never opens /dev/fb0: gw-shell is the only process
 * that applies a frame to the panel.  Touch and key events travel the other
 * way over a private Unix socket.
 *
 * Transport is HTTP/1.0 with a bearer token.  TLS is terminated by the hub;
 * the device uses a reverse SSH tunnel or a trusted LAN.  There is no shell
 * execution, no arbitrary path from the network, and no token in logs.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "gw-hub.h"

#define AGENT_VERSION "0.1.0"
#define HTTP_TIMEOUT_SECONDS 8
#define CONFIG_MAX 4096
#define HEADER_MAX 8192

typedef struct {
    char host[GW_HUB_HOST_MAX];
    int port;
    char token[GW_HUB_TOKEN_MAX];
    char device_id[GW_HUB_ID_MAX];
    int poll_ms;
} hub_config_t;

typedef struct {
    int fd;
    unsigned char buf[1024];
    size_t pos;
    size_t len;
} hub_reader_t;

static volatile sig_atomic_t g_stop;
static uint32_t g_event_seq;
static int g_view_active;
static time_t g_next_frame;
/* Persistent UI receive buffer: a line can span multiple recv() calls. */
static char g_ui_rx[2048];
static size_t g_ui_rx_used;

static void on_stop(int sig) {
    (void)sig;
    g_stop = 1;
}

static char *trim(char *text) {
    char *end;
    while (*text && isspace((unsigned char)*text)) text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
    return text;
}

static int valid_token(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!text[0]) return 0;
    for (; *p; p++) {
        if (!isalnum(*p) && !strchr("._~+/=-", *p)) return 0;
    }
    return 1;
}

static int valid_id(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!text[0]) return 0;
    for (; *p; p++) {
        if (!isalnum(*p) && *p != '.' && *p != '_' && *p != '-') return 0;
    }
    return 1;
}

static int valid_host(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!text[0]) return 0;
    for (; *p; p++) {
        if (!isalnum(*p) && !strchr(".:_-", *p)) return 0;
    }
    return 1;
}

static int read_config(const char *path, hub_config_t *cfg) {
    int fd;
    FILE *file;
    char line[256];
    struct stat st;

    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->host, sizeof(cfg->host), "%s", "127.0.0.1");
    cfg->port = 8080;
    cfg->poll_ms = 2000;
    snprintf(cfg->device_id, sizeof(cfg->device_id), "%s", "pw3");

    if (lstat(path, &st) != 0) return -1;
    if (!S_ISREG(st.st_mode) || st.st_size > CONFIG_MAX) return -1;
    fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    file = fdopen(fd, "r");
    if (!file) {
        close(fd);
        return -1;
    }
    while (fgets(line, sizeof(line), file)) {
        char *key = trim(line);
        char *value;
        if (!key[0] || key[0] == '#') continue;
        value = strchr(key, '=');
        if (!value) continue;
        *value++ = '\0';
        value = trim(value);
        key = trim(key);
        if (!strcmp(key, "host")) {
            if (!valid_host(value) || strlen(value) >= sizeof(cfg->host)) goto bad;
            snprintf(cfg->host, sizeof(cfg->host), "%s", value);
        } else if (!strcmp(key, "port")) {
            char *end = NULL;
            long port = strtol(value, &end, 10);
            if (!end || *end || port < 1 || port > 65535) goto bad;
            cfg->port = (int)port;
        } else if (!strcmp(key, "token")) {
            if (!valid_token(value) || strlen(value) >= sizeof(cfg->token)) goto bad;
            snprintf(cfg->token, sizeof(cfg->token), "%s", value);
        } else if (!strcmp(key, "device_id")) {
            if (!valid_id(value) || strlen(value) >= sizeof(cfg->device_id)) goto bad;
            snprintf(cfg->device_id, sizeof(cfg->device_id), "%s", value);
        } else if (!strcmp(key, "poll_ms")) {
            char *end = NULL;
            long ms = strtol(value, &end, 10);
            if (!end || *end || ms < 250 || ms > 600000) goto bad;
            cfg->poll_ms = (int)ms;
        }
        /* Unknown keys are ignored so the hub can add hints without
         * breaking an older device. */
    }
    fclose(file);
    if (!cfg->token[0]) return -1;
    return 0;
bad:
    fclose(file);
    return -1;
}

static int reader_fill(hub_reader_t *reader) {
    ssize_t got;
    if (reader->pos < reader->len) return 1;
    do {
        got = recv(reader->fd, reader->buf, sizeof(reader->buf), 0);
    } while (got < 0 && errno == EINTR);
    if (got <= 0) return -1;
    reader->pos = 0;
    reader->len = (size_t)got;
    return 1;
}

static int reader_getc(hub_reader_t *reader) {
    if (reader_fill(reader) < 0) return -1;
    return reader->buf[reader->pos++];
}

static int reader_line(hub_reader_t *reader, char *out, size_t cap) {
    size_t used = 0;
    for (;;) {
        int ch = reader_getc(reader);
        if (ch < 0) return -1;
        if (ch == '\n') break;
        if (ch == '\r') continue;
        if (used + 1 >= cap) return -1;
        out[used++] = (char)ch;
    }
    out[used] = '\0';
    return 0;
}

static int reader_exact(hub_reader_t *reader, unsigned char *out, size_t len) {
    while (len > 0) {
        size_t avail;
        if (reader_fill(reader) < 0) return -1;
        avail = reader->len - reader->pos;
        if (avail > len) avail = len;
        memcpy(out, reader->buf + reader->pos, avail);
        reader->pos += avail;
        out += avail;
        len -= avail;
    }
    return 0;
}

static int send_all(int fd, const void *data, size_t len) {
    const unsigned char *p = data;
    while (len > 0) {
        ssize_t sent = send(fd, p, len, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (sent == 0) return -1;
        p += (size_t)sent;
        len -= (size_t)sent;
    }
    return 0;
}

static int connect_host(const hub_config_t *cfg) {
    struct addrinfo hints;
    struct addrinfo *list = NULL;
    struct addrinfo *item;
    struct timeval timeout;
    char service[16];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%d", cfg->port);
    if (getaddrinfo(cfg->host, service, &hints, &list) != 0) return -1;

    timeout.tv_sec = HTTP_TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    for (item = list; item; item = item->ai_next) {
        fd = socket(item->ai_family, item->ai_socktype | SOCK_CLOEXEC,
                    item->ai_protocol);
        if (fd < 0) continue;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (connect(fd, item->ai_addr, item->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(list);
    return fd;
}

static int http_status(hub_reader_t *reader, int *status) {
    char line[512];
    if (reader_line(reader, line, sizeof(line)) < 0) return -1;
    if (sscanf(line, "HTTP/%*d.%*d %d", status) != 1) return -1;
    return 0;
}

static int http_headers(hub_reader_t *reader, long *content_length,
                        char *seq, size_t seq_cap, char *mode, size_t mode_cap,
                        char *rect, size_t rect_cap) {
    char line[512];
    size_t total = 0;

    *content_length = -1;
    for (;;) {
        if (reader_line(reader, line, sizeof(line)) < 0) return -1;
        total += strlen(line) + 2;
        if (total > HEADER_MAX) return -1;
        if (!line[0]) return 0;
        {
            char *colon = strchr(line, ':');
            char *value;
            if (!colon) continue;
            *colon = '\0';
            value = trim(colon + 1);
            if (!strcasecmp(line, "Content-Length")) {
                char *end = NULL;
                long length = strtol(value, &end, 10);
                if (!end || *end || length < 0) return -1;
                *content_length = length;
            } else if (!strcasecmp(line, "Transfer-Encoding")) {
                return -1;
            } else if (!strcasecmp(line, "X-GW-Seq")) {
                snprintf(seq, seq_cap, "%s", value);
            } else if (!strcasecmp(line, "X-GW-Mode")) {
                snprintf(mode, mode_cap, "%s", value);
            } else if (!strcasecmp(line, "X-GW-Rect")) {
                snprintf(rect, rect_cap, "%s", value);
            }
        }
    }
}

/* 1 = frame read, 0 = no new frame, -1 = error. */
static int http_get_frame(const hub_config_t *cfg, uint32_t last_seq,
                          unsigned char *frame, size_t cap, uint32_t *out_seq,
                          char *mode, size_t mode_cap,
                          char *rect, size_t rect_cap) {
    char request[1024];
    char seq_text[32] = "";
    long content_length = -1;
    hub_reader_t reader;
    int fd;
    int status = 0;
    int length;

    length = snprintf(request, sizeof(request),
                      "GET /v1/frame HTTP/1.0\r\n"
                      "Host: %s:%d\r\n"
                      "Authorization: Bearer %s\r\n"
                      "X-GW-Seq: %u\r\n"
                      "Accept: image/x-gw-raw\r\n"
                      "Connection: close\r\n\r\n",
                      cfg->host, cfg->port, cfg->token, last_seq);
    if (length <= 0 || (size_t)length >= sizeof(request)) return -1;

    fd = connect_host(cfg);
    if (fd < 0) return -1;
    if (send_all(fd, request, (size_t)length) < 0) {
        close(fd);
        return -1;
    }
    reader.fd = fd;
    reader.pos = reader.len = 0;

    if (http_status(&reader, &status) < 0) goto bad;
    if (http_headers(&reader, &content_length, seq_text, sizeof(seq_text),
                     mode, mode_cap, rect, rect_cap) < 0) goto bad;
    if (status == 204) {
        close(fd);
        return 0;
    }
    if (status != 200) goto bad;
    if (content_length != (long)cap) goto bad;
    if (!seq_text[0]) goto bad;
    if (reader_exact(&reader, frame, cap) < 0) goto bad;
    *out_seq = (uint32_t)strtoul(seq_text, NULL, 10);
    close(fd);
    return 1;
bad:
    close(fd);
    return -1;
}

static int http_post(const hub_config_t *cfg, const char *path,
                     const char *body) {
    char header[1024];
    char line[512];
    hub_reader_t reader;
    int fd;
    int status = 0;
    int length;

    length = snprintf(header, sizeof(header),
                      "POST %s HTTP/1.0\r\n"
                      "Host: %s:%d\r\n"
                      "Authorization: Bearer %s\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n\r\n",
                      path, cfg->host, cfg->port, cfg->token, strlen(body));
    if (length <= 0 || (size_t)length >= sizeof(header)) return -1;

    fd = connect_host(cfg);
    if (fd < 0) return -1;
    if (send_all(fd, header, (size_t)length) < 0 ||
        send_all(fd, body, strlen(body)) < 0) {
        close(fd);
        return -1;
    }
    reader.fd = fd;
    reader.pos = reader.len = 0;
    if (http_status(&reader, &status) < 0) {
        close(fd);
        return -1;
    }
    while (reader_line(&reader, line, sizeof(line)) == 0 && line[0]) {
        /* Drain bounded headers. */
    }
    close(fd);
    return (status >= 200 && status < 300) ? 0 : -1;
}

static int write_frame(const char *path, const unsigned char *data,
                       size_t len) {
    char dir[GW_HUB_PATH_MAX];
    char tmp[GW_HUB_PATH_MAX];
    char *slash;
    int fd;
    size_t done = 0;

    if (strlen(path) >= sizeof(dir)) return -1;
    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (dir[0] && mkdir(dir, 0700) != 0 && errno != EEXIST) return -1;
    }
    if (strlen(path) + 5 >= sizeof(tmp)) return -1;
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
              0600);
    if (fd < 0) return -1;
    while (done < len) {
        ssize_t wrote = write(fd, data + done, len - done);
        if (wrote < 0) {
            if (errno == EINTR) continue;
            close(fd);
            unlink(tmp);
            return -1;
        }
        if (wrote == 0) {
            close(fd);
            unlink(tmp);
            return -1;
        }
        done += (size_t)wrote;
    }
    if (fsync(fd) != 0) {
        close(fd);
        unlink(tmp);
        return -1;
    }
    if (close(fd) != 0) {
        unlink(tmp);
        return -1;
    }
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

static int ui_connect(const char *path) {
    struct sockaddr_un addr;
    int fd;

    if (strlen(path) >= sizeof(addr.sun_path)) return -1;
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int ui_send(int *fd, const char *path, const char *message) {
    if (*fd < 0) *fd = ui_connect(path);
    if (*fd < 0) return -1;
    if (send_all(*fd, message, strlen(message)) < 0) {
        close(*fd);
        *fd = -1;
        return -1;
    }
    return 0;
}

static int handle_ui_line(const hub_config_t *cfg, const char *line) {
    char action[8];
    char key[16];
    int x, y;

    if (sscanf(line, "TOUCH %7s %d %d", action, &x, &y) == 3) {
        char body[192];
        if (strcmp(action, "down") && strcmp(action, "move") &&
            strcmp(action, "up")) return -1;
        if (x < 0 || x > 65535 || y < 0 || y > 65535) return -1;
        snprintf(body, sizeof(body),
                 "{\"seq\":%u,\"type\":\"touch\",\"action\":\"%s\","
                 "\"x\":%d,\"y\":%d}",
                 ++g_event_seq, action, x, y);
        return http_post(cfg, "/v1/events", body);
    }
    if (sscanf(line, "KEY %15s", key) == 1) {
        char body[128];
        if (strcmp(key, "back") && strcmp(key, "home") &&
            strcmp(key, "page_next") && strcmp(key, "page_prev") &&
            strcmp(key, "menu")) return -1;
        snprintf(body, sizeof(body),
                 "{\"seq\":%u,\"type\":\"key\",\"key\":\"%s\"}",
                 ++g_event_seq, key);
        return http_post(cfg, "/v1/events", body);
    }
    /* VIEW is a pacing hint: idle means the Hub screen is not visible, so
     * the agent stops fetching frames and lets the device sleep. */
    if (!strncmp(line, "VIEW ", 5)) {
        g_view_active = !strcmp(line + 5, "active");
        if (g_view_active) g_next_frame = 0;
    }
    return 0;
}

static double read_uptime(void) {
    FILE *file = fopen("/proc/uptime", "r");
    double seconds = 0;
    if (!file) return 0;
    if (fscanf(file, "%lf", &seconds) != 1) seconds = 0;
    fclose(file);
    return seconds;
}

static int post_status(const hub_config_t *cfg) {
    char body[512];
    snprintf(body, sizeof(body),
             "{\"device_id\":\"%s\",\"uptime_s\":%ld,\"agent\":\"%s\","
             "\"screen\":{\"w\":%d,\"h\":%d,\"bpp\":%d}}",
             cfg->device_id, (long)read_uptime(), AGENT_VERSION,
             GW_HUB_W, GW_HUB_H, GW_HUB_BPP);
    return http_post(cfg, "/v1/status", body);
}

static int fetch_and_display(const hub_config_t *cfg, int *ui_fd,
                             const char *socket_path, const char *frame_path,
                             uint32_t *seq, int no_ui) {
    unsigned char *frame = malloc(GW_HUB_FRAME_MAX);
    char mode[16] = "full";
    char rect[32] = "-";
    uint32_t next_seq = *seq;
    int result;

    if (!frame) return -1;
    result = http_get_frame(cfg, *seq, frame, GW_HUB_FRAME_MAX, &next_seq,
                            mode, sizeof(mode), rect, sizeof(rect));
    if (result > 0) {
        char message[GW_HUB_LINE_MAX];
        if (write_frame(frame_path, frame, GW_HUB_FRAME_MAX) < 0) {
            free(frame);
            return -1;
        }
        snprintf(message, sizeof(message), "FRAME %u %d %d %s %s %s\n",
                 next_seq, GW_HUB_W, GW_HUB_H, mode, rect, frame_path);
        if (!no_ui) (void)ui_send(ui_fd, socket_path, message);
        *seq = next_seq;
    }
    free(frame);
    return result;
}

static void usage(FILE *out) {
    fprintf(out,
            "usage: gw-agent [--config PATH] [--socket PATH] [--frame PATH]\n"
            "                [--once] [--no-ui]\n");
}

int main(int argc, char **argv) {
    const char *config_path = GW_HUB_CONFIG;
    const char *socket_path = GW_HUB_SOCKET;
    const char *frame_path = GW_HUB_FRAME;
    hub_config_t cfg;
    int once = 0;
    int no_ui = 0;
    int ui_fd = -1;
    uint32_t seq = 0;
    time_t next_status;
    int wait_ms;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (!strcmp(argv[i], "--socket") && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (!strcmp(argv[i], "--frame") && i + 1 < argc) {
            frame_path = argv[++i];
        } else if (!strcmp(argv[i], "--once")) {
            once = 1;
        } else if (!strcmp(argv[i], "--no-ui")) {
            no_ui = 1;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(stdout);
            return 0;
        } else {
            usage(stderr);
            return 2;
        }
    }

    signal(SIGTERM, on_stop);
    signal(SIGINT, on_stop);

    if (read_config(config_path, &cfg) < 0 &&
        !strcmp(config_path, GW_HUB_CONFIG) &&
        read_config(GW_HUB_CONFIG_FALLBACK, &cfg) < 0) {
        if (once) {
            fprintf(stderr, "gw-agent: no usable hub config\n");
            return 1;
        }
        /* The service may be enabled before pairing.  Wait for a config
         * instead of exiting into an OpenRC restart loop. */
        while (!g_stop) {
            sleep(30);
            if (read_config(config_path, &cfg) == 0) break;
            if (!strcmp(config_path, GW_HUB_CONFIG) &&
                read_config(GW_HUB_CONFIG_FALLBACK, &cfg) == 0) break;
        }
        if (g_stop) return 0;
    }

    if (once) {
        if (fetch_and_display(&cfg, &ui_fd, socket_path, frame_path, &seq,
                              no_ui) < 0) {
            fprintf(stderr, "gw-agent: frame fetch failed\n");
            return 1;
        }
        (void)post_status(&cfg);
        if (ui_fd >= 0) close(ui_fd);
        return 0;
    }

    g_next_frame = 0;
    next_status = time(NULL);
    while (!g_stop) {
        wait_ms = g_view_active ? 50 : 1000;
        if (!no_ui) {
            if (ui_fd < 0) ui_fd = ui_connect(socket_path);
            if (ui_fd >= 0) {
                struct pollfd pfd;
                pfd.fd = ui_fd;
                pfd.events = POLLIN;
                pfd.revents = 0;
                if (poll(&pfd, 1, wait_ms) > 0) {
                    if (pfd.revents & (POLLHUP | POLLERR)) {
                        close(ui_fd);
                        ui_fd = -1;
                        g_view_active = 0;
                        g_ui_rx_used = 0;
                    } else if (pfd.revents & POLLIN) {
                        ssize_t got = recv(ui_fd, g_ui_rx + g_ui_rx_used,
                                           sizeof(g_ui_rx) - g_ui_rx_used - 1, 0);
                        if (got > 0) {
                            char *line = g_ui_rx;
                            char *newline;
                            size_t tail;
                            g_ui_rx_used += (size_t)got;
                            g_ui_rx[g_ui_rx_used] = '\0';
                            while ((newline = strchr(line, '\n')) != NULL) {
                                *newline = '\0';
                                if (line[0]) (void)handle_ui_line(&cfg, line);
                                line = newline + 1;
                            }
                            /* Keep an unterminated tail for the next read;
                             * a small AF_UNIX write is not guaranteed to
                             * arrive as one complete line. */
                            tail = (size_t)(line - g_ui_rx);
                            if (tail) {
                                memmove(g_ui_rx, line, g_ui_rx_used - tail);
                                g_ui_rx_used -= tail;
                            }
                            if (g_ui_rx_used >= sizeof(g_ui_rx) - 1)
                                g_ui_rx_used = 0;
                        } else if (got == 0) {
                            close(ui_fd);
                            ui_fd = -1;
                            g_view_active = 0;
                            g_ui_rx_used = 0;
                        }
                    }
                }
            }
        } else {
            poll(NULL, 0, wait_ms);
        }

        if (g_view_active && time(NULL) >= g_next_frame) {
            (void)fetch_and_display(&cfg, &ui_fd, socket_path, frame_path,
                                    &seq, no_ui);
            g_next_frame = time(NULL) + (cfg.poll_ms + 999) / 1000;
        }
        if (time(NULL) >= next_status) {
            (void)post_status(&cfg);
            next_status = time(NULL) + 60;
        }
    }
    if (ui_fd >= 0) close(ui_fd);
    return 0;
}

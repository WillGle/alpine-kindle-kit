/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shell side of the Kindle hub link.
 *
 * The agent writes raw 8 bpp frames into /run/gw-hub/frame.raw and sends a
 * short FRAME line.  This file copies the frame into an LVGL image descriptor
 * and forwards touch/key input back to the agent.  It never opens /dev/fb0;
 * the normal LVGL/EPDC path owns the panel.
 */
#include "gw_shell_internal.h"
#include "../system/gw-hub.h"
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_hub_listen = -1;
static int g_hub_client = -1;
static char g_hub_rx[GW_HUB_LINE_MAX * 2];
static size_t g_hub_rx_used;
static uint8_t *g_hub_pixels;
static lv_img_dsc_t g_hub_dsc;
static uint32_t g_hub_seq;
static int g_hub_have_frame;
static int g_hub_view_active;

static int hub_send(const char *message) {
    if (g_hub_client < 0) return -1;
    if (send(g_hub_client, message, strlen(message), MSG_NOSIGNAL) < 0) {
        close(g_hub_client);
        g_hub_client = -1;
        return -1;
    }
    return 0;
}

static int read_full(int fd, uint8_t *out, size_t len) {
    size_t done = 0;
    while (done < len) {
        ssize_t got = read(fd, out + done, len - done);
        if (got < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (got == 0) return -1;
        done += (size_t)got;
    }
    return 0;
}

static int safe_frame_path(const char *path) {
    const char *prefix = GW_HUB_DIR "/";
    if (strncmp(path, prefix, strlen(prefix)) != 0) return 0;
    if (strstr(path, "..")) return 0;
    return 1;
}

static void hub_apply_frame(const char *line) {
    unsigned seq = 0;
    int width = 0, height = 0;
    char mode[16] = "full";
    char rect[32] = "-";
    char path[GW_HUB_PATH_MAX];
    int fd;

    if (sscanf(line, "FRAME %u %d %d %15s %31s %127s",
               &seq, &width, &height, mode, rect, path) != 6) return;
    if (width != GW_HUB_W || height != GW_HUB_H) return;
    if (!safe_frame_path(path)) return;
    if (g_hub_have_frame && seq == g_hub_seq) return;
    fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return;
    {
        struct stat st;
        if (fstat(fd, &st) != 0 || st.st_size != GW_HUB_FRAME_MAX) {
            close(fd);
            return;
        }
    }
    if (read_full(fd, g_hub_pixels, GW_HUB_FRAME_MAX) < 0) {
        close(fd);
        return;
    }
    close(fd);
    /* The agent sends raw Y8; LVGL draws RGB332. */
    gw_y8_to_rgb332_buffer(g_hub_pixels, GW_HUB_FRAME_MAX);
    g_hub_seq = seq;
    g_hub_have_frame = 1;
    if (g_hub_img) {
        lv_img_set_src(g_hub_img, &g_hub_dsc);
        lv_obj_set_size(g_hub_img, GW_HUB_W, GW_HUB_H);
        lv_obj_invalidate(g_hub_img);
    }
    if (g_hub_status) container_set_hidden(g_hub_status, 1);
}

static void hub_apply_line(const char *line) {
    if (!strncmp(line, GW_HUB_MSG_FRAME " ", 6)) {
        hub_apply_frame(line);
    } else if (!strcmp(line, GW_HUB_MSG_CLEAR)) {
        g_hub_have_frame = 0;
        if (g_hub_img) lv_img_set_src(g_hub_img, NULL);
        if (g_hub_status) {
            container_set_hidden(g_hub_status, 0);
            label_set_if_changed(g_hub_status, GW_STR_HUB_WAITING);
        }
    } else if (!strncmp(line, GW_HUB_MSG_STATUS " ", 7)) {
        if (g_hub_status) label_set_if_changed(g_hub_status, line + 7);
    }
}

static void hub_poll(lv_timer_t *timer) {
    (void)timer;
#if GW_UI_HOST_SIM
    return;
#else
    char buffer[512];

    if (g_hub_client < 0 && g_hub_listen >= 0) {
        int fd = accept4(g_hub_listen, NULL, NULL,
                         SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd >= 0) {
            g_hub_client = fd;
            g_hub_rx_used = 0;
            (void)hub_send(g_hub_view_active ? "VIEW active\n"
                                             : "VIEW idle\n");
        }
    }
    if (g_hub_client < 0) return;

    for (;;) {
        ssize_t got = recv(g_hub_client, buffer, sizeof(buffer), 0);
        char *line;
        if (got < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close(g_hub_client);
            g_hub_client = -1;
            g_hub_rx_used = 0;
            return;
        }
        if (got == 0) {
            close(g_hub_client);
            g_hub_client = -1;
            g_hub_rx_used = 0;
            return;
        }
        if (g_hub_rx_used + (size_t)got >= sizeof(g_hub_rx)) {
            /* A line that cannot fit is a protocol error, not a reason to
             * keep buffering.  Drop the connection; the agent reconnects. */
            close(g_hub_client);
            g_hub_client = -1;
            g_hub_rx_used = 0;
            return;
        }
        memcpy(g_hub_rx + g_hub_rx_used, buffer, (size_t)got);
        g_hub_rx_used += (size_t)got;
        g_hub_rx[g_hub_rx_used] = '\0';
        while ((line = strchr(g_hub_rx, '\n')) != NULL) {
            *line = '\0';
            if (line > g_hub_rx) hub_apply_line(g_hub_rx);
            memmove(g_hub_rx, line + 1, strlen(line + 1) + 1);
            g_hub_rx_used = strlen(g_hub_rx);
        }
    }
#endif
}

GW_SHELL_INTERNAL void gw_hub_attach(lv_obj_t *img, lv_obj_t *status) {
    g_hub_img = img;
    g_hub_status = status;
    if (g_hub_pixels && g_hub_have_frame) {
        lv_img_set_src(g_hub_img, &g_hub_dsc);
        lv_obj_set_size(g_hub_img, GW_HUB_W, GW_HUB_H);
        if (g_hub_status) container_set_hidden(g_hub_status, 1);
    }
}

GW_SHELL_INTERNAL void gw_hub_set_view(int active) {
    g_hub_view_active = active;
    (void)hub_send(active ? "VIEW active\n" : "VIEW idle\n");
#if !GW_UI_HOST_SIM
    /* Fixed path, fixed arguments: the Hub screen owns the on-demand
     * Tailscale lifecycle.  No network data reaches this exec.  Never wait:
     * the render loop must keep handling touch and paint while the daemon
     * starts or stops.  gw-shell reaps the child in its main loop. */
    {
        pid_t pid = fork();
        if (pid == 0) {
            int fd;
            (void)setsid();
            fd = open("/dev/null", O_RDWR);
            if (fd >= 0) {
                (void)dup2(fd, STDIN_FILENO);
                (void)dup2(fd, STDOUT_FILENO);
                (void)dup2(fd, STDERR_FILENO);
                if (fd > STDERR_FILENO) close(fd);
            }
            execl("/usr/sbin/gw-tailscale", "gw-tailscale",
                  active ? "on" : "off", (char *)NULL);
            _exit(127);
        }
    }
#endif
}

GW_SHELL_INTERNAL void gw_hub_send_touch(const char *action, int x, int y) {
    char message[64];
    snprintf(message, sizeof(message), "TOUCH %s %d %d\n", action, x, y);
    (void)hub_send(message);
}

GW_SHELL_INTERNAL void gw_hub_send_key(const char *key) {
    char message[48];
    snprintf(message, sizeof(message), "KEY %s\n", key);
    (void)hub_send(message);
}

GW_SHELL_INTERNAL int gw_hub_has_frame(void) {
    return g_hub_have_frame;
}

GW_SHELL_INTERNAL void gw_hub_init(void) {
    if (!g_hub_pixels) {
        g_hub_pixels = calloc(1, GW_HUB_FRAME_MAX);
        if (!g_hub_pixels) return;
        g_hub_dsc.header.always_zero = 0;
        g_hub_dsc.header.w = GW_HUB_W;
        g_hub_dsc.header.h = GW_HUB_H;
        g_hub_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        g_hub_dsc.data_size = GW_HUB_FRAME_MAX;
        g_hub_dsc.data = g_hub_pixels;
    }
#if !GW_UI_HOST_SIM
    if (g_hub_listen < 0) {
        struct sockaddr_un addr;
        if (mkdir(GW_HUB_DIR, 0700) == 0 || errno == EEXIST) {
            g_hub_listen = socket(AF_UNIX,
                                  SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                                  0);
            if (g_hub_listen >= 0) {
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                snprintf(addr.sun_path, sizeof(addr.sun_path), "%s",
                         GW_HUB_SOCKET);
                unlink(GW_HUB_SOCKET);
                if (bind(g_hub_listen, (struct sockaddr *)&addr,
                         sizeof(addr)) != 0 ||
                    chmod(GW_HUB_SOCKET, 0600) != 0 ||
                    listen(g_hub_listen, 1) != 0) {
                    close(g_hub_listen);
                    g_hub_listen = -1;
                }
            }
        }
    }
#endif
    lv_timer_create(hub_poll, 50, NULL);
}

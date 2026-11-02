/* SPDX-License-Identifier: GPL-3.0-or-later
 * Gleworks system service — PW3 hardware-contract stub.
 *
 * This is a prospective hardware owner; it is not integrated with the
 * current reader and must not be run as a replacement for it yet.
 *
 * CURRENT STATUS (2026-10-30): stub skeleton. Hardware paths are present but
 * disabled pending real-device acceptance of each subsystem. The service
 * starts, creates the sockets, reads the power-button event node and responds
 * to read-only queries. Mutating commands are disabled by default; the light
 * spike flag is for temporary probes and does not establish acceptance.
 *
 * STATUS: CORE (in use).
 *
 * Build: arm-linux-musleabihf-gcc -std=c99 -Wall -Wextra -Os \
 *          -o gw-system gw-system.c
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "gw-ipc.h"
#include "gw-command-policy.h"
#include "gw-power.h"
#include "gw-power-policy.h"
#include "gw-network.h"
#include "gw-stage3.h"
#include "gw-apps.h"

#ifndef GW_WIFI_ACCEPTED
#define GW_WIFI_ACCEPTED 0
#endif

#ifndef GW_TIME_CONFIG_PATH
#define GW_TIME_CONFIG_PATH "/var/lib/gleworks/time.conf"
#endif
#ifndef GW_LIGHT_CONFIG_PATH
#define GW_LIGHT_CONFIG_PATH "/var/lib/gleworks/light.conf"
#endif
#ifndef GW_WIFI_ENABLED_PATH
#define GW_WIFI_ENABLED_PATH "/var/lib/gleworks/wifi-enabled.conf"
#endif

/* ── Forward declarations ────────────────────────────────────────────────── */

static void service_loop(int cmd_fd, int evt_fd);
static int  bind_socket(const char *path);
static int  handle_command(int client_fd, uid_t peer_uid, pid_t peer_pid,
                           const char *line, size_t len);
static int  write_line(int fd, const char *msg);
static void event_broadcast(const char *msg);
static void event_clients_init(void);
static void cleanup(void);
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
static pid_t start_wifi(const char *action);
#endif

/* ── Globals ─────────────────────────────────────────────────────────────── */

static volatile sig_atomic_t g_quit = 0;
static int g_cmd_fd = -1;
static int g_evt_fd = -1;
static int g_lock_fd = -1;

/* Power-button input device; probed at start. */
static int g_btn_fd = -1;

/* SIGCHLD self-pipe: child exit must wake service_loop immediately instead of
 * waiting for the one-second poll timeout (reader exit -> Home, shell crash). */
static int g_sigchld_fd = -1;
static int g_sigchld_wr = -1;

/* Current state — updated by hardware probes, not assumed. */
static int g_light_pct    = -1;  /* -1: not yet read */
static int g_battery_pct  = -1;  /* -1: not yet read */
static int g_charging      = -1;
static int g_net_connected = -1;
static char g_foreground_app[64] = "";   /* empty = shell foreground */
static char g_boot_id[37]        = "";   /* read from /proc */

/* Stage 3 state. */
static char g_timezone[GW_TIME_TZ_MAX] = "UTC";
static int  g_time_format = GW_TIME_FORMAT_24;
static int  g_time_sync_pending = 0;
static pid_t g_time_sync_pid = -1;
static int  g_light_target = -1;
static int  g_wifi_enabled = 0;
static gw_wifi_state_t g_wifi_state = GW_WIFI_STATE_OFF;

#ifndef GW_LIFECYCLE_SPIKE
#define GW_LIFECYCLE_SPIKE 1
#endif

/* Event listeners (service -> UI/app). Fixed-size, no dynamic allocation.
 * These exist regardless of the lifecycle build flag because the socket
 * protocol is always available. */
static int g_evt_clients[GW_IPC_MAX_CLIENTS];
static int g_evt_clients_ready = 0;

#if GW_LIFECYCLE_SPIKE
static int g_supervise = 0;
static int g_pending_reader = 0;
static time_t g_pending_deadline;
static int g_last_reader_status = 0;
static int g_shell_failures = 0;
static int g_locked = 0;
static int g_display_on = 1;
static int g_last_light = -1;
static int g_button_pressed = 0;
static struct timespec g_button_down;
static pid_t g_shell_pid = -1;
static pid_t g_reader_pid = -1;
static pid_t g_wifi_pid = -1;
static int g_wifi_error = 0;

/* Power/suspend state. g_sleep_at is zero when no suspend is scheduled. */
static int g_suspended = 0;
static struct timespec g_sleep_at = { 0, 0 };
static int g_usb_state = -2;       /* -2: not sampled yet */
static int g_charging_state = -2;
static struct timespec g_power_poll_at = { 0, 0 };

/* Wi-Fi link supervision: retry with bounded backoff while enabled. */
static struct timespec g_wifi_poll_at = { 0, 0 };
static struct timespec g_wifi_retry_at = { 0, 0 };
static int g_wifi_retry_delay = 5;

/* Fixed paths only: socket clients cannot provide executables or shell text. */
static pid_t start_child(const char *path, int reader) {
    pid_t pid = fork();
    if (pid == 0) {
        if (reader) setenv("GW_MANAGED_READER", "1", 1);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    return pid;
}

/* Display power state. The panel keeps its last image; the frontlight is the
 * only illumination, so a power press turns it off and the next press restores
 * it. Lock state stays set across wake so the Unlock button is still required. */
#define GW_DISPLAY_DEFAULT_LIGHT 13

static void display_light(int pct) {
#if GW_POWER_LIGHT_ACCEPTED || GW_ENABLE_HW_TRIAL
    if (gw_power_set_light(pct) < 0)
        dprintf(2, "GW_SYSTEM: frontlight write failed: %s\n", strerror(errno));
#endif
    g_light_pct = pct;
    if (pct > 0) {
#if GW_LIFECYCLE_SPIKE
        g_last_light = pct;
#endif
        if (gw_config_save_int(GW_LIGHT_CONFIG_PATH, "brightness", pct) < 0)
            dprintf(2, "GW_SYSTEM: brightness persist failed\n");
    }
}

static void display_off(void) {
    int now = gw_power_read_light();
    if (now > 0) g_last_light = now;
    if (g_last_light <= 0) g_last_light = GW_DISPLAY_DEFAULT_LIGHT;
    display_light(0);
    g_display_on = 0;
    g_locked = 1;
    g_pending_reader = 0;
    event_broadcast(GW_EVT_PWR_LOCK "\n");
    dprintf(2, "GW_SYSTEM: display off (saved light=%d)\n", g_last_light);
}

static void display_wake(void) {
    int level = g_last_light > 0 ? g_last_light : GW_DISPLAY_DEFAULT_LIGHT;
    display_light(level);
    g_display_on = 1;
    event_broadcast(GW_EVT_PWR_WAKE "\n");
    dprintf(2, "GW_SYSTEM: display on (light=%d lock=%s)\n",
            level, g_locked ? "locked" : "unlocked");
}

/* ── Suspend orchestration ───────────────────────────────────────────────── */

/* Fallback wake in case the button/USB wake source never arrives. It is
 * bounded and always cleared after a normal resume. */
#define GW_SUSPEND_FALLBACK_SECONDS (15 * 60)
/* Diagnostic override, bounded; normal boot uses the constant above. */
static int g_suspend_fallback = GW_SUSPEND_FALLBACK_SECONDS;
/* Let the EPDC finish the lock-screen refresh before cutting the panel. */
#define GW_SUSPEND_DELAY_MS 1500
/* If an RTC wake happens without a button, allow a short inspection window
 * before going back to sleep. */
#define GW_SUSPEND_RETRY_MS 30000

static long gw_ms_until(const struct timespec *when) {
    struct timespec now;
    long ms;
    if (!when || (when->tv_sec == 0 && when->tv_nsec == 0)) return -1;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    ms = (long)(when->tv_sec - now.tv_sec) * 1000L +
         (long)(when->tv_nsec - now.tv_nsec) / 1000000L;
    return ms < 0 ? 0 : ms;
}

static void gw_schedule_sleep(int delay_ms) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return;
    now.tv_sec += delay_ms / 1000;
    now.tv_nsec += (long)(delay_ms % 1000) * 1000000L;
    if (now.tv_nsec >= 1000000000L) { now.tv_sec++; now.tv_nsec -= 1000000000L; }
    g_sleep_at = now;
    dprintf(2, "GW_SYSTEM: suspend scheduled in %d ms (lock=%d display=%d)\n",
            delay_ms, g_locked, g_display_on);
}

static void gw_cancel_sleep(void) {
    if (g_sleep_at.tv_sec || g_sleep_at.tv_nsec) {
        g_sleep_at.tv_sec = 0;
        g_sleep_at.tv_nsec = 0;
        dprintf(2, "GW_SYSTEM: suspend cancelled\n");
    }
}

static void gw_fill_power_state(gw_power_state_t *state) {
    if (!state) return;
    state->display_on = g_display_on;
    state->locked = g_locked;
    state->usb = gw_power_read_usb();
    state->charging = gw_power_read_charging();
}

/*
 * Perform kernel suspend. Only called from the service loop after the display
 * is already off and the cable/charging gates pass (unless force is set for a
 * deliberate root-console test).
 */
static void gw_do_suspend(int force, int fallback_seconds) {
    gw_power_state_t state;
    int rc, pressed = 0;

    gw_cancel_sleep();
    gw_fill_power_state(&state);
    if (!force && !gw_power_should_suspend(&state)) {
        dprintf(2, "GW_SYSTEM: suspend skipped (display=%d usb=%d charging=%d)\n",
                state.display_on, state.usb, state.charging);
        return;
    }
    if (state.display_on) display_off();

    event_broadcast(GW_EVT_PWR_SUSPEND "\n");
    /* Flush resident filesystem data before the panel and CPU stop. */
    sync();

    if (fallback_seconds <= 0) fallback_seconds = GW_SUSPEND_FALLBACK_SECONDS;
    if (gw_power_rtc_arm(fallback_seconds) < 0)
        dprintf(2, "GW_SYSTEM: RTC fallback alarm unavailable: %s\n", strerror(errno));

    g_suspended = 1;
    dprintf(2, "GW_SYSTEM: entering mem suspend (usb=%d charging=%d)\n",
            state.usb, state.charging);
    rc = gw_power_suspend();
    g_suspended = 0;
    gw_power_rtc_clear();

    if (rc < 0) {
        dprintf(2, "GW_SYSTEM: suspend failed: %s\n", strerror(errno));
        /* Leave the panel dark and locked; a later press wakes it. */
        return;
    }

    /* Resumed. Distinguish a button wake from an RTC/timeout wake. */
    gw_power_drain_button(g_btn_fd, &pressed);
    event_broadcast(GW_EVT_PWR_RESUME "\n");
    if (pressed) {
        dprintf(2, "GW_SYSTEM: resumed by power button\n");
        display_wake();
    } else {
        dprintf(2, "GW_SYSTEM: resumed without button (RTC/wakeup source)\n");
        /* No user is necessarily present. Keep the dark lock screen and
         * return to sleep after a bounded window unless a cable appeared. */
        gw_fill_power_state(&state);
        if (!state.display_on && gw_power_should_suspend(&state))
            gw_schedule_sleep(GW_SUSPEND_RETRY_MS);
    }
}

#if GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED
/*
 * Keep the saved Wi-Fi network connected after boot and across link drops.
 * The helper script is the only component that touches wpa_supplicant; this
 * service only decides when to invoke the fixed "start" action.
 */
static void gw_wifi_supervise(void) {
    struct timespec now;
    int link;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return;
    g_wifi_poll_at = now;
    g_wifi_poll_at.tv_sec += 5;
    if (!g_wifi_enabled || g_wifi_pid > 0 || g_locked) return;
    if (g_wifi_state == GW_WIFI_STATE_SCANNING) return;
    if (access("/var/lib/gleworks/wifi.conf", R_OK) != 0) {
        g_wifi_state = GW_WIFI_STATE_OFF;
        g_net_connected = 0;
        return;
    }
    link = gw_network_status();
    if (link == 1) {
        g_net_connected = 1;
        if (g_wifi_state != GW_WIFI_STATE_CONNECTED) {
            g_wifi_state = GW_WIFI_STATE_CONNECTED;
            g_wifi_retry_delay = 5;
            event_broadcast(GW_EVT_NET_UP "\n");
            dprintf(2, "GW_SYSTEM: Wi-Fi link up\n");
        }
        return;
    }
    g_net_connected = 0;
    if (g_wifi_state == GW_WIFI_STATE_CONNECTED ||
        g_wifi_state == GW_WIFI_STATE_ASSOCIATED ||
        g_wifi_state == GW_WIFI_STATE_OBTAINING_IP) {
        g_wifi_state = GW_WIFI_STATE_FAILED;
        event_broadcast(GW_EVT_NET_DOWN "\n");
        dprintf(2, "GW_SYSTEM: Wi-Fi link down\n");
    }
    if (gw_ms_until(&g_wifi_retry_at) < 0) {
        /* First attempt is immediate; later attempts use bounded backoff. */
        g_wifi_retry_at = now;
    }
    if (gw_ms_until(&g_wifi_retry_at) == 0) {
        if (gw_network_enable_hotplug() == 0) {
            g_wifi_error = 0;
            g_wifi_state = GW_WIFI_STATE_CONNECTING;
            g_wifi_pid = start_wifi("start");
            dprintf(2, "GW_SYSTEM: Wi-Fi reconnect attempt\n");
        }
        g_wifi_retry_at = now;
        g_wifi_retry_at.tv_sec += g_wifi_retry_delay;
        g_wifi_retry_delay *= 2;
        if (g_wifi_retry_delay > 60) g_wifi_retry_delay = 60;
    }
}
#endif /* GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED */

static void gw_power_sample(int startup) {
    struct timespec now;
    gw_power_state_t state;
    int usb = gw_power_read_usb();
    int charging = gw_power_read_charging();
    int usb_changed = !startup && usb != g_usb_state;

    /* The PMIC charger-online bit lags several seconds after unplug.  With a
     * proven absent cable, report discharging so STATUS and suspend agree. */
    if (usb == GW_USB_NONE) charging = 0;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return;
    state.display_on = g_display_on;
    state.locked = g_locked;
    state.usb = usb;
    state.charging = charging;

    if (usb_changed) {
        event_broadcast(usb == GW_USB_NONE ? GW_EVT_USB_DISCONNECTED "\n"
                                          : GW_EVT_USB_CONNECTED "\n");
        dprintf(2, "GW_SYSTEM: USB state %d -> %d\n", g_usb_state, usb);
    }
    if (!startup && gw_power_charging_changed(g_charging_state, charging))
        event_broadcast(charging ? GW_EVT_PWR_CHARGING "\n"
                                 : GW_EVT_PWR_DISCHARG "\n");

    g_usb_state = usb;
    g_charging_state = charging;
    g_charging = charging;

    /* A cable removal while the device is already dark/locked is an explicit
     * opportunity to enter real suspend; a cable attachment keeps it awake. */
    if (usb_changed && usb == GW_USB_NONE &&
        gw_power_on_usb_change(&state) == GW_POWER_ACTION_SUSPEND &&
        !g_suspended && gw_ms_until(&g_sleep_at) < 0)
        gw_schedule_sleep(3000);

    g_power_poll_at = now;
    g_power_poll_at.tv_sec += 2;
}

#if GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED
static pid_t start_wifi(const char *action) {
    pid_t pid = fork();
    if (pid == 0) {
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) { dup2(nullfd, STDOUT_FILENO); dup2(nullfd, STDERR_FILENO); close(nullfd); }
        execl("/etc/gleworks-wifi.sh", "/etc/gleworks-wifi.sh", action, (char *)NULL);
        _exit(127);
    }
    return pid;
}
#endif

static void reap_children(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == g_shell_pid) {
            struct timespec now;
            int timely = clock_gettime(CLOCK_MONOTONIC, &now) == 0 &&
                         now.tv_sec < g_pending_deadline;
            g_shell_pid = -1;
            if (g_pending_reader && timely && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                g_pending_reader = 0;
                g_reader_pid = start_child("/etc/gleworks-managed-reader.sh", 1);
                if (g_reader_pid > 0) strcpy(g_foreground_app, "koreader");
                else {
                    g_last_reader_status = 127;
                    g_shell_pid = start_child("/usr/sbin/gw-shell", 0);
                }
            } else {
                g_pending_reader = 0;
                if (++g_shell_failures <= 1) {
                    g_shell_pid = start_child("/usr/sbin/gw-shell", 0);
                    if (g_shell_pid < 0) dprintf(2, "GW_SYSTEM: shell restart failed\n");
                } else {
                    dprintf(2, "GW_SYSTEM: shell stopped; USB console remains available\n");
                }
            }
        } else if (pid == g_reader_pid) {
            g_reader_pid = -1;
            g_foreground_app[0] = '\0';
            g_last_reader_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
            g_shell_pid = start_child("/usr/sbin/gw-shell", 0);
            if (g_shell_pid < 0) dprintf(2, "GW_SYSTEM: failed to restart shell\n");
        } else if (pid == g_wifi_pid) {
            g_wifi_pid = -1;
            g_wifi_error = !WIFEXITED(status) || WEXITSTATUS(status) != 0;
            if (g_wifi_error) {
                g_wifi_state = GW_WIFI_STATE_FAILED;
            } else {
                int connected = gw_network_status();
                if (connected == 1) {
                    g_wifi_state = GW_WIFI_STATE_CONNECTED;
                    g_net_connected = 1;
                    g_wifi_retry_delay = 5;
                } else if (connected == 0) {
                    g_wifi_state = GW_WIFI_STATE_FAILED;
                    g_net_connected = 0;
                } else {
                    g_wifi_state = GW_WIFI_STATE_FAILED;
                }
            }
        }
        if (pid == g_time_sync_pid) {
            g_time_sync_pid = -1;
            g_time_sync_pending = 0;
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                time_t now = time(NULL);
                if (gw_config_save_int(GW_TIME_CONFIG_PATH, "epoch", (int)now) < 0)
                    dprintf(2, "GW_SYSTEM: time sync persist failed\n");
            }
        }
    }
}
#endif

/* ── Bounded service log ─────────────────────────────────────────────────── */

static const char *g_log_path = "/run/gw-system.log";
static off_t g_log_max = 1024 * 1024;

static void enforce_log_cap(void) {
    struct stat st;
    if (g_log_path == NULL || g_log_max <= 0) return;
    if (stat(g_log_path, &st) != 0 || !S_ISREG(st.st_mode)) return;
    if (st.st_size <= g_log_max) return;
    if (truncate(g_log_path, 0) == 0)
        dprintf(2, "GW_SYSTEM: log truncated (limit %ld bytes)\n", (long)g_log_max);
}

static void setup_log(void) {
    const char *path = getenv("GW_SYSTEM_LOG");
    const char *limit = getenv("GW_SYSTEM_LOG_MAX");
    if (path != NULL && strcmp(path, "none") == 0) {
        g_log_path = NULL;
        return;
    }
    if (path != NULL && path[0] != '\0') g_log_path = path;
    if (limit != NULL && limit[0] != '\0') {
        char *end = NULL;
        long value = strtol(limit, &end, 10);
        if (end != NULL && *end == '\0' && value >= 65536 && value <= 64 * 1024 * 1024)
            g_log_max = (off_t)value;
    }
    int fd = open(g_log_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0640);
    if (fd >= 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
    enforce_log_cap();
}

/* ── Signal handler ──────────────────────────────────────────────────────── */

static void on_signal(int sig) { (void)sig; g_quit = 1; }

static void on_sigchld(int sig) {
    char byte = 0;
    ssize_t unused;
    (void)sig;
    if (g_sigchld_wr >= 0) {
        unused = write(g_sigchld_wr, &byte, 1);
        (void)unused;
    }
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    setup_log();
#if GW_LIFECYCLE_SPIKE
    {
        const char *fallback = getenv("GW_SUSPEND_FALLBACK_SECONDS");
        if (fallback && *fallback) {
            char *end = NULL;
            long value = strtol(fallback, &end, 10);
            if (end && !*end && value >= 5 && value <= 86400)
                g_suspend_fallback = (int)value;
        }
    }
#endif
#if GW_POWER_LIGHT_ACCEPTED
    dprintf(2, "GW_POWER_LIGHT_ACCEPTED=1\n");
#endif
#if GW_POWER_SUSPEND_ACCEPTED
    dprintf(2, "GW_POWER_SUSPEND_ACCEPTED=1\n");
#endif
#if GW_WIFI_ACCEPTED
    dprintf(2, "GW_WIFI_ACCEPTED=1\n");
#endif
#if GW_ENABLE_HW_TRIAL
    dprintf(2, "GW_ENABLE_HW_TRIAL=1\n");
#endif
#if GW_LIFECYCLE_SPIKE
    dprintf(2, "GW_LIFECYCLE_SPIKE=1\n");
    if (argc == 2 && !strcmp(argv[1], "--supervise")) g_supervise = 1;
    else if (argc != 1) return 2;
#else
    (void)argv;
    if (argc != 1) return 2;
#endif
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    {
        int fds[2];
        if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) == 0) {
            g_sigchld_fd = fds[0];
            g_sigchld_wr = fds[1];
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = on_sigchld;
            sa.sa_flags = SA_NOCLDSTOP;
            sigemptyset(&sa.sa_mask);
            if (sigaction(SIGCHLD, &sa, NULL) < 0) {
                close(g_sigchld_fd);
                close(g_sigchld_wr);
                g_sigchld_fd = g_sigchld_wr = -1;
            }
        }
    }

    /* Keep the lock for the lifetime of both socket paths. */
    g_lock_fd = open(GW_IPC_PID_FILE, O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (g_lock_fd < 0 || flock(g_lock_fd, LOCK_EX | LOCK_NB) < 0) {
        dprintf(2, "GW_SYSTEM: another instance is running or lock unavailable\n");
        if (g_lock_fd >= 0) close(g_lock_fd);
        return 1;
    }
    {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
        if (ftruncate(g_lock_fd, 0) < 0 || write(g_lock_fd, buf, (size_t)n) != n) {
            close(g_lock_fd);
            return 1;
        }
    }

    /* Read boot ID for STATUS replies. */
    {
        int fd = open("/proc/sys/kernel/random/boot_id", O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            ssize_t n = read(fd, g_boot_id, sizeof(g_boot_id) - 1);
            if (n > 0) {
                g_boot_id[n] = '\0';
                /* Trim trailing newline. */
                char *nl = strchr(g_boot_id, '\n');
                if (nl) *nl = '\0';
            }
            close(fd);
        }
    }

    /* Stage 3: load persistent configuration. */
    {
        int v;
        char tz[GW_TIME_TZ_MAX];
        if (gw_config_load_string(GW_TIME_CONFIG_PATH, "timezone", tz, sizeof(tz)) == 0 &&
            gw_time_timezone_valid(tz))
            strcpy(g_timezone, tz);
        if (gw_config_load_int(GW_TIME_CONFIG_PATH, "format", &v) == 0 &&
            gw_time_format_valid(v))
            g_time_format = v;
        if (gw_config_load_int(GW_TIME_CONFIG_PATH, "epoch", &v) == 0 &&
            access("/dev/rtc0", R_OK) != 0) {
            struct timespec ts;
            ts.tv_sec = (time_t)v;
            ts.tv_nsec = 0;
            clock_settime(CLOCK_REALTIME, &ts);
        }
        if (gw_config_load_int(GW_LIGHT_CONFIG_PATH, "brightness", &v) == 0) {
            v = gw_light_clamp_pct(v);
            g_light_target = v;
        }
        if (gw_config_load_int(GW_WIFI_ENABLED_PATH, "enabled", &v) == 0)
            g_wifi_enabled = v ? 1 : 0;
        setenv("TZ", g_timezone, 1);
        tzset();
    }

    event_clients_init();

    /* A supervised shell must not share the power button with an app. */
    int exclusive_button = 0;
#if GW_LIFECYCLE_SPIKE
    exclusive_button = g_supervise;
#endif
    g_btn_fd = gw_power_open_button(exclusive_button);
    if (g_btn_fd < 0) {
        dprintf(2, "GW_SYSTEM: power button not found: %s\n", strerror(errno));
        if (exclusive_button) { cleanup(); return 1; }
    } else {
        dprintf(2, "GW_SYSTEM: power button fd=%d\n", g_btn_fd);
    }

    /* Probe battery and frontlight; failures are non-fatal at stub stage. */
    g_battery_pct = gw_power_read_battery();
    g_light_pct   = gw_power_read_light();
    g_charging     = gw_power_read_charging();
    if (g_light_pct < 0) g_light_pct = g_light_target >= 0 ? g_light_target : -1;
#if GW_POWER_LIGHT_ACCEPTED || GW_ENABLE_HW_TRIAL
    if (g_light_target >= 0 && gw_light_should_write(g_light_pct < 0 ? -1 : g_light_pct, g_light_target)) {
        if (gw_power_set_light(g_light_target) == 0) g_light_pct = g_light_target;
    }
#endif
#if GW_LIFECYCLE_SPIKE
    if (g_light_target > 0) g_last_light = g_light_target;
    else if (g_light_pct > 0) g_last_light = g_light_pct;
    else { g_display_on = 0; g_last_light = GW_DISPLAY_DEFAULT_LIGHT; }
    if (g_light_pct <= 0 && g_last_light > 0) g_display_on = 0;
#endif

    g_cmd_fd = bind_socket(GW_IPC_CMD_SOCKET);
    g_evt_fd = bind_socket(GW_IPC_EVT_SOCKET);
    if (g_cmd_fd < 0 || g_evt_fd < 0) {
        dprintf(2, "GW_SYSTEM: failed to bind sockets\n");
        cleanup();
        return 1;
    }

    dprintf(2, "GW_SYSTEM: ready boot=%s battery=%d light=%d charging=%d\n",
            g_boot_id, g_battery_pct, g_light_pct, g_charging);

#if GW_LIFECYCLE_SPIKE
    if (g_supervise) {
        g_shell_pid = start_child("/usr/sbin/gw-shell", 0);
        if (g_shell_pid < 0) { cleanup(); return 1; }
    }
#endif
#if GW_LIFECYCLE_SPIKE
    if (g_supervise) {
        gw_power_sample(1);
        if (clock_gettime(CLOCK_MONOTONIC, &g_wifi_poll_at) == 0)
            g_wifi_poll_at.tv_sec += 1;
    }
#endif
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
    /* The early boot script may already have connected Wi-Fi before this
     * service starts.  Reflect the real link instead of reporting OFF, and do
     * not restart supplicant/DHCP underneath an existing connection. */
    if (gw_network_status() == 1) {
        g_wifi_enabled = 1;
        g_wifi_state = GW_WIFI_STATE_CONNECTED;
        g_net_connected = 1;
    /* Auto-reconnect using the last saved credentials, if any. */
    } else if (g_wifi_enabled && access("/var/lib/gleworks/wifi.conf", R_OK) == 0) {
        g_wifi_state = GW_WIFI_STATE_CONNECTING;
        gw_network_enable_hotplug();
        g_wifi_error = 0;
        g_wifi_pid = start_wifi("start");
    } else if (g_wifi_enabled) {
        g_wifi_state = GW_WIFI_STATE_OFF;
    }
#endif

    service_loop(g_cmd_fd, g_evt_fd);

    cleanup();
    return 0;
}

/* ── Socket helpers ──────────────────────────────────────────────────────── */

static int bind_socket(const char *path) {
    struct sockaddr_un addr;
    int fd;

    unlink(path);  /* Remove stale socket. */

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    /* Root-only sockets: there is no dedicated group on the device image, so
     * 0600 is stricter and matches the all-root service contract.  SO_PEERCRED
     * still records UID/PID, but PID equality is defense in depth, not a
     * sandbox boundary. */
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        chmod(path, 0600) < 0 ||
        listen(fd, GW_IPC_MAX_CLIENTS) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* ── Event broadcast ─────────────────────────────────────────────────────── */

static void event_clients_init(void) {
    int i;
    for (i = 0; i < GW_IPC_MAX_CLIENTS; i++) g_evt_clients[i] = -1;
    g_evt_clients_ready = 1;
}

/*
 * Best-effort broadcast. Event delivery is not durable and a slow/stuck
 * listener is dropped rather than blocking the hardware service. Clients
 * must re-query STATUS after reconnecting.
 */
static void event_broadcast(const char *msg) {
    int i;
    size_t length;
    if (!g_evt_clients_ready || !msg) return;
    length = strlen(msg);
    for (i = 0; i < GW_IPC_MAX_CLIENTS; i++) {
        ssize_t n;
        int drop = 0;
        if (g_evt_clients[i] < 0) continue;
        do {
            n = write(g_evt_clients[i], msg, length);
        } while (n < 0 && errno == EINTR);
        if (gw_ipc_drop_listener(n, length, errno))
            drop = 1;
        if (drop) {
            close(g_evt_clients[i]);
            g_evt_clients[i] = -1;
        }
    }
}

/* ── Main loop ───────────────────────────────────────────────────────────── */

/*
 * Poll bounded command connections alongside the power-button input.
 */
static void service_loop(int cmd_fd, int evt_fd) {
    struct client {
        int fd;
        uid_t uid;
        pid_t pid;
        char line[GW_IPC_MAX_LINE];
        size_t used;
        time_t deadline;
    } clients[GW_IPC_MAX_CLIENTS];
    struct pollfd pfds[4 + GW_IPC_MAX_CLIENTS];
    int i;
    for (i = 0; i < GW_IPC_MAX_CLIENTS; i++) clients[i].fd = -1;
    pfds[0].fd     = cmd_fd;
    pfds[0].events = POLLIN;
    pfds[1].fd     = evt_fd;
    pfds[1].events = POLLIN;
    pfds[2].fd     = g_btn_fd;   /* may be -1; poll ignores negative fds */
    pfds[2].events = POLLIN;
    pfds[3].fd     = g_sigchld_fd; /* may be -1 when pipe setup failed */
    pfds[3].events = POLLIN;

    while (!g_quit) {
#if GW_LIFECYCLE_SPIKE
        if (g_supervise) reap_children();
#endif
        if (g_time_sync_pid > 0) {
            int tstatus;
            pid_t tp = waitpid(g_time_sync_pid, &tstatus, WNOHANG);
            if (tp == g_time_sync_pid) {
                g_time_sync_pid = -1;
                g_time_sync_pending = 0;
                if (!WIFEXITED(tstatus) || WEXITSTATUS(tstatus) != 0)
                    dprintf(2, "GW_SYSTEM: time sync failed\n");
                else
                    dprintf(2, "GW_SYSTEM: time sync completed\n");
            }
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        {
            static time_t next_log_check;
            if (now.tv_sec >= next_log_check) {
                enforce_log_cap();
                next_log_check = now.tv_sec + 60;
            }
        }
#if GW_LIFECYCLE_SPIKE
        if (g_pending_reader && now.tv_sec >= g_pending_deadline) {
            g_pending_reader = 0;
            dprintf(2, "GW_SYSTEM: reader handoff timed out\n");
        }
        /* Re-sample cable/charger state every two seconds and act on the
         * scheduled suspend deadline.  Both run in the service loop, which is
         * the only owner of the button and power state. */
        if (g_supervise && gw_ms_until(&g_power_poll_at) == 0)
            gw_power_sample(0);
        if (g_supervise && !g_suspended && gw_ms_until(&g_sleep_at) == 0)
            gw_do_suspend(0, g_suspend_fallback);
#endif
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
        if (g_supervise && gw_ms_until(&g_wifi_poll_at) == 0)
            gw_wifi_supervise();
#endif
        for (i = 0; i < GW_IPC_MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0 && now.tv_sec >= clients[i].deadline) {
                memset(clients[i].line, 0, sizeof(clients[i].line));
                close(clients[i].fd);
                clients[i].fd = -1;
            }
            pfds[i + 4].fd = clients[i].fd;
            pfds[i + 4].events = POLLIN;
        }
        /* SIGCHLD normally wakes the loop; keep a short fallback timeout while
         * a reader handoff is waiting in case the self-pipe is unavailable. */
        int timeout = 1000;
#if GW_LIFECYCLE_SPIKE
        if (g_pending_reader) timeout = 20;
#endif
        int r = poll(pfds, 4 + GW_IPC_MAX_CLIENTS, timeout);
        if (r < 0) { if (errno == EINTR) continue; break; }

        /* Drain the signal pipe; reap_children() runs at the top of the loop. */
        if (pfds[3].revents & POLLIN) {
            char drain[32];
            while (read(g_sigchld_fd, drain, sizeof(drain)) > 0) { }
        }

        /* New event listener (service -> UI). */
        if (pfds[1].revents & POLLIN) {
            int listener = accept4(evt_fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
            if (listener >= 0) {
                int slot;
                for (slot = 0; slot < GW_IPC_MAX_CLIENTS; slot++)
                    if (g_evt_clients[slot] < 0) break;
                if (slot == GW_IPC_MAX_CLIENTS) close(listener);
                else g_evt_clients[slot] = listener;
            }
        }
        /* Power-button input event. */
        if (g_btn_fd >= 0 && (pfds[2].revents & POLLIN)) {
            int key = gw_power_read_button_event(g_btn_fd);
            if (key == GW_BTN_KEY_DOWN) {
                dprintf(2, "GW_SYSTEM: power button key down\n");
#if GW_LIFECYCLE_SPIKE
                if (g_supervise && clock_gettime(CLOCK_MONOTONIC, &g_button_down) == 0)
                    g_button_pressed = 1;
#endif
            }
#if GW_LIFECYCLE_SPIKE
            else if (key == GW_BTN_KEY_UP && g_supervise && g_button_pressed) {
                struct timespec up;
                g_button_pressed = 0;
                if (clock_gettime(CLOCK_MONOTONIC, &up) == 0) {
                    long elapsed_ms = (up.tv_sec - g_button_down.tv_sec) * 1000L +
                                      (up.tv_nsec - g_button_down.tv_nsec) / 1000000L;
                    if (elapsed_ms >= 80 && elapsed_ms <= 5000) {
                        gw_power_state_t state;
                        int action;
                        gw_fill_power_state(&state);
                        action = gw_power_on_short_press(&state);
                        event_broadcast(GW_EVT_PWR_BTN_SHORT "\n");
                        if (action & GW_POWER_ACTION_WAKE) {
                            display_wake();
                        } else if (action & GW_POWER_ACTION_LOCK) {
                            display_off();
                            if (action & GW_POWER_ACTION_SUSPEND) {
                                /* Give a foreground reader time to notice
                                 * lock=locked, save and exit before mem sleep. */
                                gw_schedule_sleep(g_reader_pid > 0 ? 7000
                                                                  : GW_SUSPEND_DELAY_MS);
                            }
                        }
                    }
                }
            }
#endif
        }

        /* Incoming command connection. */
        if (pfds[0].revents & POLLIN) {
            struct sockaddr_un peer;
            socklen_t plen = sizeof(peer);
            int client = accept4(cmd_fd, (struct sockaddr *)&peer, &plen,
                                 SOCK_CLOEXEC | SOCK_NONBLOCK);
            if (client < 0) continue;
            struct ucred cred;
            socklen_t clen = sizeof(cred);
            for (i = 0; i < GW_IPC_MAX_CLIENTS && clients[i].fd >= 0; i++) {}
            if (i == GW_IPC_MAX_CLIENTS ||
                getsockopt(client, SOL_SOCKET, SO_PEERCRED, &cred, &clen) < 0 ||
                clen != sizeof(cred) || cred.pid <= 0) {
                close(client);
            } else {
                clock_gettime(CLOCK_MONOTONIC, &now);
                clients[i].fd = client;
                clients[i].uid = cred.uid;
                clients[i].pid = cred.pid;
                clients[i].used = 0;
                /* Idle timeout for a persistent connection. */
                clients[i].deadline = now.tv_sec + GW_IPC_IDLE_SECONDS;
            }
        }

        for (i = 0; i < GW_IPC_MAX_CLIENTS; i++) {
            struct client *c = &clients[i];
            if (c->fd < 0 || pfds[i + 4].fd != c->fd || !pfds[i + 4].revents)
                continue;
            char buf[GW_IPC_MAX_LINE];
            ssize_t n = read(c->fd, buf, sizeof(buf));
            int done = n <= 0;
            ssize_t j;
            for (j = 0; j < n && !done; j++) {
                unsigned char ch = (unsigned char)buf[j];
                if (ch == '\n') {
                    c->line[c->used] = '\0';
                    if (!strcmp(c->line, "QUIT")) {
                        done = 1;
                        break;
                    }
                    handle_command(c->fd, c->uid, c->pid, c->line, c->used);
                    c->used = 0;
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    c->deadline = now.tv_sec + GW_IPC_IDLE_SECONDS;
                } else if (ch < 32 || ch > 126 || c->used >= sizeof(c->line) - 1) {
                    write_line(c->fd, "ERR invalid-request\n");
                    done = 1;
                } else c->line[c->used++] = (char)ch;
            }
            memset(buf, 0, sizeof(buf));
            if (done) {
                memset(c->line, 0, sizeof(c->line));
                close(c->fd); c->fd = -1;
            }
        }
    }
    for (i = 0; i < GW_IPC_MAX_CLIENTS; i++)
        if (clients[i].fd >= 0) close(clients[i].fd);
}

/* ── Command dispatch ────────────────────────────────────────────────────── */

static int is_privileged(uid_t uid) { return gw_ipc_uid_privileged((unsigned long)uid); }

/* One installed-app listing per service generation; install/remove invalidate
 * it so APP COUNT and APP INFO do not re-enumerate the filesystem per query. */
static gw_app_info_t g_app_cache[GW_APPS_MAX];
static int g_app_cache_count = -1;

static int apps_cached(gw_app_info_t **out) {
    if (g_app_cache_count < 0) {
        g_app_cache_count = gw_apps_list(g_app_cache, GW_APPS_MAX);
        if (g_app_cache_count < 0) g_app_cache_count = 0;
    }
    *out = g_app_cache;
    return g_app_cache_count;
}

static void apps_cache_invalidate(void) { g_app_cache_count = -1; }

#if GW_LIFECYCLE_SPIKE
#define GW_SUPERVISE g_supervise
#define GW_SHELL_PID g_shell_pid
#define GW_LOCKED g_locked
#else
/* Without the lifecycle supervisor there is no supervised shell to name. */
#define GW_SUPERVISE 0
#define GW_SHELL_PID ((pid_t)-1)
#define GW_LOCKED 0
#endif

/* Returns 1 when the request was rejected and a response was written. */
static int authorize_command(int fd, uid_t uid, pid_t pid, const char *line) {
    const gw_command_policy_t *policy = gw_command_policy(line);
    if (!policy) return 0;
    if ((policy->flags & GW_CMD_PRIV) && !is_privileged(uid)) {
        (void)write_line(fd, GW_RESP_ERR " permission-denied\n");
        return 1;
    }
    if ((policy->flags & GW_CMD_SHELL) &&
        (!GW_SUPERVISE || pid != GW_SHELL_PID)) {
        (void)write_line(fd, GW_RESP_ERR " unavailable\n");
        return 1;
    }
    if ((policy->flags & GW_CMD_LOCKED) && GW_LOCKED) {
        (void)write_line(fd, GW_RESP_ERR " unavailable\n");
        return 1;
    }
    return 0;
}

static int handle_command(int fd, uid_t uid, pid_t pid, const char *line, size_t len) {
    (void)len;
#if !GW_LIFECYCLE_SPIKE
    (void)pid;
#endif
    char resp[GW_IPC_MAX_LINE];

    if (authorize_command(fd, uid, pid, line)) return 0;

    if (strcmp(line, GW_CMD_CAPS) == 0) {
        const char *light = (GW_POWER_LIGHT_ACCEPTED || GW_ENABLE_HW_TRIAL) ? "1" : "0";
#if GW_LIFECYCLE_SPIKE && GW_WIFI_ACCEPTED
        snprintf(resp, sizeof(resp),
                 "CAPS light-set=%s wifi=%s apps=install-only suspend=%s\n", light,
                 g_supervise ? "1" : "0",
                 (GW_POWER_SUSPEND_ACCEPTED || GW_ENABLE_HW_TRIAL) ? "1" : "0");
        return write_line(fd, resp);
#elif GW_LIFECYCLE_SPIKE && GW_ENABLE_HW_TRIAL
        snprintf(resp, sizeof(resp),
                 "CAPS light-set=%s wifi=%s apps=install-only suspend=%s\n", light,
                 g_supervise ? "trial" : "0",
                 (GW_POWER_SUSPEND_ACCEPTED || GW_ENABLE_HW_TRIAL) ? "1" : "0");
        return write_line(fd, resp);
#else
        snprintf(resp, sizeof(resp),
                 "CAPS light-set=%s wifi=0 apps=install-only suspend=%s\n", light,
                 (GW_POWER_SUSPEND_ACCEPTED || GW_ENABLE_HW_TRIAL) ? "1" : "0");
        return write_line(fd, resp);
#endif
    }
    if (strcmp(line, GW_CMD_STATUS) == 0) {
        int usb = gw_power_read_usb();
        const char *foreground = g_foreground_app[0] ? g_foreground_app : "unknown";
#if GW_LIFECYCLE_SPIKE
        if (g_supervise && g_shell_pid > 0 && g_reader_pid < 0) foreground = "none";
#endif
        g_battery_pct = gw_power_read_battery();
        g_light_pct = gw_power_read_light();
        g_charging = usb == GW_USB_NONE ? 0 : gw_power_read_charging();
#if GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED
        {
            int link = gw_network_status();
            g_net_connected = link;
            /* Boot-time Wi-Fi may finish DHCP after this service starts.
             * Report the link that actually exists rather than a stale OFF. */
            if (link == 1 &&
                (g_wifi_state == GW_WIFI_STATE_OFF ||
                 g_wifi_state == GW_WIFI_STATE_FAILED))
                g_wifi_state = GW_WIFI_STATE_CONNECTED;
        }
#endif
        snprintf(resp, sizeof(resp),
                 "STATUS version=0.1-stub boot=%s power=%s"
                 " battery=%d light=%d foreground=%s net=%s usb=%s"
                 " time=%ld timezone=%s time-format=%d wifi-state=%s"
#if GW_LIFECYCLE_SPIKE
                 " reader-exit=%d lock=%s wifi-error=%d wifi-busy=%d"
                 " display=%s"
#endif
                 "\n",
                 g_boot_id[0] ? g_boot_id : "unknown",
                 g_charging < 0 ? "unknown" : g_charging ? "charging" : "battery",
                 g_battery_pct,
                 g_light_pct,
                 foreground,
                 g_net_connected < 0 ? "unknown" : g_net_connected ? "connected" : "disconnected",
                 usb < 0 ? "unknown" : usb == 0 ? "none" : usb == 1 ? "power" : "host-class",
                 (long)time(NULL),
                 g_timezone,
                 g_time_format,
                 gw_wifi_state_name(g_wifi_state)
#if GW_LIFECYCLE_SPIKE
                 , g_last_reader_status, g_locked ? "locked" : "unlocked",
                 g_wifi_error, g_wifi_pid > 0,
                 g_display_on ? "on" : "off"
#endif
                 );
        return write_line(fd, resp);
    }

    if (strcmp(line, GW_CMD_LIGHT_GET) == 0) {
        g_light_pct = gw_power_read_light();
        snprintf(resp, sizeof(resp), "LIGHT %d\n", g_light_pct);
        return write_line(fd, resp);
    }

    if (strcmp(line, GW_CMD_TIME_GET) == 0) {
        time_t now = time(NULL);
        char formatted[64];
        if (gw_time_format_string(now, g_timezone, g_time_format, formatted, sizeof(formatted)) != 0)
            strcpy(formatted, "unavailable");
        snprintf(resp, sizeof(resp), "TIME %ld %s %d %s\n", (long)now, g_timezone, g_time_format, formatted);
        return write_line(fd, resp);
    }
    if (strcmp(line, GW_CMD_TIMEZONE_GET) == 0) {
        snprintf(resp, sizeof(resp), "TIMEZONE %s\n", g_timezone);
        return write_line(fd, resp);
    }
    if (strncmp(line, GW_CMD_TIME_SYNC, strlen(GW_CMD_TIME_SYNC)) == 0) {
        if (!is_privileged(uid))
            return write_line(fd, GW_RESP_ERR " permission-denied\n");
        if (g_net_connected != 1)
            return write_line(fd, GW_RESP_ERR " network-unavailable\n");
        if (g_time_sync_pid > 0)
            return write_line(fd, GW_RESP_ERR " busy\n");
        g_time_sync_pid = fork();
        if (g_time_sync_pid == 0) {
            /* A blocked resolver must not leave a privileged child forever. */
            alarm(30);
            execl("/usr/sbin/ntpd", "ntpd", "-q", "-p", "pool.ntp.org", (char *)NULL);
            _exit(127);
        }
        if (g_time_sync_pid < 0) return write_line(fd, GW_RESP_ERR " unavailable\n");
        g_time_sync_pending = 1;
        return write_line(fd, GW_RESP_OK "\n");
    }

    /* Authorization already happened in authorize_command(). */

#if GW_LIFECYCLE_SPIKE
    if (g_supervise && strcmp(line, GW_CMD_LOCK) == 0) {
        gw_power_state_t state;
        if (g_pending_reader) return write_line(fd, GW_RESP_ERR " unavailable\n");
        if (g_display_on) display_off();
        else { g_locked = 1; event_broadcast(GW_EVT_PWR_LOCK "\n"); }
        gw_fill_power_state(&state);
        state.display_on = 0;
        if (gw_power_should_suspend(&state)) gw_schedule_sleep(GW_SUSPEND_DELAY_MS);
        return write_line(fd, GW_RESP_OK "\n");
    }
    if (g_supervise && strcmp(line, GW_CMD_UNLOCK) == 0) {
        gw_cancel_sleep();
        if (!g_display_on) display_wake();
        g_locked = 0;
        event_broadcast(GW_EVT_PWR_UNLOCK "\n");
        return write_line(fd, GW_RESP_OK "\n");
    }
#endif

    if (strncmp(line, GW_CMD_TIME_SET " ", sizeof(GW_CMD_TIME_SET " ") - 1) == 0) {
        const char *value = line + sizeof(GW_CMD_TIME_SET " ") - 1;
        char *end = NULL;
        long epoch;
        errno = 0;
        epoch = strtol(value, &end, 10);
        if (errno || !*value || *end || epoch < 0)
            return write_line(fd, GW_RESP_ERR " invalid-request\n");
        struct timespec ts;
        ts.tv_sec = (time_t)epoch;
        ts.tv_nsec = 0;
        if (clock_settime(CLOCK_REALTIME, &ts) != 0)
            return write_line(fd, GW_RESP_ERR " hardware-error\n");
        if (gw_config_save_int(GW_TIME_CONFIG_PATH, "epoch", (int)epoch) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        return write_line(fd, GW_RESP_OK "\n");
    }
    if (strncmp(line, GW_CMD_TIMEZONE_SET " ", sizeof(GW_CMD_TIMEZONE_SET " ") - 1) == 0) {
        const char *value = line + sizeof(GW_CMD_TIMEZONE_SET " ") - 1;
        if (!gw_time_timezone_valid(value))
            return write_line(fd, GW_RESP_ERR " invalid-request\n");
        strcpy(g_timezone, value);
        setenv("TZ", g_timezone, 1);
        tzset();
        if (gw_config_save_string(GW_TIME_CONFIG_PATH, "timezone", g_timezone) < 0 ||
            gw_config_save_int(GW_TIME_CONFIG_PATH, "format", g_time_format) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        return write_line(fd, GW_RESP_OK "\n");
    }
    if (strncmp(line, GW_CMD_TIME_FORMAT " ", sizeof(GW_CMD_TIME_FORMAT " ") - 1) == 0) {
        const char *value = line + sizeof(GW_CMD_TIME_FORMAT " ") - 1;
        int fmt = atoi(value);
        if (!gw_time_format_valid(fmt))
            return write_line(fd, GW_RESP_ERR " invalid-request\n");
        g_time_format = fmt;
        if (gw_config_save_string(GW_TIME_CONFIG_PATH, "timezone", g_timezone) < 0 ||
            gw_config_save_int(GW_TIME_CONFIG_PATH, "format", g_time_format) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        return write_line(fd, GW_RESP_OK "\n");
    }

    if (strcmp(line, GW_CMD_NET_ENABLE) == 0) {
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
        g_wifi_enabled = 1;
        if (gw_config_save_int(GW_WIFI_ENABLED_PATH, "enabled", 1) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        g_wifi_retry_delay = 5;
        g_wifi_retry_at.tv_sec = 0;
        g_wifi_retry_at.tv_nsec = 0;
        if (access("/var/lib/gleworks/wifi.conf", R_OK) == 0) {
            gw_wifi_transition(g_wifi_state, GW_WIFI_EVENT_CONNECT_START, &g_wifi_state);
            gw_network_enable_hotplug();
            g_wifi_pid = start_wifi("start");
        } else {
            g_wifi_state = GW_WIFI_STATE_OFF;
        }
        return write_line(fd, GW_RESP_OK "\n");
#else
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
#endif
    }
    if (strcmp(line, GW_CMD_NET_DISABLE) == 0) {
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
        g_wifi_enabled = 0;
        if (gw_config_save_int(GW_WIFI_ENABLED_PATH, "enabled", 0) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        if (g_wifi_pid > 0) { kill(g_wifi_pid, SIGTERM); g_wifi_pid = -1; }
        start_wifi("stop");
        g_wifi_state = GW_WIFI_STATE_OFF;
        g_net_connected = 0;
        return write_line(fd, GW_RESP_OK "\n");
#else
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
#endif
    }
    if (strcmp(line, GW_CMD_NET_FORGET) == 0) {
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
        if (g_wifi_pid > 0) { kill(g_wifi_pid, SIGTERM); g_wifi_pid = -1; }
        start_wifi("stop");
        unlink("/var/lib/gleworks/wifi.conf");
        g_wifi_enabled = 0;
        if (gw_config_save_int(GW_WIFI_ENABLED_PATH, "enabled", 0) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        g_wifi_retry_at.tv_sec = 0;
        g_wifi_retry_at.tv_nsec = 0;
        g_wifi_state = GW_WIFI_STATE_OFF;
        g_net_connected = 0;
        return write_line(fd, GW_RESP_OK "\n");
#else
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
#endif
    }
    if (strcmp(line, GW_CMD_NET_STATE) == 0) {
        snprintf(resp, sizeof(resp), "NET STATE %s\n", gw_wifi_state_name(g_wifi_state));
        return write_line(fd, resp);
    }

    if (strcmp(line, GW_CMD_SUSPEND) == 0 || strcmp(line, "SUSPEND FORCE") == 0 ||
        strcmp(line, "SUSPEND TEST") == 0) {
#if GW_LIFECYCLE_SPIKE && (GW_POWER_SUSPEND_ACCEPTED || GW_ENABLE_HW_TRIAL)
        int force = strcmp(line, GW_CMD_SUSPEND) != 0;
        int fallback = strcmp(line, "SUSPEND TEST") == 0 ? 10
                                                         : g_suspend_fallback;
        gw_power_state_t state;
        if (!g_supervise)
            return write_line(fd, GW_RESP_ERR " unavailable\n");
        gw_fill_power_state(&state);
        state.display_on = 0;   /* the service is about to darken the panel */
        if (!force && !gw_power_should_suspend(&state))
            return write_line(fd, GW_RESP_ERR " cable-or-charging\n");
        if (write_line(fd, GW_RESP_OK "\n") < 0) return -1;
        display_off();
        gw_do_suspend(force, fallback);
        return 0;
#else
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
#endif
    }
    if (strcmp(line, GW_CMD_POWEROFF) == 0) {
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
    }
    if (strcmp(line, GW_CMD_REBOOT) == 0) {
        /* Implemented via proven shutdown helper when validated. */
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
    }
    if (strcmp(line, GW_CMD_RESCUE) == 0) {
        /* Remove boot.enabled and reboot. Accepted path: Milestone 5. */
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
    }
    if (strncmp(line, GW_CMD_LIGHT_SET " ", sizeof(GW_CMD_LIGHT_SET " ") - 1) == 0) {
        const char *value = line + sizeof(GW_CMD_LIGHT_SET " ") - 1;
        const char *p = value;
        unsigned pct = 0;
        if (!*p) return write_line(fd, GW_RESP_ERR " invalid-request\n");
        while (*p) {
            if (*p < '0' || *p > '9' || pct > 100)
                return write_line(fd, GW_RESP_ERR " invalid-request\n");
            pct = pct * 10 + (unsigned)(*p++ - '0');
        }
        if (pct > 100) return write_line(fd, GW_RESP_ERR " invalid-request\n");
#if GW_POWER_LIGHT_ACCEPTED || GW_ENABLE_HW_TRIAL
        if (g_light_pct < 0 || gw_light_should_write(g_light_pct, (int)pct)) {
            if (gw_power_set_light((int)pct) < 0)
                return write_line(fd, GW_RESP_ERR " hardware-error\n");
        } else {
            /* Avoid redundant hardware write. */
        }
        if (pct > 0) g_light_target = (int)pct;
        if (gw_config_save_int(GW_LIGHT_CONFIG_PATH, "brightness", (int)pct) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        g_light_pct = gw_power_read_light();
        if (g_light_pct < 0) g_light_pct = (int)pct;
        return write_line(fd, GW_RESP_OK "\n");
#else
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
#endif
    }
    if (strncmp(line, GW_CMD_APP_LAUNCH, strlen(GW_CMD_APP_LAUNCH)) == 0) {
#if GW_LIFECYCLE_SPIKE
        if (g_supervise && pid == g_shell_pid && g_reader_pid < 0 && !g_locked &&
            !g_pending_reader && !strcmp(line, GW_CMD_APP_LAUNCH " koreader")) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
                return write_line(fd, GW_RESP_ERR " unavailable\n");
            g_pending_reader = 1;
            g_pending_deadline = now.tv_sec + 5;
            return write_line(fd, GW_RESP_OK "\n");
        }
#endif
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
    }
    if (strcmp(line, "APP COUNT") == 0) {
        gw_app_info_t *apps;
        int count = apps_cached(&apps);
        (void)apps;
        snprintf(resp, sizeof(resp), "APPS %d\n", count);
        return write_line(fd, resp);
    }
    if (strcmp(line, "APP LIST") == 0) {
        gw_app_info_t *apps;
        int count = apps_cached(&apps);
        int i;
        for (i = 0; i < count; i++) {
            snprintf(resp, sizeof(resp), "APP %s %s %s\n", apps[i].id,
                     apps[i].version,
                     apps[i].description[0] ? apps[i].description : "-");
            if (write_line(fd, resp) < 0) return -1;
        }
        return write_line(fd, "APP END\n");
    }
    if (strncmp(line, "APP INFO ", 9) == 0) {
        gw_app_info_t *apps;
        const char *p = line + 9;
        unsigned index = 0;
        int count;
        if (!*p) return write_line(fd, GW_RESP_ERR " invalid-request\n");
        while (*p) {
            if (*p < '0' || *p > '9' || index > GW_APPS_MAX)
                return write_line(fd, GW_RESP_ERR " invalid-request\n");
            index = index * 10 + (unsigned)(*p++ - '0');
        }
        count = apps_cached(&apps);
        if (index >= (unsigned)count)
            return write_line(fd, "APP END\n");
        snprintf(resp, sizeof(resp), "APP %s %s %s\n", apps[index].id,
                 apps[index].version,
                 apps[index].description[0] ? apps[index].description : "-");
        return write_line(fd, resp);
    }
    if (strcmp(line, "APP SCAN") == 0) {
        char names[GW_APPS_MAX][GW_APP_NAME_MAX];
        int count = gw_apps_usb_scan(names, GW_APPS_MAX);
        snprintf(resp, sizeof(resp), "APP USB %d\n", count < 0 ? 0 : count);
        return write_line(fd, resp);
    }
    if (strncmp(line, "APP USB ", 8) == 0) {
        char names[GW_APPS_MAX][GW_APP_NAME_MAX];
        const char *p = line + 8;
        unsigned index = 0;
        int count = gw_apps_usb_scan(names, GW_APPS_MAX);
        if (!*p) return write_line(fd, GW_RESP_ERR " invalid-request\n");
        while (*p) {
            if (*p < '0' || *p > '9' || index > GW_APPS_MAX)
                return write_line(fd, GW_RESP_ERR " invalid-request\n");
            index = index * 10 + (unsigned)(*p++ - '0');
        }
        if (count < 0 || index >= (unsigned)count)
            return write_line(fd, "APP USB END\n");
        snprintf(resp, sizeof(resp), "APP USBFILE %s\n", names[index]);
        return write_line(fd, resp);
    }
    if (strncmp(line, "APP INSTALL USB", 15) == 0 ||
        strncmp(line, "APP UPDATE USB", 14) == 0) {
        typedef struct {
            char name[GW_APP_NAME_MAX];
            char id[64];
            char version[32];
            int chosen;
        } candidate_t;
        char names[GW_APPS_MAX][GW_APP_NAME_MAX];
        candidate_t candidates[GW_APPS_MAX];
        char last_error[160] = "none";
        const char *selection = line + (line[4] == 'I' ? 15 : 14);
        int count, i, j, used = 0, installed = 0, skipped = 0, failed = 0;
        int all = 0, selected = -1;
        while (*selection == ' ') selection++;
        if (!strcmp(selection, "ALL")) {
            all = 1;
        } else if (!*selection) {
            return write_line(fd, GW_RESP_ERR " invalid-request\n");
        } else {
            while (*selection) {
                if (*selection < '0' || *selection > '9')
                    return write_line(fd, GW_RESP_ERR " invalid-request\n");
                if (selected > GW_APPS_MAX)
                    return write_line(fd, GW_RESP_ERR " invalid-request\n");
                selected = (selected < 0 ? 0 : selected * 10) +
                           (*selection++ - '0');
            }
        }
        count = gw_apps_usb_scan(names, GW_APPS_MAX);
        if (count <= 0)
            return write_line(fd, GW_RESP_ERR " no-packages-on-usb\n");
        if (!all && (selected < 0 || selected >= count))
            return write_line(fd, GW_RESP_ERR " invalid-package-index\n");
        /* A single selection installs exactly that USB entry.  ALL keeps only
         * the newest bundle per app ID, so a stale file cannot downgrade an
         * app depending on directory order. */
        for (i = 0; i < count && used < GW_APPS_MAX; i++) {
            char path[256];
            gw_app_info_t info;
            if (!all && i != selected) continue;
            if (snprintf(path, sizeof(path), "%s/%s", GW_APPS_USB_DIR, names[i]) >=
                (int)sizeof(path)) continue;
            /* Verify and read metadata from the same private staged copy, so
             * switching the USB file after the check cannot change what the
             * installer later reads. */
            {
                char verify_error[96] = "";
                if (gw_apps_package_info_verified(path, NULL, &info,
                                                  verify_error,
                                                  sizeof(verify_error)) != 0) {
                    failed++;
                    snprintf(last_error, sizeof(last_error), "package-signature-%s",
                             verify_error[0] ? verify_error : "invalid");
                    continue;
                }
            }
            snprintf(candidates[used].name, sizeof(candidates[used].name), "%s", names[i]);
            snprintf(candidates[used].id, sizeof(candidates[used].id), "%s", info.id);
            snprintf(candidates[used].version, sizeof(candidates[used].version), "%s", info.version);
            candidates[used].chosen = 1;
            used++;
        }
        if (all) {
            for (i = 0; i < used; i++)
                for (j = 0; j < used; j++)
                    if (i != j && !strcmp(candidates[i].id, candidates[j].id) &&
                        gw_apps_version_compare(candidates[i].version,
                                                candidates[j].version) < 0)
                        candidates[i].chosen = 0;
        }
        for (i = 0; i < used; i++) {
            char path[256];
            char err[160] = "";
            if (!candidates[i].chosen) { skipped++; continue; }
            if (snprintf(path, sizeof(path), "%s/%s", GW_APPS_USB_DIR,
                         candidates[i].name) >= (int)sizeof(path)) continue;
            if (gw_apps_install(path, NULL, err, sizeof(err)) == 0) {
                installed++;
            } else if (strstr(err, "version-already-installed")) {
                skipped++;
            } else {
                failed++;
                snprintf(last_error, sizeof(last_error), "%s",
                         err[0] ? err : "install-failed");
            }
        }
        if (installed) apps_cache_invalidate();
        dprintf(2, "GW_SYSTEM: USB app install: %d installed, %d skipped, %d failed (%s)\n",
                installed, skipped, failed, last_error);
        if (!failed) {
            snprintf(resp, sizeof(resp), "OK %d skipped=%d\n", installed, skipped);
            return write_line(fd, resp);
        }
        if (installed > 0) {
            snprintf(resp, sizeof(resp), "ERR partial-%d-installed-%d-failed-%s\n",
                     installed, failed, last_error);
            return write_line(fd, resp);
        }
        snprintf(resp, sizeof(resp), "ERR %s\n", last_error);
        return write_line(fd, resp);
    }
    if (strncmp(line, "APP REMOVE ", 11) == 0) {
        char err[160] = "";
        if (gw_apps_remove(line + 11, err, sizeof(err)) < 0) {
            snprintf(resp, sizeof(resp), "ERR %s\n", err[0] ? err : "remove-failed");
            return write_line(fd, resp);
        }
        apps_cache_invalidate();
        return write_line(fd, GW_RESP_OK "\n");
    }
    if (strcmp(line, GW_CMD_APP_EXIT) == 0) {
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
    }
    if (strncmp(line, GW_CMD_APP_CRASH, strlen(GW_CMD_APP_CRASH)) == 0) {
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
    }
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
    if (!strcmp(line, "NET SCAN")) {
        if (g_wifi_pid > 0) return write_line(fd, GW_RESP_ERR " unavailable\n");
        if (gw_network_enable_hotplug()) return write_line(fd, GW_RESP_ERR " unavailable\n");
        g_wifi_error = 0;
        g_wifi_state = GW_WIFI_STATE_SCANNING;
        g_wifi_pid = start_wifi("scan");
        return write_line(fd, g_wifi_pid > 0 ? GW_RESP_OK "\n" : GW_RESP_ERR " unavailable\n");
    }
    if (!strcmp(line, "NET SCAN RESULTS")) {
        char rows[GW_IPC_MAX_LINE * 4];
        char *cursor;
        int count = gw_network_scan_all(rows, sizeof(rows));
        if (count < 0) return write_line(fd, "NET END\n");
        snprintf(resp, sizeof(resp), "NET RESULTS %d\n", count);
        if (write_line(fd, resp) < 0) return -1;
        cursor = rows;
        while (*cursor) {
            char *newline = strchr(cursor, '\n');
            char hex[65], row[GW_IPC_MAX_LINE];
            int signal_dbm, secure;
            if (!newline) break;
            *newline = '\0';
            if (sscanf(cursor, "%64s %d %d", hex, &signal_dbm, &secure) == 3) {
                snprintf(row, sizeof(row), "NET SSID %s %d %d\n", hex,
                         signal_dbm, secure);
                if (write_line(fd, row) < 0) return -1;
            }
            cursor = newline + 1;
        }
        return write_line(fd, "NET END\n");
    }
    if (!strncmp(line, "NET RESULT ", 11)) {
        char ssid[65];
        int signal_dbm = 0, secure = 0;
        const char *p = line + 11;
        unsigned index = 0;
        if (!*p) return write_line(fd, GW_RESP_ERR " invalid-request\n");
        while (*p) {
            if (*p < '0' || *p > '9' || index > 50) return write_line(fd, GW_RESP_ERR " invalid-request\n");
            index = index * 10 + (unsigned)(*p++ - '0');
        }
        if (index > 50 || gw_network_scan_result(index, ssid, sizeof(ssid),
                                                 &signal_dbm, &secure))
            return write_line(fd, "NET END\n");
        snprintf(resp, sizeof(resp), "NET SSID %s %d %d\n", ssid, signal_dbm,
                 secure);
        return write_line(fd, resp);
    }
#endif
    if (strncmp(line, GW_CMD_NET_CONNECT " ", sizeof(GW_CMD_NET_CONNECT " ") - 1) == 0) {
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
        const char *ssid = line + sizeof(GW_CMD_NET_CONNECT " ") - 1;
        const char *separator;
        if (g_wifi_pid > 0)
            return write_line(fd, GW_RESP_ERR " unavailable\n");
        separator = strchr(ssid, ' ');
        if (!separator || strchr(separator + 1, ' ') ||
            gw_network_save(ssid, (size_t)(separator - ssid), separator + 1, strlen(separator + 1)))
            return write_line(fd, GW_RESP_ERR " invalid-or-unavailable\n");
        if (gw_network_enable_hotplug()) return write_line(fd, GW_RESP_ERR " unavailable\n");
        g_wifi_enabled = 1;
        if (gw_config_save_int(GW_WIFI_ENABLED_PATH, "enabled", 1) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        g_wifi_error = 0;
        g_wifi_state = GW_WIFI_STATE_CONNECTING;
        g_wifi_retry_delay = 5;
        g_wifi_retry_at.tv_sec = 0;
        g_wifi_retry_at.tv_nsec = 0;
        g_wifi_pid = start_wifi("start");
        return write_line(fd, g_wifi_pid > 0 ? GW_RESP_OK "\n" : GW_RESP_ERR " unavailable\n");
#else
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
#endif
    }
    if (strcmp(line, GW_CMD_NET_DISCO) == 0) {
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
        if (g_wifi_pid > 0)
            return write_line(fd, GW_RESP_ERR " unavailable\n");
        /* Manual disconnect also turns auto-reconnect off, but retains the
         * saved credentials so Connect/Enable can reuse them. */
        g_wifi_enabled = 0;
        if (gw_config_save_int(GW_WIFI_ENABLED_PATH, "enabled", 0) < 0)
            return write_line(fd, GW_RESP_ERR " persist-failed\n");
        g_wifi_retry_at.tv_sec = 0;
        g_wifi_retry_at.tv_nsec = 0;
        g_wifi_error = 0;
        g_wifi_state = GW_WIFI_STATE_OFF;
        g_net_connected = 0;
        g_wifi_pid = start_wifi("stop");
        return write_line(fd, g_wifi_pid > 0 ? GW_RESP_OK "\n" : GW_RESP_ERR " unavailable\n");
#else
        return write_line(fd, GW_RESP_ERR " not-accepted\n");
#endif
    }
    if (strcmp(line, GW_CMD_NET_STATUS) == 0) {
#if GW_LIFECYCLE_SPIKE && (GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED)
        g_net_connected = gw_network_status();
#endif
        if (g_net_connected == 1) {
            char ssid[65], ip[32];
            if (gw_network_saved_ssid(ssid, sizeof(ssid)) == 0 &&
                gw_network_ip(ip, sizeof(ip)) == 0)
                snprintf(resp, sizeof(resp), "NET connected %s %s\n", ssid, ip);
            else if (gw_network_saved_ssid(ssid, sizeof(ssid)) == 0)
                snprintf(resp, sizeof(resp), "NET connected %s\n", ssid);
            else
                snprintf(resp, sizeof(resp), "NET connected\n");
        } else if (g_net_connected == 0)
            snprintf(resp, sizeof(resp), "NET disconnected\n");
        else
            snprintf(resp, sizeof(resp), "NET unknown\n");
        return write_line(fd, resp);
    }

    return write_line(fd, GW_RESP_ERR " unknown-command\n");
}

/* ── I/O helpers ─────────────────────────────────────────────────────────── */

static int write_line(int fd, const char *msg) {
    size_t left = strlen(msg);
    const char *p = msg;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        p += n; left -= (size_t)n;
    }
    return 0;
}

/* ── Cleanup ─────────────────────────────────────────────────────────────── */

static void cleanup(void) {
    int i;
    for (i = 0; i < GW_IPC_MAX_CLIENTS; i++)
        if (g_evt_clients[i] >= 0) { close(g_evt_clients[i]); g_evt_clients[i] = -1; }
#if GW_LIFECYCLE_SPIKE
    if (g_shell_pid > 0) { kill(g_shell_pid, SIGTERM); waitpid(g_shell_pid, NULL, WNOHANG); }
    if (g_reader_pid > 0) { kill(g_reader_pid, SIGTERM); waitpid(g_reader_pid, NULL, WNOHANG); }
#endif
    if (g_cmd_fd >= 0) { close(g_cmd_fd); g_cmd_fd = -1; }
    if (g_evt_fd >= 0) { close(g_evt_fd); g_evt_fd = -1; }
    if (g_btn_fd >= 0) { close(g_btn_fd); g_btn_fd = -1; }
    if (g_sigchld_fd >= 0) { close(g_sigchld_fd); g_sigchld_fd = -1; }
    if (g_sigchld_wr >= 0) { close(g_sigchld_wr); g_sigchld_wr = -1; }
    unlink(GW_IPC_CMD_SOCKET);
    unlink(GW_IPC_EVT_SOCKET);
    if (g_lock_fd >= 0) { close(g_lock_fd); g_lock_fd = -1; }
}

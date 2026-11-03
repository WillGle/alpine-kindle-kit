/* SPDX-License-Identifier: GPL-3.0-or-later
 * Power/display/suspend helpers for gw-system. */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#ifndef GW_LIFECYCLE_SPIKE
#define GW_LIFECYCLE_SPIKE 1
#endif
#ifndef GW_ENABLE_HW_TRIAL
#define GW_ENABLE_HW_TRIAL 0
#endif
#ifndef GW_WIFI_ACCEPTED
#define GW_WIFI_ACCEPTED 0
#endif
#ifndef GW_POWER_LIGHT_ACCEPTED
#define GW_POWER_LIGHT_ACCEPTED 0
#endif

#include "gw-system-power.h"
#include "gw-network.h"
#include "gw-stage3.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if GW_LIFECYCLE_SPIKE

#define GW_SUSPEND_FALLBACK_SECONDS (15 * 60)
#define GW_SUSPEND_RETRY_MS 5000

int g_suspend_fallback = GW_SUSPEND_FALLBACK_SECONDS;

pid_t start_child(const char *path, int reader) {
    pid_t pid = fork();
    if (pid == 0) {
        if (reader) setenv("GW_MANAGED_READER", "1", 1);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    return pid;
}

void display_light(int pct) {
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

static void gw_wall_stamp(char *buf, size_t size) {
    struct timespec ts;
    struct tm tm;
    if (!buf || size == 0) return;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0 ||
        localtime_r(&ts.tv_sec, &tm) == NULL ||
        strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm) == 0) {
        snprintf(buf, size, "unknown");
    }
}

/* Includes suspend time; CLOCK_MONOTONIC does not. */
static double gw_boot_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0) return -1.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void display_off(void) {
    int now = gw_power_read_light();
    char stamp[32];
    gw_wall_stamp(stamp, sizeof(stamp));
    if (now > 0) g_last_light = now;
    if (g_last_light <= 0) g_last_light = GW_DISPLAY_DEFAULT_LIGHT;
    display_light(0);
    g_display_on = 0;
    g_locked = 1;
    g_pending_reader = 0;
    event_broadcast(GW_EVT_PWR_LOCK "\n");
    dprintf(2, "GW_SYSTEM: display off wall=%s (saved light=%d)\n",
            stamp, g_last_light);
}

void display_wake(void) {
    int level = g_last_light > 0 ? g_last_light : GW_DISPLAY_DEFAULT_LIGHT;
    char stamp[32];
    gw_wall_stamp(stamp, sizeof(stamp));
    display_light(level);
    g_display_on = 1;
    event_broadcast(GW_EVT_PWR_WAKE "\n");
    dprintf(2, "GW_SYSTEM: display on wall=%s (light=%d lock=%s)\n",
            stamp, level, g_locked ? "locked" : "unlocked");
}

long gw_ms_until(const struct timespec *when) {
    struct timespec now;
    long ms;
    if (!when || (when->tv_sec == 0 && when->tv_nsec == 0)) return -1;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    ms = (long)(when->tv_sec - now.tv_sec) * 1000L +
         (long)(when->tv_nsec - now.tv_nsec) / 1000000L;
    return ms < 0 ? 0 : ms;
}

void gw_schedule_sleep(int delay_ms) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return;
    now.tv_sec += delay_ms / 1000;
    now.tv_nsec += (long)(delay_ms % 1000) * 1000000L;
    if (now.tv_nsec >= 1000000000L) { now.tv_sec++; now.tv_nsec -= 1000000000L; }
    g_sleep_at = now;
    dprintf(2, "GW_SYSTEM: suspend scheduled in %d ms (lock=%d display=%d)\n",
            delay_ms, g_locked, g_display_on);
}

void gw_cancel_sleep(void) {
    if (g_sleep_at.tv_sec || g_sleep_at.tv_nsec) {
        g_sleep_at.tv_sec = 0;
        g_sleep_at.tv_nsec = 0;
        dprintf(2, "GW_SYSTEM: suspend cancelled\n");
    }
}

void gw_fill_power_state(gw_power_state_t *state) {
    if (!state) return;
    state->display_on = g_display_on;
    state->locked = g_locked;
    state->usb = gw_power_read_usb();
    state->charging = state->usb == GW_USB_NONE ? 0 : gw_power_read_charging();
}

void gw_do_suspend(int force, int fallback_seconds) {
    gw_power_state_t state;
    char stamp[32];
    double before = -1.0, after = -1.0, slept;
    int rc, pressed = 0;

    gw_cancel_sleep();
    gw_fill_power_state(&state);
    gw_wall_stamp(stamp, sizeof(stamp));
    if (!force && !gw_power_should_suspend(&state)) {
        dprintf(2, "GW_SYSTEM: suspend skipped wall=%s (display=%d usb=%d charging=%d)\n",
                stamp, state.display_on, state.usb, state.charging);
        return;
    }
    if (state.display_on) display_off();

    event_broadcast(GW_EVT_PWR_SUSPEND "\n");
    sync();

    if (fallback_seconds <= 0) fallback_seconds = GW_SUSPEND_FALLBACK_SECONDS;
    if (gw_power_rtc_arm(fallback_seconds) < 0)
        dprintf(2, "GW_SYSTEM: RTC fallback alarm unavailable: %s\n", strerror(errno));

    g_suspended = 1;
    before = gw_boot_seconds();
    gw_wall_stamp(stamp, sizeof(stamp));
    dprintf(2, "GW_SYSTEM: entering mem suspend wall=%s (usb=%d charging=%d)\n",
            stamp, state.usb, state.charging);
    rc = gw_power_suspend();
    after = gw_boot_seconds();
    g_suspended = 0;
    gw_power_rtc_clear();

    slept = (before >= 0.0 && after >= 0.0) ? after - before : -1.0;
    gw_wall_stamp(stamp, sizeof(stamp));
    if (rc < 0) {
        dprintf(2, "GW_SYSTEM: suspend failed wall=%s slept=%.1fs: %s\n",
                stamp, slept, strerror(errno));
        return;
    }

    gw_power_drain_button(g_btn_fd, &pressed);
    event_broadcast(GW_EVT_PWR_RESUME "\n");
    if (pressed) {
        dprintf(2, "GW_SYSTEM: resumed by power button wall=%s slept=%.1fs\n",
                stamp, slept);
        display_wake();
    } else {
        dprintf(2, "GW_SYSTEM: resumed without button wall=%s slept=%.1fs\n",
                stamp, slept);
        gw_fill_power_state(&state);
        if (!state.display_on && gw_power_should_suspend(&state))
            gw_schedule_sleep(GW_SUSPEND_RETRY_MS);
    }
}

#if GW_ENABLE_HW_TRIAL || GW_WIFI_ACCEPTED
pid_t start_wifi(const char *action) {
    pid_t pid = fork();
    if (pid == 0) {
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) { dup2(nullfd, STDOUT_FILENO); dup2(nullfd, STDERR_FILENO); close(nullfd); }
        execl("/etc/gleworks-wifi.sh", "/etc/gleworks-wifi.sh", action, (char *)NULL);
        _exit(127);
    }
    return pid;
}

void gw_wifi_supervise(void) {
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
#endif

#if GW_LIFECYCLE_SPIKE
void gw_power_sample(int startup) {
    struct timespec now;
    gw_power_state_t state;
    char stamp[32];
    int usb = gw_power_read_usb();
    int charging = gw_power_read_charging();
    int usb_changed = !startup && usb != g_usb_state;

    if (usb == GW_USB_NONE) charging = 0;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return;
    state.display_on = g_display_on;
    state.locked = g_locked;
    state.usb = usb;
    state.charging = charging;

    if (usb_changed) {
        event_broadcast(usb == GW_USB_NONE ? GW_EVT_USB_DISCONNECTED "\n"
                                          : GW_EVT_USB_CONNECTED "\n");
        gw_wall_stamp(stamp, sizeof(stamp));
        dprintf(2, "GW_SYSTEM: USB state %d -> %d wall=%s\n",
                g_usb_state, usb, stamp);
    }
    if (!startup && gw_power_charging_changed(g_charging_state, charging))
        event_broadcast(charging ? GW_EVT_PWR_CHARGING "\n"
                                 : GW_EVT_PWR_DISCHARG "\n");

    g_usb_state = usb;
    g_charging_state = charging;
    g_charging = charging;

    if (usb_changed && usb == GW_USB_NONE &&
        gw_power_on_usb_change(&state) == GW_POWER_ACTION_SUSPEND &&
        !g_suspended && gw_ms_until(&g_sleep_at) < 0)
        gw_schedule_sleep(3000);

    g_power_poll_at = now;
    g_power_poll_at.tv_sec += 2;
}
#endif

#if GW_LIFECYCLE_SPIKE
void reap_children(void) {
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
#endif

/* SPDX-License-Identifier: GPL-3.0-or-later
 * PW3 hardware power/light/button probes — system service internal only.
 *
 * These functions read PW3-specific sysfs nodes and input event devices.
 * They return -1 on failure; the caller decides whether failure is fatal.
 *
 * None of these paths are called from UI or app code. The service owns all
 * hardware access; UI queries status via the IPC socket.
 *
 * STATUS: CORE (in use).
 *
 * CURRENT STATUS (2026-10-30): probe paths documented from dry-run evidence
 * (kernel_pm_devices_dry_run=true, kernel_mem_rtc_20s_same_boot=true).
 * mem-suspend write and frontlight write are present but guarded by
 * GW_POWER_SUSPEND_ACCEPTED / GW_POWER_LIGHT_ACCEPTED compile-time flags
 * (both 0 until hardware acceptance is recorded in ACCEPTANCE.json).  A
 * bring-up build may set GW_ENABLE_HW_TRIAL=1; that enables the write path for
 * a trial but never means accepted.
 */
#ifndef GW_POWER_H
#define GW_POWER_H

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

/* Set to 1 only after real-device acceptance for each subsystem. */
#ifndef GW_POWER_SUSPEND_ACCEPTED
#define GW_POWER_SUSPEND_ACCEPTED 0
#endif
#ifndef GW_POWER_LIGHT_ACCEPTED
#define GW_POWER_LIGHT_ACCEPTED 0
#endif
#ifndef GW_POWER_POWEROFF_ACCEPTED
#define GW_POWER_POWEROFF_ACCEPTED 0
#endif
/* Explicit trial gate; never implies acceptance and defaults off. */
#ifndef GW_ENABLE_HW_TRIAL
#define GW_ENABLE_HW_TRIAL 0
#endif

/* PW3 sysfs paths — verified against 3.0.35-lab126 in dry-run evidence.
 * Overridable so the host tests can exercise the exact I/O helpers against
 * temporary files instead of the live kernel. */
#ifndef GW_SYSFS_BATTERY
#define GW_SYSFS_BATTERY  "/sys/class/power_supply/max77696-battery/capacity"
#endif
#ifndef GW_SYSFS_CHARGING
#define GW_SYSFS_CHARGING "/sys/class/power_supply/max77696-charger/online"
#endif
#ifndef GW_SYSFS_LIGHT_LV
#define GW_SYSFS_LIGHT_LV "/sys/class/backlight/max77696-bl/brightness"
#endif
#ifndef GW_SYSFS_PM_STATE
#define GW_SYSFS_PM_STATE "/sys/power/state"
#endif
#ifndef GW_SYSFS_WAKEUP
#define GW_SYSFS_WAKEUP   "/sys/devices/platform/fsl-usb2-udc/power/wakeup"
#endif
#ifndef GW_SYSFS_WAKEUP_COUNT
#define GW_SYSFS_WAKEUP_COUNT "/sys/power/wakeup_count"
#endif
#ifndef GW_SYSFS_RTC_WAKEALARM
#define GW_SYSFS_RTC_WAKEALARM "/sys/class/rtc/rtc0/wakealarm"
#endif
#ifndef GW_SYSFS_RTC_EPOCH
#define GW_SYSFS_RTC_EPOCH     "/sys/class/rtc/rtc0/since_epoch"
#endif
#ifndef GW_USB_STATE
#define GW_USB_STATE      "/proc/asession"
#endif

/* Bounded retries when the kernel rejects a stale wakeup_count sample. */
#define GW_SUSPEND_RETRIES 3

/* evdev path for power button on PW3. */
#define GW_BTN_EVENT_GLOB "/dev/input/event"
#define GW_BTN_KEY_DOWN 116  /* KEY_POWER */
#define GW_BTN_KEY_UP   117

/* Read a small integer from a sysfs node. Returns value or -1 on error. */
static inline int gw_sysfs_read_int(const char *path) {
    char buf[16]; char *end; long value; int fd; ssize_t n;
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0 || n == (ssize_t)(sizeof(buf) - 1)) return -1;
    buf[n] = '\0';
    errno = 0;
    value = strtol(buf, &end, 10);
    if (errno || end == buf || value < 0 || value > INT_MAX) return -1;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    return *end ? -1 : (int)value;
}

/* Write a string to a sysfs node. Returns 0 on success, -1 on error. */
static inline int gw_sysfs_write(const char *path, const char *val) {
    int fd; ssize_t n; size_t len = strlen(val);
    fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    n = write(fd, val, len);
    close(fd);
    return (n == (ssize_t)len) ? 0 : -1;
}

/* ── Battery ─────────────────────────────────────────────────────────────── */

static inline int gw_power_read_battery(void) {
    return gw_sysfs_read_int(GW_SYSFS_BATTERY);
}

static inline int gw_power_read_charging(void) {
    int online = gw_sysfs_read_int(GW_SYSFS_CHARGING);
    return online < 0 ? -1 : online > 0;
}

/* 0: no cable, 1: other power, 2: USB host charging class, -1: unknown.
 * The host class does not prove that a USB console has enumerated. */
static inline int gw_power_parse_usb(FILE *file) {
    char line[160], extra;
    int connected = -1, host = -1, value;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "max77696_charger_get_connected()", sizeof("max77696_charger_get_connected()") - 1) == 0) {
            if (sscanf(line, "max77696_charger_get_connected() %d %c", &value, &extra) != 1 ||
                connected != -1 || (value != 0 && value != 1)) return -1;
            connected = value;
        } else if (strncmp(line, "max77696_uic_is_usbhost()", sizeof("max77696_uic_is_usbhost()") - 1) == 0) {
            if (sscanf(line, "max77696_uic_is_usbhost() %d %c", &value, &extra) != 1 ||
                host != -1 || (value != 0 && value != 1)) return -1;
            host = value;
        }
    }
    if (ferror(file) || connected < 0 || host < 0 || (!connected && host)) return -1;
    return !connected ? 0 : host ? 2 : 1;
}

static inline int gw_power_read_usb(void) {
    /* Root-only diagnostic override used by the uinput acceptance run; it is
     * never set by the normal boot path. */
    const char *override = getenv("GW_POWER_USB_OVERRIDE");
    FILE *file;
    int value;
    if (override && *override) {
        value = atoi(override);
        if (value >= -1 && value <= 2) return value;
    }
    file = fopen(GW_USB_STATE, "r");
    if (!file) return -1;
    value = gw_power_parse_usb(file);
    fclose(file);
    return value;
}

/* ── Frontlight ──────────────────────────────────────────────────────────── */

static inline int gw_power_read_light(void) {
    int raw = gw_sysfs_read_int(GW_SYSFS_LIGHT_LV);
    if (raw < 0 || raw > 4095) return -1;
    return (raw * 100 + 2047) / 4095;
}

/*
 * Set frontlight. Only compiled active when GW_POWER_LIGHT_ACCEPTED=1.
 * Returns 0 on success, -1 if not accepted or write failed.
 */
static inline int gw_power_set_light(int pct) {
#if GW_POWER_LIGHT_ACCEPTED || GW_ENABLE_HW_TRIAL
    char buf[8];
    int value = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    snprintf(buf, sizeof(buf), "%d\n", (value * 4095 + 50) / 100);
    return gw_sysfs_write(GW_SYSFS_LIGHT_LV, buf);
#else
    (void)pct;
    errno = ENOTSUP;
    return -1;
#endif
}

/* ── Power button evdev ──────────────────────────────────────────────────── */

/*
 * Find the power-button input device by scanning event nodes for EV_KEY /
 * KEY_POWER. Returns an open fd or -1.
 */
static inline int gw_power_open_button(int exclusive) {
    char path[48]; int n;
    /* Root-only diagnostic override: point the service at a uinput evdev node
     * for the physical-path acceptance run. The node is still validated. */
    const char *override = getenv("GW_POWER_BUTTON_DEV");
    if (override && *override) {
        unsigned long evbits = 0;
        unsigned long keybits[KEY_MAX / (8*sizeof(long)) + 1] = {0};
        int fd = open(override, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) return -1;
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) >= 0 &&
            (evbits & (1UL << EV_KEY)) &&
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0 &&
            (keybits[KEY_POWER / (8*sizeof(long))] &
             (1UL << (KEY_POWER % (8*sizeof(long)))))) {
            if (!exclusive || ioctl(fd, EVIOCGRAB, 1) >= 0) return fd;
        }
        close(fd);
        errno = ENODEV;
        return -1;
    }
    for (n = 0; n < 8; n++) {
        unsigned long evbits = 0; unsigned long keybits[KEY_MAX / (8*sizeof(long)) + 1] = {0};
        snprintf(path, sizeof(path), "%s%d", GW_BTN_EVENT_GLOB, n);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) >= 0 &&
            (evbits & (1UL << EV_KEY)) &&
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0 &&
            (keybits[KEY_POWER / (8*sizeof(long))] &
             (1UL << (KEY_POWER % (8*sizeof(long)))))) {
            if (exclusive && ioctl(fd, EVIOCGRAB, 1) < 0) {
                close(fd);
                continue;
            }
            return fd;
        }
        close(fd);
    }
    errno = ENODEV;
    return -1;
}

/*
 * Read one input event. Returns KEY_POWER down/up, 0 for other events,
 * and -1 on error/EOF.
 */
static inline int gw_power_read_button_event(int fd) {
    struct input_event ev;
    ssize_t n = read(fd, &ev, sizeof(ev));
    if (n != (ssize_t)sizeof(ev)) return -1;
    if (ev.type == EV_KEY && ev.code == KEY_POWER && ev.value == 1)
        return GW_BTN_KEY_DOWN;
    if (ev.type == EV_KEY && ev.code == KEY_POWER && ev.value == 0)
        return GW_BTN_KEY_UP;
    return 0;
}

/*
 * Drain queued input events after a resume. Returns the number of events read
 * and sets *power_pressed when any KEY_POWER down/up was observed. This is
 * used to distinguish a button wake from an RTC/timeout wake without blocking.
 */
static inline int gw_power_drain_button(int fd, int *power_pressed) {
    struct input_event ev;
    int count = 0;
    if (power_pressed) *power_pressed = 0;
    if (fd < 0) return -1;
    while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        count++;
        if (ev.type == EV_KEY && ev.code == KEY_POWER && power_pressed)
            *power_pressed = 1;
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) return count ? count : -1;
    return count;
}

/* ── RTC wake alarm ──────────────────────────────────────────────────────── */

/* The RTC reports and accepts seconds since the Unix epoch. */
static inline int gw_power_read_i64(const char *path, int64_t *out) {
    char buf[32], *end;
    long long value;
    int fd, n;
    if (!out) return -1;
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    n = (int)read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0 || n >= (int)sizeof(buf) - 1) return -1;
    buf[n] = '\0';
    errno = 0;
    value = strtoll(buf, &end, 10);
    if (errno || end == buf) return -1;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end) return -1;
    *out = (int64_t)value;
    return 0;
}

/*
 * Arm a one-shot RTC wake alarm `seconds` from the RTC's own epoch.
 * Returns 0 after the alarm value reads back as expected. The service uses a
 * bounded fallback alarm so a missed button/USB wake cannot strand the device
 * asleep; the alarm is cleared on every normal resume.
 */
static inline int gw_power_rtc_arm(int seconds) {
    int64_t now, expected;
    char buf[32];
    if (seconds <= 0 || seconds > 24 * 60 * 60) { errno = EINVAL; return -1; }
    if (gw_power_read_i64(GW_SYSFS_RTC_EPOCH, &now) < 0) return -1;
    expected = now + seconds;
    /* Clearing first is required by the Linux RTC sysfs contract. */
    if (gw_sysfs_write(GW_SYSFS_RTC_WAKEALARM, "0\n") < 0) return -1;
    snprintf(buf, sizeof(buf), "%lld\n", (long long)expected);
    if (gw_sysfs_write(GW_SYSFS_RTC_WAKEALARM, buf) < 0) return -1;
    {
        int64_t readback;
        if (gw_power_read_i64(GW_SYSFS_RTC_WAKEALARM, &readback) < 0 ||
            readback != expected) {
            errno = EIO;
            return -1;
        }
    }
    return 0;
}

static inline int gw_power_rtc_clear(void) {
    /* Writing 0 disarms and clears a pending alarm on Linux RTC sysfs. */
    return gw_sysfs_write(GW_SYSFS_RTC_WAKEALARM, "0\n");
}

/* ── Suspend ─────────────────────────────────────────────────────────────── */

/*
 * Suspend to RAM with the kernel wakeup_count handshake.
 *
 * A stale count sample makes the kernel refuse "mem" with EAGAIN; retrying
 * with a fresh sample avoids sleeping while a wakeup event is racing. Only
 * compiled active when GW_POWER_SUSPEND_ACCEPTED=1 and called after the EPDC
 * refresh, app save and lock step. The 40-second hardware reset via
 * gleworks_pid1_reset remains independent and is never disabled here.
 */
static inline int gw_power_suspend(void) {
#if GW_POWER_SUSPEND_ACCEPTED || GW_ENABLE_HW_TRIAL
    int attempt;
    for (attempt = 0; attempt < GW_SUSPEND_RETRIES; attempt++) {
        char count[32];
        int fd, n;
        fd = open(GW_SYSFS_WAKEUP_COUNT, O_RDONLY | O_CLOEXEC);
        if (fd < 0) return -1;
        n = (int)read(fd, count, sizeof(count) - 1);
        close(fd);
        if (n <= 0 || n >= (int)sizeof(count) - 1) return -1;
        count[n] = '\0';
        /* The written value must be the exact string just read. */
        fd = open(GW_SYSFS_WAKEUP_COUNT, O_WRONLY | O_CLOEXEC);
        if (fd < 0) return -1;
        n = (int)write(fd, count, strlen(count));
        close(fd);
        if (n != (int)strlen(count)) {
            if (errno == EAGAIN || errno == EINTR) continue;
            return -1;
        }
        return gw_sysfs_write(GW_SYSFS_PM_STATE, "mem\n");
    }
    errno = EAGAIN;
    return -1;
#else
    errno = ENOTSUP;
    return -1;
#endif
}

#endif /* GW_POWER_H */

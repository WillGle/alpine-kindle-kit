/* SPDX-License-Identifier: GPL-3.0-or-later
 * Console fixture replacing only the graphical shell in the disposable VM.
 * Requests originate from the supervised shell PID, as on the real device.
 */
#define _GNU_SOURCE
#include "gw-ipc-client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

/* VM adapter only: the real supervisor still requires a valid evdev button. */
static int input_fixture(void) {
    struct uinput_user_dev device;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) return 1;
    memset(&device, 0, sizeof(device));
    snprintf(device.name, sizeof(device.name), "Alpine core VM power button");
    device.id.bustype = BUS_VIRTUAL;
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) || ioctl(fd, UI_SET_KEYBIT, KEY_POWER) ||
        write(fd, &device, sizeof(device)) != sizeof(device) ||
        ioctl(fd, UI_DEV_CREATE)) return 1;
    int touch = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (touch < 0) return 1;
    memset(&device, 0, sizeof(device));
    snprintf(device.name, sizeof(device.name), "Alpine core VM touch");
    device.id.bustype = BUS_VIRTUAL;
    device.absmax[ABS_MT_POSITION_X] = 1071;
    device.absmax[ABS_MT_POSITION_Y] = 1447;
    if (ioctl(touch, UI_SET_EVBIT, EV_ABS) || ioctl(touch, UI_SET_EVBIT, EV_KEY) ||
        ioctl(touch, UI_SET_KEYBIT, BTN_TOUCH) ||
        ioctl(touch, UI_SET_ABSBIT, ABS_MT_POSITION_X) ||
        ioctl(touch, UI_SET_ABSBIT, ABS_MT_POSITION_Y) ||
        write(touch, &device, sizeof(device)) != sizeof(device) ||
        ioctl(touch, UI_DEV_CREATE)) return 1;
    FILE *ready = fopen("/run/input-fixture-ready", "w");
    if (!ready) return 1;
    fclose(ready);
    for (;;) pause();
}

static void check(int ok, const char *name) {
    printf("GW_CORE_CHECK_%s %s\n", ok ? "PASS" : "FAIL", name);
    fflush(stdout);
    if (!ok) { FILE *f = fopen("/run/check-failed", "w"); if (f) fclose(f); exit(1); }
}

static int tool(const char *command) { return system(command) == 0; }

static void launch(const char *id, int next) {
    gw_ipc_conn_t conn;
    char command[128], response[512];
    FILE *f;
    gw_ipc_conn_init(&conn);
    snprintf(command, sizeof(command), "APP LAUNCH %s", id);
    check(gw_ipc_conn_cmd(&conn, command, response, sizeof(response)) == 0 &&
          !strcmp(response, "OK\n"), "app-launch-accepted");
    gw_ipc_conn_close(&conn);
    f = fopen("/run/check-phase", "w");
    check(f != NULL, "phase-record");
    fprintf(f, "%d\n", next); fclose(f);
    /* Match gw-shell: a pending handoff is accepted only after a clean exit. */
    exit(0);
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--input")) return input_fixture();
    int fd = open("/dev/console", O_WRONLY), phase = 0;
    FILE *f;
    if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }
    f = fopen("/run/check-phase", "r");
    if (f) { (void)fscanf(f, "%d", &phase); fclose(f); }
    if (phase == 0) {
        check(access("/opt/koreader", F_OK) < 0 &&
              access("/usr/sbin/tailscaled", F_OK) < 0, "no-personal-apps-in-core");
        check(tool("gw-appctl list | grep -qx 'APP END'"), "empty-app-list");
        check(tool("gw-appctl install /vm/probe-1.gwapp"), "signed-install-v1");
        launch("qemu-probe", 1);
    } else if (phase == 1) {
        check(access("/run/app-stopped", F_OK) == 0, "external-app-stop");
        check(tool("grep -qx v1 /var/lib/gleworks/appdata/qemu-probe/version"), "app-data-v1");
        check(tool("gw-appctl install /vm/probe-2.gwapp"), "signed-update-v2");
        check(access("/var/lib/gleworks/apps/qemu-probe/1.0/payload/run", X_OK) == 0,
              "previous-version-retained");
        launch("qemu-probe", 2);
    } else if (phase == 2) {
        check(tool("gwctl status | grep -q 'reader-exit=23'"), "app-failure-service-survives");
        check(!tool("gw-appctl install /vm/broken.gwapp"), "tampered-update-rejected");
        check(tool("test \"$(readlink /var/lib/gleworks/apps/qemu-probe/active)\" = 2.0"),
              "rejected-update-keeps-active");
        check(tool("gw-appctl install /vm/crash.gwapp"), "crashing-app-install");
        launch("qemu-crash", 5);
    } else if (phase == 5) {
        check(tool("gwctl status | grep -q 'reader-exit=128'"), "signalled-app-service-survives");
        check(tool("gw-appctl remove qemu-crash"), "crashed-app-remove");
        check(tool("cd /var/lib/gleworks/apps/qemu-probe && ln -s 1.0 rollback && mv -Tf rollback active"),
              "manual-app-version-rollback");
        check(tool("touch /run/rollback"), "rollback-mode");
        launch("qemu-probe", 3);
    } else if (phase == 3) {
        check(tool("grep -qx v1 /var/lib/gleworks/appdata/qemu-probe/version"), "rollback-v1-runs");
        check(tool("gw-appctl remove qemu-probe"), "remove-app");
        check(access("/var/lib/gleworks/appdata/qemu-probe/version", F_OK) == 0,
              "remove-retains-data");
        check(tool("gw-appctl install /vm/reader.gwapp"), "optional-reader-id-install");
        launch("koreader", 4);
    } else if (phase == 4) {
        check(access("/run/reader-fixture-ran", F_OK) == 0, "reader-id-generic-launch");
        check(tool("gw-appctl remove koreader"), "optional-reader-id-remove");
        check(tool("gw-appctl list | grep -qx 'APP END'"), "empty-app-list-after-remove");
        check(tool("touch /run/check-complete"), "suite-complete");
        execl("/vm/gw-shell-real", "/vm/gw-shell-real", (char *)NULL);
        check(0, "real-shell-exec");
    }
    for (;;) pause();
}

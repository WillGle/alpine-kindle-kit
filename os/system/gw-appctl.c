/* SPDX-License-Identifier: GPL-3.0-or-later
 * Command-line app bundle manager; the system service uses the same library
 * in-process, and this tool exists for maintenance and USB workflows.
 * STATUS: CORE (in use). */
#define _GNU_SOURCE
#include "gw-apps.h"
#include "gw-signature.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
    fprintf(stderr,
            "usage: gw-appctl list\n"
            "       gw-appctl find <id>\n"
            "       gw-appctl install <package.gwapp> [signature]\n"
            "       gw-appctl remove <id>\n"
            "       gw-appctl scan\n"
            "       gw-appctl verify <package.gwapp> [signature]\n");
}

int main(int argc, char **argv) {
    char err[160];
    int i;
    if (argc < 2) { usage(); return 2; }

    if (!strcmp(argv[1], "list")) {
        gw_app_info_t apps[GW_APPS_MAX];
        int count = gw_apps_list(apps, GW_APPS_MAX);
        if (count < 0) { puts("ERR list-failed"); return 1; }
        for (i = 0; i < count; i++)
            printf("APP %s %s %s\n", apps[i].id, apps[i].version,
                   apps[i].description[0] ? apps[i].description : "-");
        return puts("APP END") == EOF ? 1 : 0;
    }
    if (!strcmp(argv[1], "find") && argc == 3) {
        gw_app_info_t app;
        if (gw_apps_find(argv[2], &app) < 0) { puts("ERR not-installed"); return 1; }
        printf("APP %s %s %s\n", app.id, app.version,
               app.description[0] ? app.description : "-");
        return 0;
    }
    if (!strcmp(argv[1], "scan")) {
        char names[GW_APPS_MAX][GW_APP_NAME_MAX];
        int count = gw_apps_usb_scan(names, GW_APPS_MAX);
        if (count < 0) { puts("ERR scan-unavailable"); return 1; }
        for (i = 0; i < count; i++) printf("USB %s\n", names[i]);
        puts("USB END");
        return 0;
    }
    if (!strcmp(argv[1], "verify") && (argc == 3 || argc == 4)) {
        const char *sig = argc == 4 ? argv[3] : NULL;
        char path[512];
        if (!sig) {
            snprintf(path, sizeof(path), "%s.sig", argv[2]);
            sig = path;
        }
        err[0] = '\0';
        if (gw_signature_verify_file(argv[2], sig, GW_APP_KEY_PATH,
                                     err, sizeof(err)) == 0) {
            puts("OK");
            return 0;
        }
        printf("ERR %s\n", err[0] ? err : "invalid");
        return 1;
    }
    if (geteuid() != 0) { puts("ERR permission-denied"); return 1; }
    if (!strcmp(argv[1], "install") && (argc == 3 || argc == 4)) {
        err[0] = '\0';
        if (gw_apps_install(argv[2], argc == 4 ? argv[3] : NULL,
                            err, sizeof(err)) == 0) {
            puts("OK");
            return 0;
        }
        printf("ERR %s\n", err[0] ? err : "install-failed");
        return 1;
    }
    if (!strcmp(argv[1], "remove") && argc == 3) {
        err[0] = '\0';
        if (gw_apps_remove(argv[2], err, sizeof(err)) == 0) {
            puts("OK");
            return 0;
        }
        printf("ERR %s\n", err[0] ? err : "remove-failed");
        return 1;
    }
    usage();
    return 2;
}

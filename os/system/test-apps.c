/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host tests for app version selection and manifest enforcement. */
#include "gw-apps.h"
#include "gw-app-manifest.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void) {
    /* Newest bundle wins regardless of directory order. */
    CHECK(gw_apps_version_compare("1.10.0", "1.9.0") > 0);
    CHECK(gw_apps_version_compare("1.9.0", "1.10.0") < 0);
    CHECK(gw_apps_version_compare("1.0", "1.0.0") == 0);
    CHECK(gw_apps_version_compare("2.0.0", "1.99.99") > 0);
    CHECK(gw_apps_version_compare("1.0.0-beta", "1.0.0") < 0);
    CHECK(gw_apps_version_compare("1.2.3.4", "1.2.3") > 0);

    /* Valid manifest, including empty optional lists. */
    {
        static const char text[] =
            "id=notes\nversion=1.2.0\nentrypoint=payload/notes\n"
            "formats=epub,pdf\npermissions=display,touch\n"
            "arch=armv7\nprofile=pw3-0kb-5.16.2.1.1\nmin_os=0.1\n"
            "description=Quick notes\n";
        gw_app_manifest_t app;
        CHECK(gw_app_manifest_parse(text, sizeof(text) - 1, &app) == 0);
        CHECK(!strcmp(app.id, "notes"));
        CHECK(!strcmp(app.version, "1.2.0"));
        CHECK(app.format_count == 2 && app.perm_count == 2);
        CHECK(gw_app_manifest_compatible(&app, "0.1") == 1);
        CHECK(gw_app_manifest_compatible(&app, "0.0.9") == 0);
    }

    /* Missing keys, duplicates, absolute paths and bad ids are refused. */
    {
        gw_app_manifest_t app;
        static const char missing[] = "id=x\nversion=1\n";
        static const char duplicate[] =
            "id=x\nid=y\nversion=1\nentrypoint=a\nformats=\npermissions=\n"
            "arch=armv7\nprofile=pw3-0kb-5.16.2.1.1\nmin_os=0.1\n";
        static const char absolute[] =
            "id=x\nversion=1\nentrypoint=/bin/sh\nformats=\npermissions=\n"
            "arch=armv7\nprofile=pw3-0kb-5.16.2.1.1\nmin_os=0.1\n";
        static const char traversal[] =
            "id=x\nversion=1\nentrypoint=../x\nformats=\npermissions=\n"
            "arch=armv7\nprofile=pw3-0kb-5.16.2.1.1\nmin_os=0.1\n";
        CHECK(gw_app_manifest_parse(missing, sizeof(missing) - 1, &app) < 0);
        CHECK(gw_app_manifest_parse(duplicate, sizeof(duplicate) - 1, &app) < 0);
        CHECK(gw_app_manifest_parse(absolute, sizeof(absolute) - 1, &app) < 0);
        CHECK(gw_app_manifest_parse(traversal, sizeof(traversal) - 1, &app) < 0);
    }

    if (failures) {
        printf("App manager tests: %d failures\n", failures);
        return 1;
    }
    printf("App manager tests: PASS\n");
    return 0;
}

/* SPDX-License-Identifier: GPL-3.0-or-later
 * Signed application bundle management.
 *
 * Package layout (gzip tar):
 *   manifest              required, see gw-app-manifest.h
 *   payload/...           files referenced by manifest entrypoint
 * A detached raw RSA/SHA-256 signature is stored next to the bundle as
 *   <name>.gwapp.sig
 * and verified against /etc/gleworks/app-keys/release.pem before extraction.
 *
 * Layout on the device:
 *   /var/lib/gleworks/apps/<id>/<version>/      read-only payload
 *   /var/lib/gleworks/apps/<id>/active -> <version>
 *   /var/lib/gleworks/appdata/<id>/             persistent app data
 *
 * USB drop directory:
 *   /mnt/us/gleworks/apps/<name>.gwapp plus <name>.gwapp.sig
 *
 * STATUS: CORE (in use).  Trust is local: replacing both the bundle and its
 * signature requires the private release key, which is not on the device.
 */
#ifndef GW_APPS_H
#define GW_APPS_H

#include <stddef.h>

#ifndef GW_APPS_ROOT
#define GW_APPS_ROOT       "/var/lib/gleworks/apps"
#endif
#ifndef GW_APP_DATA_ROOT
#define GW_APP_DATA_ROOT   "/var/lib/gleworks/appdata"
#endif
#ifndef GW_APPS_USB_DIR
#define GW_APPS_USB_DIR    "/mnt/us/gleworks/apps"
#endif
#define GW_APPS_MAX        32
#define GW_APP_NAME_MAX    128

typedef struct {
    char id[64];
    char version[32];
    char description[128];
    char entrypoint[256];
} gw_app_info_t;

/* List installed bundles, sorted by id. Returns count or -1. */
int gw_apps_list(gw_app_info_t *out, int max);

/* Find one installed app. Returns 0 when found. */
int gw_apps_find(const char *id, gw_app_info_t *out);

/* Install/upgrade from a package file.  The previous version directory is
 * retained; only the active symlink moves after full verification. */
int gw_apps_install(const char *package, const char *signature,
                    char *err, size_t err_size);

/* Remove the payload and active link, retaining appdata. Built-in koreader
 * is refused. */
int gw_apps_remove(const char *id, char *err, size_t err_size);

/* Enumerate USB bundles. Returns count, or -1 when the drop directory is
 * absent/unreadable. names is an array of max entries of GW_APP_NAME_MAX. */
int gw_apps_usb_scan(char names[][GW_APP_NAME_MAX], int max);

/* Verify the detached signature next to a package.  Update selection must call
 * this before reading untrusted manifest metadata. */
int gw_apps_verify_package(const char *package, char *err, size_t err_size);

/* Read only id/version/description from a package manifest without extracting
 * or executing anything. Used to choose the newest bundle per app ID before
 * installation. Callers must verify the signature first; gw_apps_install
 * verifies again on the private staged copy. */
int gw_apps_package_info(const char *package, gw_app_info_t *out);

/* Numeric dotted-version comparison; returns <0, 0, >0. Non-numeric parts
 * fall back to a byte comparison. */
int gw_apps_version_compare(const char *left, const char *right);

#endif /* GW_APPS_H */

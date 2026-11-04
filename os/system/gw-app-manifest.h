/* SPDX-License-Identifier: GPL-3.0-or-later
 * Gleworks app manifest — format shared between the system service, app
 * manager and package installer. One ASCII manifest per app, named manifest,
 * is stored at the root of the package payload.
 *
 * The parser is used by the signed-package installer, the USB update picker
 * and generic app launch.  Runtime isolation and fd handoff remain open; see
 * docs/reference/app-session-contract.md.
 *
 * STATUS: CORE (install, metadata and entrypoint launch; isolation pending).
 */
#ifndef GW_APP_MANIFEST_H
#define GW_APP_MANIFEST_H
#include <stddef.h>

/* Maximum field lengths (bytes, including NUL). */
#define GW_APP_ID_MAX         64
#define GW_APP_VERSION_MAX    32
#define GW_APP_PATH_MAX      256
#define GW_APP_FORMAT_MAX     16
#define GW_APP_FORMAT_COUNT    8
#define GW_APP_PERM_MAX       32
#define GW_APP_PERM_COUNT      8
#define GW_APP_PROFILE_MAX    64
#define GW_APP_ARCH_MAX       16
#define GW_APP_OSVER_MAX      16
#define GW_APP_DESC_MAX      128
#define GW_APP_MANIFEST_MAX 4096

/*
 * Parsed representation of a manifest. All string fields are NUL-terminated
 * and bounded by the *_MAX constants above. Unparsed/absent fields are empty
 * strings or zero.
 */
typedef struct gw_app_manifest {
    char id[GW_APP_ID_MAX];
    char version[GW_APP_VERSION_MAX];
    char entrypoint[GW_APP_PATH_MAX];
    char formats[GW_APP_FORMAT_COUNT][GW_APP_FORMAT_MAX];
    int  format_count;
    char permissions[GW_APP_PERM_COUNT][GW_APP_PERM_MAX];
    int  perm_count;
    char min_os[GW_APP_OSVER_MAX];
    char arch[GW_APP_ARCH_MAX];
    char profile[GW_APP_PROFILE_MAX];
    char description[GW_APP_DESC_MAX];
} gw_app_manifest_t;

/* Reject malformed, duplicate, unknown or oversized fields; discard out on failure. */
int gw_app_manifest_parse(const char *data, size_t length, gw_app_manifest_t *out);
int gw_app_manifest_compatible(const gw_app_manifest_t *app, const char *os_version);

/*
 * Known permission tokens. The system service grants only the intersection of
 * what the manifest declares and what the OS policy allows for that app ID.
 * On kernel 3.0.35 only DAC UID/GID isolation is enforced; no seccomp/namespaces.
 */
#define GW_PERM_DISPLAY     "display"    /* framebuffer and EPDC */
#define GW_PERM_TOUCH       "touch"      /* evdev touch input */
#define GW_PERM_FRONTLIGHT  "frontlight" /* brightness via service IPC only */
#define GW_PERM_NETWORK     "network"    /* wpa_supplicant request via service */

/*
 * Install pipeline states — recorded in the staging directory during install.
 * Each state transition is written to staging/state before mutation begins;
 * a restart in mid-install finds the state and returns to the last safe point.
 */
typedef enum {
    GW_INSTALL_NONE       = 0,   /* no install in progress */
    GW_INSTALL_STAGING    = 1,   /* downloading/reading into staging */
    GW_INSTALL_VERIFIED   = 2,   /* signature/hash/manifest/space passed */
    GW_INSTALL_EXTRACTED  = 3,   /* extracted to version-specific root */
    GW_INSTALL_CHECKED    = 4,   /* entrypoint exists and is executable */
    GW_INSTALL_ACTIVE     = 5,   /* active symlink updated; old version kept */
} gw_install_state_t;

/* Staging directory relative to the app data root. */
#define GW_INSTALL_STAGING_DIR  "staging"
#define GW_INSTALL_STATE_FILE   "staging/state"
#define GW_INSTALL_ACTIVE_LINK  "active"    /* symlink → versioned subdir */

#endif /* GW_APP_MANIFEST_H */

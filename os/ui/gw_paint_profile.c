/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_paint_profile.h"

/* Text-heavy screens do not paint live drag frames: moving glyphs leave a
 * trail that no later cleanup can make look right.  Lists keep a couple of
 * previews, then clean only the touched area with a partial GC16. */
static const gw_paint_profile_t profiles[GW_PAINT_COUNT] = {
    [GW_PAINT_HOME] = {
        "home", GW_CONTENT_STATIC, 0, 0, GW_WAVE_DEFAULT, GW_WAVE_DEFAULT,
        0, 0, 0
    },
    [GW_PAINT_SETTINGS] = {
        "settings", GW_CONTENT_TEXT, 0, 0, GW_WAVE_DEFAULT, GW_WAVE_GC16,
        0, 1, 15
    },
    [GW_PAINT_NETWORK] = {
        "network", GW_CONTENT_LIST, 2, 160, GW_WAVE_GC16_FAST, GW_WAVE_REAGL,
        800, 1, 0
    },
    [GW_PAINT_APPS] = {
        "apps", GW_CONTENT_LIST, 2, 160, GW_WAVE_GC16_FAST, GW_WAVE_REAGL,
        800, 1, 0
    },
    [GW_PAINT_UPDATE] = {
        "update", GW_CONTENT_LIST, 2, 160, GW_WAVE_GC16_FAST, GW_WAVE_REAGL,
        800, 1, 0
    },
    [GW_PAINT_NOTES] = {
        "notes", GW_CONTENT_TEXT, 0, 0, GW_WAVE_DEFAULT, GW_WAVE_GC16,
        0, 1, 15
    },
    [GW_PAINT_CALENDAR] = {
        "calendar", GW_CONTENT_STATIC, 0, 0, GW_WAVE_DEFAULT, GW_WAVE_DEFAULT,
        0, 0, 0
    },
    [GW_PAINT_HUB] = {
        "hub", GW_CONTENT_STATIC, 0, 0, GW_WAVE_DEFAULT, GW_WAVE_DEFAULT,
        0, 0, 0
    },
    [GW_PAINT_LOCK] = {
        "lock", GW_CONTENT_STATIC, 0, 0, GW_WAVE_DEFAULT, GW_WAVE_DEFAULT,
        0, 0, 0
    }
};

const gw_paint_profile_t *gw_paint_profile_get(gw_paint_profile_id_t id) {
    if (id < 0 || id >= GW_PAINT_COUNT) id = GW_PAINT_HOME;
    return &profiles[id];
}

const char *gw_paint_profile_name(gw_paint_profile_id_t id) {
    return gw_paint_profile_get(id)->name;
}

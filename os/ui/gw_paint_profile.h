/* SPDX-License-Identifier: GPL-3.0-or-later
 * Universal paint engine profiles.  One EPDC engine owns the waveform and
 * refresh policy; each screen selects a profile by content type. */
#ifndef GW_PAINT_PROFILE_H
#define GW_PAINT_PROFILE_H

typedef enum {
    GW_PAINT_HOME = 0,
    GW_PAINT_SETTINGS,
    GW_PAINT_NETWORK,
    GW_PAINT_APPS,
    GW_PAINT_UPDATE,
    GW_PAINT_NOTES,
    GW_PAINT_CALENDAR,
    GW_PAINT_LOCK,
    GW_PAINT_COUNT
} gw_paint_profile_id_t;

typedef enum {
    GW_CONTENT_STATIC = 0,
    GW_CONTENT_TEXT,
    GW_CONTENT_LIST
} gw_paint_content_t;

typedef enum {
    GW_WAVE_DEFAULT = 0,
    GW_WAVE_GC16_FAST,
    GW_WAVE_GL16,
    GW_WAVE_REAGL,
    GW_WAVE_GC16,
    GW_WAVE_DU4
} gw_paint_wave_t;

typedef struct {
    const char *name;
    gw_paint_content_t content;
    int preview_max;              /* live preview frames while dragging */
    int preview_ms;               /* minimum spacing between previews */
    gw_paint_wave_t preview_wave;
    gw_paint_wave_t release_wave;
    int cleanup_ms;                /* area cleanup after scroll; 0 = none */
    int cleanup_area;              /* 1: touched union only; 0: full panel */
    gw_paint_wave_t cleanup_wave;
    int periodic_full_after;       /* partial updates before a full clean; 0=off */
} gw_paint_profile_t;

const gw_paint_profile_t *gw_paint_profile_get(gw_paint_profile_id_t id);
const char *gw_paint_profile_name(gw_paint_profile_id_t id);

#endif /* GW_PAINT_PROFILE_H */

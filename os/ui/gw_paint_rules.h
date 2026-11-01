/* SPDX-License-Identifier: GPL-3.0-or-later
 * Ordered paint rules.  Rules are evaluated by priority and can override the
 * screen profile for a context (text, list, large area, lock). */
#ifndef GW_PAINT_RULES_H
#define GW_PAINT_RULES_H

#include "gw_paint_profile.h"

typedef struct {
    int priority;
    const char *name;
    gw_paint_profile_id_t screen;   /* GW_PAINT_COUNT = any */
    gw_paint_content_t content;     /* 3 = any */
    int scrolling;                  /* -1 any, 0 idle, 1 drag/release */
    int min_area_pct;
    int max_area_pct;
    gw_paint_profile_id_t result;
    int force_no_preview;
    int page_snap;
} gw_paint_rule_t;

typedef struct {
    const gw_paint_profile_t *profile;
    const char *rule;
    int preview_max;
    int preview_ms;
    int page_snap;
} gw_paint_decision_t;

gw_paint_decision_t gw_paint_rules_decide(gw_paint_profile_id_t screen,
                                          gw_paint_content_t content,
                                          int scrolling, int area_pct);

#endif /* GW_PAINT_RULES_H */

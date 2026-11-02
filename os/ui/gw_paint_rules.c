/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stddef.h>
#include "gw_paint_rules.h"

#define ANY_CONTENT ((gw_paint_content_t)3)
#define ANY_SCREEN  ((gw_paint_profile_id_t)GW_PAINT_COUNT)

/* Higher priority wins.  Rules are deliberately small and composable.
 * Only criteria the shell actually evaluates at profile-selection time are
 * represented here; drag state and dirty area are runtime EPDC decisions. */
static const gw_paint_rule_t rules[] = {
    { 100, "lock-full", GW_PAINT_LOCK, ANY_CONTENT,
      GW_PAINT_LOCK, 1, 0 },
    { 90, "text-page-snap", GW_PAINT_SETTINGS, GW_CONTENT_TEXT,
      GW_PAINT_SETTINGS, 1, 1 },
    { 90, "notes-page-snap", GW_PAINT_NOTES, GW_CONTENT_TEXT,
      GW_PAINT_NOTES, 1, 1 },
    { 80, "hub-full", GW_PAINT_HUB, ANY_CONTENT,
      GW_PAINT_HUB, 1, 0 },
    { 70, "list-preview", ANY_SCREEN, GW_CONTENT_LIST,
      GW_PAINT_NETWORK, 0, 0 },
    { 60, "static", ANY_SCREEN, GW_CONTENT_STATIC,
      GW_PAINT_HOME, 1, 0 },
    { 10, "default", ANY_SCREEN, ANY_CONTENT,
      GW_PAINT_HOME, 0, 0 }
};

gw_paint_decision_t gw_paint_rules_decide(gw_paint_profile_id_t screen,
                                          gw_paint_content_t content) {
    size_t i;
    gw_paint_decision_t decision;
    const gw_paint_rule_t *best = NULL;
    for (i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        const gw_paint_rule_t *rule = &rules[i];
        if (rule->screen != ANY_SCREEN && rule->screen != screen) continue;
        if (rule->content != ANY_CONTENT && rule->content != content) continue;
        if (!best || rule->priority > best->priority) best = rule;
    }
    if (!best) best = &rules[sizeof(rules) / sizeof(rules[0]) - 1];
    decision.profile = gw_paint_profile_get(best->result);
    decision.rule = best->name;
    decision.preview_max = best->force_no_preview
                               ? 0 : decision.profile->preview_max;
    decision.preview_ms = decision.profile->preview_ms;
    decision.page_snap = best->page_snap;
    return decision;
}

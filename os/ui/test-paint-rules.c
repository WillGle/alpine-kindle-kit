/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_paint_rules.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void) {
    gw_paint_decision_t d = gw_paint_rules_decide(GW_PAINT_SETTINGS,
                                                   GW_CONTENT_TEXT);
    CHECK(!strcmp(d.rule, "text-page-snap"));
    CHECK(d.preview_max == 0 && d.page_snap == 1);
    d = gw_paint_rules_decide(GW_PAINT_NETWORK, GW_CONTENT_LIST);
    CHECK(!strcmp(d.rule, "list-preview"));
    CHECK(d.preview_max == 2);
    d = gw_paint_rules_decide(GW_PAINT_LOCK, GW_CONTENT_STATIC);
    CHECK(!strcmp(d.rule, "lock-full") && d.page_snap == 0);
    d = gw_paint_rules_decide(GW_PAINT_HOME, GW_CONTENT_STATIC);
    CHECK(!strcmp(d.rule, "static") && d.page_snap == 0);
    d = gw_paint_rules_decide(GW_PAINT_HUB, GW_CONTENT_STATIC);
    CHECK(!strcmp(d.rule, "hub-full") && d.preview_max == 0);
    if (failures) { printf("Paint rule tests: %d failures\n", failures); return 1; }
    printf("Paint rule tests: PASS\n");
    return 0;
}

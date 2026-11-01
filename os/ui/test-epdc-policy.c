/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host tests for the pure EPDC policy (no ioctl, no framebuffer). */
#include "gw_epdc_policy.h"
#include <stdio.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void) {
    CHECK(!gw_epdc_should_periodic_clean(GW_EPDC_PARTIAL_LIMIT - 1, 1000, 0));
    CHECK(!gw_epdc_should_periodic_clean(GW_EPDC_PARTIAL_LIMIT, 599, 0));
    CHECK(!gw_epdc_should_periodic_clean(GW_EPDC_PARTIAL_LIMIT, 1000, 1));
    CHECK(gw_epdc_should_periodic_clean(GW_EPDC_PARTIAL_LIMIT, 600, 0));

    CHECK(!gw_epdc_scroll_preview_allowed(0, 6, 100, 0, 0));
    CHECK(!gw_epdc_scroll_preview_allowed(0, 0, 100, 0, 160));
    CHECK(gw_epdc_scroll_preview_allowed(0, 6, 100, 0, 160));
    CHECK(!gw_epdc_scroll_preview_allowed(6, 6, 1000, 0, 160));
    CHECK(!gw_epdc_scroll_preview_allowed(2, 6, 1000, 900, 160));
    CHECK(gw_epdc_scroll_preview_allowed(2, 6, 1060, 900, 160));

    if (failures) {
        printf("EPDC policy tests: %d failures\n", failures);
        return 1;
    }
    printf("EPDC policy tests: PASS\n");
    return 0;
}

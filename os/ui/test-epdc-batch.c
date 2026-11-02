/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host test: separated dirty rectangles produce separate EPDC submissions;
 * nearby rectangles are compacted into one.  No ioctl or framebuffer access. */
#define GW_EPDC_STATE_IMPLEMENTATION
#define _GNU_SOURCE
#include "lv_port_epdc.h"
#include <stdio.h>

static int failures;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void) {
    g_epdc_smoke = 1;
    g_update_count = 0;

    gw_epdc_add_dirty(10, 10, 40, 40);
    gw_epdc_add_dirty(600, 1200, 700, 1300);
    CHECK(g_dirty_count == 2);
    CHECK(gw_epdc_present() == 0);
    CHECK(g_update_count == 2);

    g_update_count = 0;
    gw_epdc_add_dirty(10, 10, 40, 40);
    gw_epdc_add_dirty(41, 10, 80, 40);
    CHECK(g_dirty_count == 1);
    CHECK(gw_epdc_present() == 0);
    CHECK(g_update_count == 1);

    /* A text page snap is four progressive bands, counted as one budget
     * unit. */
    g_update_count = 0;
    g_partial_updates = 0;
    gw_epdc_add_dirty(0, 0, 1000, 1400);
    g_split_page_pending = 1;
    CHECK(gw_epdc_present() == 0);
    CHECK(g_update_count == 4);
    CHECK(g_partial_updates == 1);

    if (failures) {
        printf("EPDC batch tests: %d failures\n", failures);
        return 1;
    }
    printf("EPDC batch tests: PASS\n");
    return 0;
}

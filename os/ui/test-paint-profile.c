/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gw_paint_profile.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void) {
    const gw_paint_profile_t *settings = gw_paint_profile_get(GW_PAINT_SETTINGS);
    const gw_paint_profile_t *network = gw_paint_profile_get(GW_PAINT_NETWORK);
    const gw_paint_profile_t *home = gw_paint_profile_get(GW_PAINT_HOME);
    CHECK(settings && settings->content == GW_CONTENT_TEXT);
    CHECK(settings && settings->preview_max == 0);       /* no text smearing */
    CHECK(settings && settings->cleanup_area == 1);
    CHECK(settings && settings->release_wave == GW_WAVE_DU4);
    CHECK(settings && settings->cleanup_wave == GW_WAVE_GC16);
    CHECK(settings && settings->cleanup_ms == 0);        /* one GC16 page pass */
    CHECK(settings && settings->periodic_full_after == 15);
    CHECK(network && network->content == GW_CONTENT_LIST);
    CHECK(network && network->preview_max == 2);
    CHECK(network && network->cleanup_area == 1);
    CHECK(home && home->cleanup_ms == 0);
    CHECK(!strcmp(gw_paint_profile_name(GW_PAINT_APPS), "apps"));
    CHECK(gw_paint_profile_get((gw_paint_profile_id_t)999) == home);
    if (failures) {
        printf("Paint profile tests: %d failures\n", failures);
        return 1;
    }
    printf("Paint profile tests: PASS\n");
    return 0;
}

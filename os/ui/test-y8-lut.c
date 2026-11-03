/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host test: raw Y8 image data must survive the RGB332 LVGL pipeline.
 * The PW3 panel is Y8, but LVGL draws RGB332; the flush maps RGB332 back to
 * gray.  A raw frame therefore has to be pre-encoded with the inverse table,
 * otherwise a mid-gray byte renders as dark red.
 */
#define GW_EPDC_STATE_IMPLEMENTATION
#define _GNU_SOURCE
#include "lv_port_epdc.h"
#include <stdio.h>

static int failures;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void) {
    int v;
    gw_init_gray_lut();

    /* Every 8-bit gray must round-trip to within the quantization step. */
    for (v = 0; v < 256; v++) {
        uint8_t encoded = gw_y8_to_rgb332((uint8_t)v);
        int decoded = gw_rgb332_to_gray_lut(encoded);
        int delta = decoded - v;
        if (delta < 0) delta = -delta;
        if (delta > 5) {
            printf("FAIL: gray %d -> %d -> %d (delta %d)\n",
                   v, encoded, decoded, delta);
            failures++;
        }
    }

    /* A mid-gray must stay mid-tone: the old code produced ~42. */
    {
        int decoded = gw_rgb332_to_gray_lut(gw_y8_to_rgb332(128));
        CHECK(decoded >= 123 && decoded <= 133);
    }

    /* Black and white must remain exact. */
    CHECK(gw_rgb332_to_gray_lut(gw_y8_to_rgb332(0)) == 0);
    CHECK(gw_rgb332_to_gray_lut(gw_y8_to_rgb332(255)) == 255);

    {
        uint8_t pixels[4] = {0, 64, 128, 255};
        int i;
        gw_y8_to_rgb332_buffer(pixels, 4);
        for (i = 0; i < 4; i++) {
            int decoded = gw_rgb332_to_gray_lut(pixels[i]);
            int want = i == 0 ? 0 : i == 1 ? 64 : i == 2 ? 128 : 255;
            int delta = decoded - want;
            if (delta < 0) delta = -delta;
            CHECK(delta <= 5);
        }
    }

    printf("y8/rgb332 round-trip: %d failures\n", failures);
    return failures ? 1 : 0;
}

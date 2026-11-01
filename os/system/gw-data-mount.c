/* SPDX-License-Identifier: GPL-3.0-or-later
 * EXPERIMENTAL: data-image mount trial.
 *
 * STATUS: SPIKE (accepted=0). Not part of base OS; kept for experimentation.
 * This file is intentionally not compiled into the default 'service' target.
 *
 * Purpose: probe mounting a separate data image for app/document storage.
 * Do not enable in production builds until accepted on real hardware.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "gw-data-mount: experimental spike; not implemented\n");
    return 1;
}

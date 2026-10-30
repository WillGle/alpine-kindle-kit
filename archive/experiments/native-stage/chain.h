/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>

#define GW_OWNED_SOURCE 1
#define GW_OWNED_LIST 2
#define GW_OWNED_CONTROL 3
#define GW_OWNED_OTHER 4

/* Must be constructed internally from actual successful allocations, never
 * from a user request or by walking an untrusted chain to infer ownership. */
struct gw_owned_page {
    u32 physical;
    const u32 *words;
    unsigned int role;
};

int gw_validate_chain(u32 head, u32 destination, u32 bytes, u32 control,
                       const struct gw_owned_page *owned, unsigned int count);

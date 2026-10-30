/* SPDX-License-Identifier: GPL-2.0-only
 * Compile-only bounded chain validation. Reads registered owned pages only.
 */
#include <linux/errno.h>
#include <linux/string.h>
#include "chain.h"

#define RAM_START 0x80000000U
#define RAM_END 0xa0000000U
#define PAGE_BYTES 4096U
#define PAGE_BITS 4095U

static int find_page(const struct gw_owned_page *owned, unsigned int count,
                      u32 address, unsigned int role)
{
    unsigned int i;
    for (i = 0; i < count; i++)
        if (owned[i].physical == address && owned[i].role == role)
            return i;
    return -1;
}

int gw_validate_chain(u32 head, u32 destination, u32 bytes, u32 control,
                       const struct gw_owned_page *owned, unsigned int count)
{
    unsigned char visited[128];
    unsigned int i, j, offset = 0, steps;
    u32 copied = 0, end, address, entry;
    int current, selected = 0;

    if (!owned || !count || count > 1024 || destination != 0x80008000U ||
        !bytes || bytes > 2*1024*1024 || (bytes & PAGE_BITS))
        return -EINVAL;
    end = destination + bytes;
    /* Reject duplicate physical registrations or aliases with different roles.
     * Registration itself remains the responsibility of the bounded allocator. */
    for (i = 0; i < count; i++) {
        address = owned[i].physical;
        if ((address & PAGE_BITS) || address < RAM_START ||
            address > RAM_END-PAGE_BYTES || !owned[i].words ||
            owned[i].role < GW_OWNED_SOURCE || owned[i].role > GW_OWNED_OTHER)
            return -EINVAL;
        for (j = 0; j < i; j++)
            if (address == owned[j].physical)
                return -EINVAL;
    }
    if ((control < end && control+PAGE_BYTES > destination) ||
        find_page(owned, count, control, GW_OWNED_CONTROL) < 0 ||
        (head & PAGE_BITS) != 2)
        return -EINVAL;
    current = find_page(owned, count, head & ~PAGE_BITS, GW_OWNED_LIST);
    if (current < 0)
        return -EINVAL;
    memset(visited, 0, sizeof(visited));
    visited[current >> 3] |= 1U << (current & 7);
    for (steps = 0; steps < 2048; steps++) {
        if (offset >= PAGE_BYTES/sizeof(u32))
            return -EINVAL;
        address = owned[current].physical;
        if (address == control || (address < end && address+PAGE_BYTES > destination))
            return -EINVAL;
        entry = owned[current].words[offset++];
        address = entry & ~PAGE_BITS;
        switch (entry & PAGE_BITS) {
        case 1: /* One contiguous segment; destination must precede sources. */
            if (selected || copied || address != destination)
                return -EINVAL;
            selected = 1;
            break;
        case 2:
            current = find_page(owned, count, address, GW_OWNED_LIST);
            if (current < 0 || (visited[current >> 3] & (1U << (current & 7))))
                return -EINVAL;
            visited[current >> 3] |= 1U << (current & 7);
            offset = 0;
            break;
        case 4:
            return entry == 4 && selected && copied == bytes ? 0 : -EINVAL;
        case 8:
            if (!selected || copied >= bytes || address == control)
                return -EINVAL;
            i = find_page(owned, count, address, GW_OWNED_SOURCE);
            if (i >= count || (visited[i >> 3] & (1U << (i & 7))))
                return -EINVAL;
            /* A self-copy is safe; a source in another destination page may
             * be destroyed before its turn. Stock allocator must resolve it. */
            if (address >= destination && address < end && address != destination+copied)
                return -EINVAL;
            visited[i >> 3] |= 1U << (i & 7);
            copied += PAGE_BYTES;
            break;
        default:
            return -EINVAL;
        }
    }
    return -EINVAL;
}

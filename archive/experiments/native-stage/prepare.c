/* SPDX-License-Identifier: GPL-2.0-only
 * Returning control-page preparation only. No transition function linked.
 */
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/highmem.h>
#include <asm/outercache.h>
#include <asm/irqflags.h>
#include <asm/memory.h>
#include "transition.h"

extern unsigned char gw_relocate_start[], gw_relocate_end[];
extern unsigned char gw_relocate_list[], gw_relocate_entry[], gw_relocate_dtb[];

#define GW_RAM_BASE 0x80000000UL
#define GW_RAM_END  0xa0000000UL
#define GW_ENTRY    0x80008000UL

int gw_prepare_handoff(struct kimage *image, unsigned long dtb,
                       unsigned long dtb_size)
{
    unsigned long control, list, start, end, length;
    unsigned char *buffer;

    if (!image || !image->control_code_page || image->nr_segments != 1)
        return -EINVAL;
    start = image->segment[0].mem;
    if (image->start != GW_ENTRY || start != GW_ENTRY ||
        !image->segment[0].bufsz || image->segment[0].bufsz > 2*1024*1024 ||
        image->segment[0].memsz != PAGE_ALIGN(image->segment[0].bufsz))
        return -EINVAL;
    end = start + image->segment[0].memsz;
    if ((dtb & 7) || dtb < start || dtb >= end || dtb_size < 40 ||
        dtb_size > end-dtb || dtb+dtb_size != start+image->segment[0].bufsz)
        return -EINVAL;
    control = page_to_pfn(image->control_code_page) << PAGE_SHIFT;
    list = image->head & PAGE_MASK;
    if (control < GW_RAM_BASE || control > GW_RAM_END-PAGE_SIZE ||
        list < GW_RAM_BASE || list > GW_RAM_END-PAGE_SIZE || control == list ||
        (control < end && control+PAGE_SIZE > start) ||
        (list < end && list+PAGE_SIZE > start))
        return -EINVAL;
    buffer = page_address(image->control_code_page);
    length = gw_relocate_end-gw_relocate_start;
    if (!buffer || length > PAGE_SIZE)
        return -EINVAL;
    memset(buffer, 0, PAGE_SIZE);
    memcpy(buffer, gw_relocate_start, length);
    *(unsigned long *)(buffer+(gw_relocate_list-gw_relocate_start)) = list;
    *(unsigned long *)(buffer+(gw_relocate_entry-gw_relocate_start)) = GW_ENTRY;
    *(unsigned long *)(buffer+(gw_relocate_dtb-gw_relocate_start)) = dtb;
    /* Cache coherency is established at the final global flush, not here. */
    return 0;
}

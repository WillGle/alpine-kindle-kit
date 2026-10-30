/*
 * kexec.c - kexec system call
 * Copyright (C) 2002-2004 Eric Biederman  <ebiederm@xmission.com>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <linux/capability.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kexec.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/highmem.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/ioport.h>
#include <linux/hardirq.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <generated/utsrelease.h>
#include <linux/utsname.h>
#include <linux/numa.h>
#include <linux/suspend.h>
#include <linux/device.h>
#include <linux/freezer.h>
#include <linux/pm.h>
#include <linux/cpu.h>
#include <linux/console.h>
#include <linux/vmalloc.h>
#include <linux/swap.h>
#include <linux/kmsg_dump.h>
#include <linux/syscore_ops.h>

#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/sections.h>

static unsigned int allocated_pages, peak_pages;
static unsigned int allocation_budget = 1024;

/* This registry is written only at allocation/free/role transitions under
 * gw_lock. Chain parsing never creates an ownership entry. */
static struct gw_owned_page owned[1024];
static unsigned int owned_count;
static void gw_role(struct page *page, unsigned int role)
{
    unsigned int i;
    u32 physical = page_to_pfn(page) << PAGE_SHIFT;
    for (i = 0; i < owned_count; i++)
        if (owned[i].physical == physical) {
            owned[i].role = role;
            return;
        }
}
static void gw_forget(struct page *page)
{
    unsigned int i;
    u32 physical = page_to_pfn(page) << PAGE_SHIFT;
    for (i = 0; i < owned_count; i++)
        if (owned[i].physical == physical) {
            owned[i] = owned[--owned_count];
            return;
        }
}

#define KIMAGE_NO_DEST (-1UL)

static int kimage_is_destination_range(struct kimage *image,
				       unsigned long start, unsigned long end);
static struct page *kimage_alloc_page(struct kimage *image,
				       gfp_t gfp_mask,
				       unsigned long dest);

static int do_kimage_alloc(struct kimage **rimage, unsigned long entry,
	                    unsigned long nr_segments,
                            struct kexec_segment *segments)
{
	size_t segment_bytes;
	struct kimage *image;
	unsigned long i;
	int result;

	/* Allocate a controlling structure */
	result = -ENOMEM;
	image = kzalloc(sizeof(*image), GFP_NOWAIT);
	if (!image)
		goto out;

	image->head = 0;
	image->entry = &image->head;
	image->last_entry = &image->head;
	image->control_page = ~0; /* By default this does not apply */
	image->start = entry;
	image->type = KEXEC_TYPE_DEFAULT;

	/* Initialize the list of control pages */
	INIT_LIST_HEAD(&image->control_pages);

	/* Initialize the list of destination pages */
	INIT_LIST_HEAD(&image->dest_pages);

	/* Initialize the list of unusable pages */
	INIT_LIST_HEAD(&image->unuseable_pages);

	/* Read in the segments */
	image->nr_segments = nr_segments;
	segment_bytes = nr_segments * sizeof(*segments);
	memcpy(image->segment, segments, segment_bytes);
	result = 0;
	if (result) {
		result = -EFAULT;
		goto out;
	}

	/*
	 * Verify we have good destination addresses.  The caller is
	 * responsible for making certain we don't attempt to load
	 * the new image into invalid or reserved areas of RAM.  This
	 * just verifies it is an address we can use.
	 *
	 * Since the kernel does everything in page size chunks ensure
	 * the destination addresses are page aligned.  Too many
	 * special cases crop of when we don't do this.  The most
	 * insidious is getting overlapping destination addresses
	 * simply because addresses are changed to page size
	 * granularity.
	 */
	result = -EADDRNOTAVAIL;
	for (i = 0; i < nr_segments; i++) {
		unsigned long mstart, mend;

		mstart = image->segment[i].mem;
		mend   = mstart + image->segment[i].memsz;
		if ((mstart & ~PAGE_MASK) || (mend & ~PAGE_MASK))
			goto out;
		if (mend >= KEXEC_DESTINATION_MEMORY_LIMIT)
			goto out;
	}

	/* Verify our destination addresses do not overlap.
	 * If we alloed overlapping destination addresses
	 * through very weird things can happen with no
	 * easy explanation as one segment stops on another.
	 */
	result = -EINVAL;
	for (i = 0; i < nr_segments; i++) {
		unsigned long mstart, mend;
		unsigned long j;

		mstart = image->segment[i].mem;
		mend   = mstart + image->segment[i].memsz;
		for (j = 0; j < i; j++) {
			unsigned long pstart, pend;
			pstart = image->segment[j].mem;
			pend   = pstart + image->segment[j].memsz;
			/* Do the segments overlap ? */
			if ((mend > pstart) && (mstart < pend))
				goto out;
		}
	}

	/* Ensure our buffer sizes are strictly less than
	 * our memory sizes.  This should always be the case,
	 * and it is easier to check up front than to be surprised
	 * later on.
	 */
	result = -EINVAL;
	for (i = 0; i < nr_segments; i++) {
		if (image->segment[i].bufsz > image->segment[i].memsz)
			goto out;
	}

	result = 0;
out:
	if (result == 0)
		*rimage = image;
	else
		kfree(image);

	return result;

}

static int kimage_normal_alloc(struct kimage **rimage, unsigned long entry,
				unsigned long nr_segments,
				struct kexec_segment *segments)
{
	int result;
	struct kimage *image;

	/* Allocate and initialize a controlling structure */
	image = NULL;
	result = do_kimage_alloc(&image, entry, nr_segments, segments);
	if (result)
		goto out;

	*rimage = image;

	/*
	 * Find a location for the control code buffer, and add it
	 * the vector of segments so that it's pages will also be
	 * counted as destination pages.
	 */
	result = -ENOMEM;
	image->control_code_page = kimage_alloc_control_pages(image,
					   get_order(KEXEC_CONTROL_PAGE_SIZE));
	if (!image->control_code_page) {
		printk(KERN_ERR "Could not allocate control_code_buffer\n");
		goto out;
	}

	image->swap_page = kimage_alloc_control_pages(image, 0);
	if (!image->swap_page) {
		printk(KERN_ERR "Could not allocate swap buffer\n");
		goto out;
	}

	result = 0;
 out:
	if (result == 0)
		*rimage = image;
	/* Caller owns *rimage and performs complete failure cleanup. */

	return result;
}

static int kimage_is_destination_range(struct kimage *image,
					unsigned long start,
					unsigned long end)
{
	unsigned long i;

	for (i = 0; i < image->nr_segments; i++) {
		unsigned long mstart, mend;

		mstart = image->segment[i].mem;
		mend = mstart + image->segment[i].memsz;
		if ((end > mstart) && (start < mend))
			return 1;
	}

	return 0;
}

static struct page *kimage_alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *pages;

	if (order || allocated_pages >= allocation_budget || owned_count >= 1024)
		return NULL;
	pages = alloc_pages(GFP_NOWAIT | __GFP_NOWARN, 0);
	if (pages) {
		unsigned int count, i;
		owned[owned_count].physical = page_to_pfn(pages) << PAGE_SHIFT;
        owned[owned_count].words = page_address(pages);
        owned[owned_count++].role = GW_OWNED_OTHER;
        allocated_pages++;
		if (allocated_pages > peak_pages) peak_pages = allocated_pages;
		pages->mapping = NULL;
		set_page_private(pages, order);
		count = 1 << order;
		for (i = 0; i < count; i++)
			SetPageReserved(pages + i);
	}

	return pages;
}

static void kimage_free_pages(struct page *page)
{
	unsigned int order, count, i;

	order = page_private(page);
	count = 1 << order;
	for (i = 0; i < count; i++)
		ClearPageReserved(page + i);
	gw_forget(page);
	__free_pages(page, order);
	allocated_pages--;
}

static void kimage_free_page_list(struct list_head *list)
{
	struct list_head *pos, *next;

	list_for_each_safe(pos, next, list) {
		struct page *page;

		page = list_entry(pos, struct page, lru);
		list_del(&page->lru);
		kimage_free_pages(page);
	}
}

static struct page *kimage_alloc_normal_control_pages(struct kimage *image,
							unsigned int order)
{
	/* Control pages are special, they are the intermediaries
	 * that are needed while we copy the rest of the pages
	 * to their final resting place.  As such they must
	 * not conflict with either the destination addresses
	 * or memory the kernel is already using.
	 *
	 * The only case where we really need more than one of
	 * these are for architectures where we cannot disable
	 * the MMU and must instead generate an identity mapped
	 * page table for all of the memory.
	 *
	 * At worst this runs in O(N) of the image size.
	 */
	struct list_head extra_pages;
	struct page *pages;
	unsigned int count;

	count = 1 << order;
	INIT_LIST_HEAD(&extra_pages);

	/* Loop while I can allocate a page and the page allocated
	 * is a destination page.
	 */
	do {
		unsigned long pfn, epfn, addr, eaddr;

		pages = kimage_alloc_pages(GFP_KERNEL, order);
		if (!pages)
			break;
		pfn   = page_to_pfn(pages);
		epfn  = pfn + count;
		addr  = pfn << PAGE_SHIFT;
		eaddr = epfn << PAGE_SHIFT;
		if ((epfn >= (KEXEC_CONTROL_MEMORY_LIMIT >> PAGE_SHIFT)) ||
			      kimage_is_destination_range(image, addr, eaddr)) {
			list_add(&pages->lru, &extra_pages);
			pages = NULL;
		}
	} while (!pages);

	if (pages) {
		gw_role(pages, GW_OWNED_CONTROL);
		/* Remember the allocated page... */
		list_add(&pages->lru, &image->control_pages);

		/* Because the page is already in it's destination
		 * location we will never allocate another page at
		 * that address.  Therefore kimage_alloc_pages
		 * will not return it (again) and we don't need
		 * to give it an entry in image->segment[].
		 */
	}
	/* Deal with the destination pages I have inadvertently allocated.
	 *
	 * Ideally I would convert multi-page allocations into single
	 * page allocations, and add everything to image->dest_pages.
	 *
	 * For now it is simpler to just free the pages.
	 */
	kimage_free_page_list(&extra_pages);

	return pages;
}

struct page *kimage_alloc_control_pages(struct kimage *image, unsigned int order)
{
 return kimage_alloc_normal_control_pages(image, order);
}

static int kimage_add_entry(struct kimage *image, kimage_entry_t entry)
{
	if (*image->entry != 0)
		image->entry++;

	if (image->entry == image->last_entry) {
		kimage_entry_t *ind_page;
		struct page *page;

		page = kimage_alloc_page(image, GFP_KERNEL, KIMAGE_NO_DEST);
		if (!page)
			return -ENOMEM;

		gw_role(page, GW_OWNED_LIST);
		ind_page = page_address(page);
		*image->entry = virt_to_phys(ind_page) | IND_INDIRECTION;
		image->entry = ind_page;
		image->last_entry = ind_page +
				      ((PAGE_SIZE/sizeof(kimage_entry_t)) - 1);
	}
	*image->entry = entry;
	image->entry++;
	*image->entry = 0;

	return 0;
}

static int kimage_set_destination(struct kimage *image,
				   unsigned long destination)
{
	int result;

	destination &= PAGE_MASK;
	result = kimage_add_entry(image, destination | IND_DESTINATION);
	if (result == 0)
		image->destination = destination;

	return result;
}


static int kimage_add_page(struct kimage *image, unsigned long page)
{
	int result;

	page &= PAGE_MASK;
	result = kimage_add_entry(image, page | IND_SOURCE);
	if (result == 0)
		image->destination += PAGE_SIZE;

	return result;
}


static void kimage_free_extra_pages(struct kimage *image)
{
	/* Walk through and free any extra destination pages I may have */
	kimage_free_page_list(&image->dest_pages);

	/* Walk through and free any unusable pages I have cached */
	kimage_free_page_list(&image->unuseable_pages);

}
static void kimage_terminate(struct kimage *image)
{
	if (*image->entry != 0)
		image->entry++;

	*image->entry = IND_DONE;
}

#define for_each_kimage_entry(image, ptr, entry) \
	for (ptr = &image->head; (entry = *ptr) && !(entry & IND_DONE); \
		ptr = (entry & IND_INDIRECTION)? \
			phys_to_virt((entry & PAGE_MASK)): ptr +1)

static void kimage_free_entry(kimage_entry_t entry)
{
	struct page *page;

	page = pfn_to_page(entry >> PAGE_SHIFT);
	kimage_free_pages(page);
}

static void kimage_free(struct kimage *image)
{
	kimage_entry_t *ptr, entry;
	kimage_entry_t ind = 0;

	if (!image)
		return;

	kimage_free_extra_pages(image);
	for_each_kimage_entry(image, ptr, entry) {
		if (entry & IND_INDIRECTION) {
			/* Free the previous indirection page */
			if (ind & IND_INDIRECTION)
				kimage_free_entry(ind);
			/* Save this indirection page until we are
			 * done with it.
			 */
			ind = entry;
		}
		else if (entry & IND_SOURCE)
			kimage_free_entry(entry);
	}
	/* Free the final indirection page */
	if (ind & IND_INDIRECTION)
		kimage_free_entry(ind);

	/* Handle any machine specific cleanup */
	machine_kexec_cleanup(image);

	/* Free the kexec control pages... */
	kimage_free_page_list(&image->control_pages);
	kfree(image);
}

static kimage_entry_t *kimage_dst_used(struct kimage *image,
					unsigned long page)
{
	kimage_entry_t *ptr, entry;
	unsigned long destination = 0;

	for_each_kimage_entry(image, ptr, entry) {
		if (entry & IND_DESTINATION)
			destination = entry & PAGE_MASK;
		else if (entry & IND_SOURCE) {
			if (page == destination)
				return ptr;
			destination += PAGE_SIZE;
		}
	}

	return NULL;
}

static struct page *kimage_alloc_page(struct kimage *image,
					gfp_t gfp_mask,
					unsigned long destination)
{
	/*
	 * Here we implement safeguards to ensure that a source page
	 * is not copied to its destination page before the data on
	 * the destination page is no longer useful.
	 *
	 * To do this we maintain the invariant that a source page is
	 * either its own destination page, or it is not a
	 * destination page at all.
	 *
	 * That is slightly stronger than required, but the proof
	 * that no problems will not occur is trivial, and the
	 * implementation is simply to verify.
	 *
	 * When allocating all pages normally this algorithm will run
	 * in O(N) time, but in the worst case it will run in O(N^2)
	 * time.   If the runtime is a problem the data structures can
	 * be fixed.
	 */
	struct page *page;
	unsigned long addr;

	/*
	 * Walk through the list of destination pages, and see if I
	 * have a match.
	 */
	list_for_each_entry(page, &image->dest_pages, lru) {
		addr = page_to_pfn(page) << PAGE_SHIFT;
		if (addr == destination) {
			list_del(&page->lru);
			return page;
		}
	}
	page = NULL;
	while (1) {
		kimage_entry_t *old;

		/* Allocate a page, if we run out of memory give up */
		page = kimage_alloc_pages(gfp_mask, 0);
		if (!page)
			return NULL;
		/* If the page cannot be used file it away */
		if (page_to_pfn(page) >
				(KEXEC_SOURCE_MEMORY_LIMIT >> PAGE_SHIFT)) {
			list_add(&page->lru, &image->unuseable_pages);
			continue;
		}
		addr = page_to_pfn(page) << PAGE_SHIFT;

		/* If it is the destination page we want use it */
		if (addr == destination)
			break;

		/* If the page is not a destination page use it */
		if (!kimage_is_destination_range(image, addr,
						  addr + PAGE_SIZE))
			break;

		/*
		 * I know that the page is someones destination page.
		 * See if there is already a source page for this
		 * destination page.  And if so swap the source pages.
		 */
		old = kimage_dst_used(image, addr);
		if (old) {
			/* If so move it */
			unsigned long old_addr;
			struct page *old_page;

			old_addr = *old & PAGE_MASK;
			old_page = pfn_to_page(old_addr >> PAGE_SHIFT);
			copy_highpage(page, old_page);
			gw_role(page, GW_OWNED_SOURCE);
			*old = addr | (*old & ~PAGE_MASK);

			/* The old page I have found cannot be a
			 * destination page, so return it if it's
			 * gfp_flags honor the ones passed in.
			 */
			if (!(gfp_mask & __GFP_HIGHMEM) &&
			    PageHighMem(old_page)) {
				kimage_free_pages(old_page);
				continue;
			}
			addr = old_addr;
			page = old_page;
			break;
		}
		else {
			/* Place the page on the destination list I
			 * will use it later.
			 */
			list_add(&page->lru, &image->dest_pages);
		}
	}

	return page;
}

static int kimage_load_normal_segment(struct kimage *image,
					 struct kexec_segment *segment)
{
	unsigned long maddr;
	unsigned long ubytes, mbytes;
	int result;
	unsigned char __user *buf;

	result = 0;
	buf = segment->buf;
	ubytes = segment->bufsz;
	mbytes = segment->memsz;
	maddr = segment->mem;

	result = kimage_set_destination(image, maddr);
	if (result < 0)
		goto out;

	while (mbytes) {
		struct page *page;
		char *ptr;
		size_t uchunk, mchunk;

		page = kimage_alloc_page(image, GFP_HIGHUSER, maddr);
		if (!page) {
			result  = -ENOMEM;
			goto out;
		}
		gw_role(page, GW_OWNED_SOURCE);
		result = kimage_add_page(image, page_to_pfn(page) << PAGE_SHIFT);
		if (result < 0) {
			kimage_free_pages(page);
			goto out;
		}

		ptr = kmap(page);
		/* Start with a clear page */
		clear_page(ptr);
		ptr += maddr & ~PAGE_MASK;
		mchunk = PAGE_SIZE - (maddr & ~PAGE_MASK);
		if (mchunk > mbytes)
			mchunk = mbytes;

		uchunk = mchunk;
		if (uchunk > ubytes)
			uchunk = ubytes;

		result = copy_from_user(ptr, buf, uchunk);
		kunmap(page);
		if (result) {
			result = -EFAULT;
			goto out;
		}
		ubytes -= uchunk;
		maddr  += mchunk;
		buf    += mchunk;
		mbytes -= mchunk;
	}
out:
	return result;
}


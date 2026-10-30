/* GPL-2.0-only: normal kexec staging only. Execution code is absent. */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/version.h>
#include "protocol.h"
#include "chain.h"
#include "transition.h"
#include "stock-normal-core.inc"
#include "sha256.inc"

static DEFINE_MUTEX(gw_lock);
static struct kimage *staged;
static int opened;

/* Stock ARM prepare/cleanup are empty; no machine_kexec implementation linked. */
void machine_kexec_cleanup(struct kimage *image) { }

static void drop_image(void)
{
	kimage_free(staged);
	staged = NULL;
}

static int validate_staged(void)
{
    if (!staged) return -ENOENT;
    if (owned_count != allocated_pages) return -EIO;
    return gw_validate_chain(staged->head, GW_DEST, staged->segment[0].memsz,
        page_to_pfn(staged->control_code_page) << PAGE_SHIFT, owned, owned_count);
}

static int prepare_fixed_image(void)
{
    static const u8 expected[32] __attribute__((aligned(4))) = {
        0x16, 0xc9, 0xbe, 0x18, 0x7c, 0x4d, 0xde, 0xfe, 0x61, 0xb2, 0xb5, 0xe1, 0x84, 0xb0, 0x30, 0x85, 0xdb, 0x55, 0x7f, 0x19, 0xac, 0xc9, 0xa9, 0x19, 0x1c, 0x03, 0x6c, 0xfa, 0x94, 0x7a, 0xb9, 0x2f
    };
    struct sha256_state hash;
    u8 digest[32] __attribute__((aligned(4)));
    kimage_entry_t *ptr, entry;
    unsigned int remaining, length;
    int result = validate_staged();
    if (result) return result;
    if (staged->segment[0].bufsz != 2722513) return -EINVAL;
    sha256_init(&hash);
    remaining = staged->segment[0].bufsz;
    /* Ownership/termination/roles were checked before any physical dereference.
     * Mutex remains held through hashing and control-page preparation. */
    for_each_kimage_entry(staged, ptr, entry) {
        if (!(entry & IND_SOURCE)) continue;
        length = remaining > PAGE_SIZE ? PAGE_SIZE : remaining;
        sha256_update(&hash, phys_to_virt(entry & PAGE_MASK), length);
        remaining -= length;
    }
    sha256_final(&hash, digest);
    if (remaining || memcmp(digest, expected, sizeof(digest))) return -EINVAL;
    result = gw_prepare_handoff(staged, GW_DEST + 2702408, 20105);
    if (result) return result;
    return validate_staged();
}

static int verify_image(struct gw_status *status)
{
	kimage_entry_t *ptr, entry;
	unsigned long destination = 0, count = 0, bytes = 0;
	u32 checksum = 2166136261U;

	{ int result = validate_staged(); if (result) return result; }
	for_each_kimage_entry(staged, ptr, entry) {
		unsigned int i;
		const volatile unsigned char *data;

		if (++count > 2048)
			return -EIO;
		if (entry & IND_INDIRECTION)
			continue;
		if (entry & IND_DESTINATION) {
			destination = entry & PAGE_MASK;
			continue;
		}
		if (!(entry & IND_SOURCE) || destination != GW_DEST + bytes ||
		    bytes >= staged->segment[0].memsz)
			return -EIO;
		data = phys_to_virt(entry & PAGE_MASK);
		for (i = 0; i < PAGE_SIZE; i++)
			checksum = (checksum ^ data[i]) * 16777619U;
		bytes += PAGE_SIZE;
		destination += PAGE_SIZE;
	}
	if (bytes != staged->segment[0].memsz)
		return -EIO;
	status->bytes = staged->segment[0].bufsz;
	status->checksum = checksum;
	status->owned_pages = allocated_pages;
	status->peak_pages = peak_pages;
	return 0;
}

static long gw_ioctl(struct file *file, unsigned int command, unsigned long arg)
{
	struct gw_request request;
	struct gw_segment user_segment;
	struct kexec_segment segment;
	struct kimage *image = NULL;
	struct gw_status status;
	long result = 0;

	if (!capable(CAP_SYS_BOOT))
		return -EPERM;
	if (!mutex_trylock(&gw_lock))
		return -EBUSY;
	switch (command) {
	case GW_LOAD:
		if (staged) { result = -EBUSY; break; }
		if (copy_from_user(&request, (void __user *)arg, sizeof(request))) {
			result = -EFAULT; break;
		}
		if (request.flags || request.count != 1 || request.entry != GW_DEST) {
			result = -EINVAL; break;
		}
		if (copy_from_user(&user_segment, (void __user *)request.segments,
				   sizeof(user_segment))) {
			result = -EFAULT; break;
		}
		if (!user_segment.bufsz || user_segment.bufsz > GW_LIMIT ||
		    user_segment.mem != GW_DEST ||
		    user_segment.memsz != PAGE_ALIGN(user_segment.bufsz)) {
			result = -EINVAL; break;
		}
		/* Snapshot validated metadata once; source bytes remain user memory. */
		segment.buf = (void __user *)user_segment.buf;
		segment.bufsz = user_segment.bufsz;
		segment.mem = user_segment.mem;
		segment.memsz = user_segment.memsz;
		peak_pages = allocated_pages;
		result = kimage_normal_alloc(&image, request.entry, 1, &segment);
		if (!result)
			result = kimage_load_normal_segment(image, &segment);
		if (!result) {
			kimage_terminate(image);
			staged = image;
			result = verify_image(&status);
			if (result)
				drop_image();
			else
				pr_info("gleworks_alpine_stage: staged %u bytes; %u pages owned\n",
					status.bytes, status.owned_pages);
		} else {
			kimage_free(image);
		}
		break;
	case GW_PREPARE:
        if (arg) { result = -EINVAL; break; }
        result = prepare_fixed_image();
        break;
    case GW_UNLOAD:
		if (arg) { result = -EINVAL; break; }
		drop_image();
		if (allocated_pages || owned_count)
			result = -EIO;
		else
			pr_info("gleworks_alpine_stage: unloaded image; zero owned pages\n");
		break;
	case GW_VERIFY:
		result = verify_image(&status);
		if (!result && copy_to_user((void __user *)arg, &status, sizeof(status)))
			result = -EFAULT;
		break;
	case GW_BUDGET:
		/* Local page-budget fault cases, never global allocator injection. */
		if (staged || allocated_pages) { result = -EBUSY; break; }
		if (arg > 1024) { result = -EINVAL; break; }
		allocation_budget = arg;
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&gw_lock);
	return result;
}

static int gw_open(struct inode *inode, struct file *file)
{
	int result = 0;
	if (!capable(CAP_SYS_BOOT))
		return -EPERM;
	mutex_lock(&gw_lock);
	if (opened)
		result = -EBUSY;
	else
		opened = 1;
	if (!result)
		allocation_budget = 1024;
	mutex_unlock(&gw_lock);
	return result;
}

static int gw_release(struct inode *inode, struct file *file)
{
	mutex_lock(&gw_lock);
	drop_image();
	opened = 0;
	mutex_unlock(&gw_lock);
	return 0;
}

static const struct file_operations gw_fops = {
	.owner = THIS_MODULE,
	.open = gw_open,
	.release = gw_release,
	.unlocked_ioctl = gw_ioctl,
};
static struct miscdevice gw_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gleworks_alpine_stage",
	.fops = &gw_fops,
	.mode = 0600,
};

static int __init gw_init(void)
{
	int result;
	BUILD_BUG_ON(sizeof(struct module) != 292);
	BUILD_BUG_ON(LINUX_VERSION_CODE != KERNEL_VERSION(3, 0, 35));
	BUILD_BUG_ON(PAGE_SIZE != 4096);
	BUILD_BUG_ON(sizeof(struct gw_segment) != sizeof(struct kexec_segment));
	result = misc_register(&gw_device);
	if (!result)
		pr_info("gleworks_alpine_stage: prepare-only ready; execution code absent\n");
	return result;
}
static void __exit gw_exit(void)
{
	misc_deregister(&gw_device);
	drop_image();
	pr_info("gleworks_alpine_stage: module removed; owned pages=%u\n", allocated_pages);
}
module_init(gw_init);
module_exit(gw_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Owned-chain and fixed-image preparation; no execute path");

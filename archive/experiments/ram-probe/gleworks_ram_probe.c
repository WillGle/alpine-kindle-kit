/* GPL-2.0-only: bounded allocation experiment, never a kernel handoff. */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/gfp.h>
#include <linux/string.h>

#define PROBE_PAGES 16
static unsigned long owned[PROBE_PAGES];

static void release_owned(void)
{
	unsigned int i;

	for (i = 0; i < PROBE_PAGES; i++) {
		if (owned[i]) {
			free_page(owned[i]);
			owned[i] = 0;
		}
	}
}

static int verify_owned(void)
{
	unsigned int i, j;

	for (i = 0; i < PROBE_PAGES; i++) {
		const volatile unsigned char *data;

		if (!owned[i])
			return -EIO;
		/* Volatile reads ensure the stored bytes are actually read back. */
		data = (const volatile unsigned char *)owned[i];
		for (j = 0; j < PAGE_SIZE; j++) {
			if (data[j] != (unsigned char)(0xa5 ^ i))
				return -EIO;
		}
	}
	return 0;
}

static int __init gleworks_ram_probe_init(void)
{
	unsigned int i;
	int error;

	BUILD_BUG_ON(sizeof(struct module) != 292);
	BUILD_BUG_ON(LINUX_VERSION_CODE != KERNEL_VERSION(3, 0, 35));
	BUILD_BUG_ON(PAGE_SIZE != 4096);
	for (i = 0; i < PROBE_PAGES; i++) {
		/* Order zero, no reclaim/IO waits and no emergency reserve access. */
		owned[i] = __get_free_page(GFP_NOWAIT | __GFP_NOWARN);
		if (!owned[i]) {
			release_owned();
			pr_info("gleworks_ram_probe: allocation declined; released owned pages\n");
			return -ENOMEM;
		}
		memset((void *)owned[i], 0xa5 ^ i, PAGE_SIZE);
	}
	error = verify_owned();
	if (error) {
		release_owned();
		pr_err("gleworks_ram_probe: initial readback failed; released owned pages\n");
		return error;
	}
	pr_info("gleworks_ram_probe: loaded; 16 pages, 65536 bytes verified\n");
	return 0;
}

static void __exit gleworks_ram_probe_exit(void)
{
	int error = verify_owned();

	release_owned();
	if (error)
		pr_err("gleworks_ram_probe: final readback failed; released owned pages\n");
	else
		pr_info("gleworks_ram_probe: unloaded; 65536 bytes reverified and released\n");
}

module_init(gleworks_ram_probe_init);
module_exit(gleworks_ram_probe_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Bounded 64 KiB RAM experiment; no execute or reboot interface");

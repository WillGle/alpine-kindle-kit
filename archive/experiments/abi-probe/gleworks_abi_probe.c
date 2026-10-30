/* GPL-2.0-only: inert ABI candidate for the captured PW3 kernel. */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

static int __init gleworks_abi_probe_init(void)
{
	/* Captured stock modules all have a 292-byte module descriptor. */
	BUILD_BUG_ON(sizeof(struct module) != 292);
	BUILD_BUG_ON(LINUX_VERSION_CODE != KERNEL_VERSION(3, 0, 35));
	pr_info("gleworks_abi_probe: loaded; module descriptor=%u bytes\n",
		(unsigned int)sizeof(struct module));
	return 0;
}

static void __exit gleworks_abi_probe_exit(void)
{
	pr_info("gleworks_abi_probe: unloaded\n");
}

module_init(gleworks_abi_probe_init);
module_exit(gleworks_abi_probe_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PW3 inert ABI probe; no boot, memory patch or device interface");

/* GPL-2.0-only: bounded stock-watchdog test; never a kernel handoff. */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/capability.h>
#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/err.h>
#include "protocol.h"

static DEFINE_MUTEX(guard_lock);
static atomic_t opened = ATOMIC_INIT(0);
static struct delayed_work recovery;
static struct file *watchdog;
static void (*restore_driver)(void);
static int state;
static unsigned int attempts;

/* Called only with guard_lock held, in sleepable process/workqueue context. */
static void restore_locked(void)
{
	if (!watchdog)
		return;
	/* NOWAYOUT close pings once. Reinit then restores the stock feed timer. */
	filp_close(watchdog, NULL);
	watchdog = NULL;
	restore_driver();
	state = GW_WD_RESTORED;
	pr_info("gleworks_watchdog_guard: restored stock watchdog feeding\n");
}

static void recover(struct work_struct *work)
{
	mutex_lock(&guard_lock);
	restore_locked();
	mutex_unlock(&guard_lock);
}

static int guard_open(struct inode *inode, struct file *file)
{
	if (!capable(CAP_SYS_BOOT))
		return -EPERM;
	if (atomic_cmpxchg(&opened, 0, 1))
		return -EBUSY;
	return 0;
}

static long guard_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long result = 0;
	struct file *candidate;
	if (!capable(CAP_SYS_BOOT))
		return -EPERM;
	if (arg)
		return -EINVAL;
	/* No lock while waiting for a worker that needs this same lock. */
	if (cmd == GW_WD_RESTORE)
		cancel_delayed_work_sync(&recovery);
	mutex_lock(&guard_lock);
	switch (cmd) {
	case GW_WD_STATE:
		result = state;
		break;
	case GW_WD_RESTORE:
		restore_locked();
		break;
	case GW_WD_ARM:
		if (watchdog || attempts >= 2) {
			result = -EBUSY;
			break;
		}
		/* Schedule recovery BEFORE open removes the stock driver's feed timer.
		 * The mutex excludes timeout restoration until open has completed. */
		if (!schedule_delayed_work(&recovery, GW_WD_DEADLINE * HZ)) {
			result = -EBUSY;
			break;
		}
		candidate = filp_open("/dev/misc/watchdog", O_WRONLY | O_NOFOLLOW, 0);
		if (IS_ERR(candidate)) {
			result = PTR_ERR(candidate);
			/* Failed open owns no watchdog. Do not touch another owner's timer. */
			cancel_delayed_work(&recovery);
			break;
		}
		watchdog = candidate;
		attempts++;
		state = GW_WD_ARMED;
		pr_info("gleworks_watchdog_guard: armed; restore deadline=%ds\n",
			GW_WD_DEADLINE);
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&guard_lock);
	return result;
}

static int guard_release(struct inode *inode, struct file *file)
{
	cancel_delayed_work_sync(&recovery);
	mutex_lock(&guard_lock);
	restore_locked();
	mutex_unlock(&guard_lock);
	atomic_set(&opened, 0);
	return 0;
}

static const struct file_operations operations = {
	.owner = THIS_MODULE,
	.open = guard_open,
	.release = guard_release,
	.unlocked_ioctl = guard_ioctl,
	.llseek = no_llseek,
};
static struct miscdevice guard_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gleworks_watchdog_guard",
	.fops = &operations,
	.mode = 0600,
};

static int __init guard_init(void)
{
	unsigned long address;
	BUILD_BUG_ON(sizeof(struct module) != 292);
	BUILD_BUG_ON(LINUX_VERSION_CODE != KERNEL_VERSION(3, 0, 35));
	/* Exact captured firmware symbol; never accept a user-supplied callback.
	 * Signature and timer semantics audited in stock imx2_wdt.c. */
	address = kallsyms_lookup_name("imx2_wdt_reinit");
	if (address != 0xc04542c8UL)
		return -ENODEV;
	restore_driver = (void (*)(void))address;
	INIT_DELAYED_WORK(&recovery, recover);
	return misc_register(&guard_device);
}

static void __exit guard_exit(void)
{
	misc_deregister(&guard_device);
	cancel_delayed_work_sync(&recovery);
	mutex_lock(&guard_lock);
	restore_locked();
	mutex_unlock(&guard_lock);
}

module_init(guard_init);
module_exit(guard_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Bounded watchdog trial with stock timer restoration; no flash or handoff");

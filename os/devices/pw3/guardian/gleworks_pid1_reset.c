/* GPL-2.0-only: independent RAM-only trial deadline, stock emergency restart. */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/capability.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/reboot.h>
#include "reset-protocol.h"
#include "reset-token.h"
static const unsigned int token[4]=GW_RESET_TOKEN;
static DEFINE_MUTEX(guard_lock);
static DECLARE_WAIT_QUEUE_HEAD(guard_wait);
static struct task_struct *worker;
static unsigned int state; /* 0 idle, 1 test, 2 real armed, 3 real fired, 4 test fired */
static unsigned long deadline;
static bool pinned;
static int guard_thread(void *unused)
{
    for(;;) {
        unsigned int mode;
        unsigned long until;
        long left;
        wait_event_interruptible(guard_wait,kthread_should_stop() || state==1 || state==2);
        if(kthread_should_stop()) return 0;
        mutex_lock(&guard_lock);
        mode=state; until=deadline;
        mutex_unlock(&guard_lock);
        if(mode!=1 && mode!=2) continue;
        left=(long)(until-jiffies);
        if(left>0) {
            wait_event_interruptible_timeout(guard_wait,kthread_should_stop() ||
                state!=mode || deadline!=until,left);
            continue;
        }
        if(kthread_should_stop()) return 0;
        mutex_lock(&guard_lock);
        if(state!=mode || deadline!=until || time_before(jiffies,deadline)) { mutex_unlock(&guard_lock); continue; }
        state=mode==1?4:3;
        mutex_unlock(&guard_lock);
        if(mode==1) {
            pr_info("gleworks_pid1_reset: returning deadline test fired\n");
            /* A completed test must release the module pin, otherwise module
             * removal and a later real arm both stay blocked until reboot. */
            mutex_lock(&guard_lock);
            if(pinned) { pinned=false; module_put(THIS_MODULE); }
            mutex_unlock(&guard_lock);
            continue;
        }
        /* Process context is required: stock ARM pm_restart invokes PMIC I2C.
         * Bypass global sync, reboot notifiers and device shutdown. No guessed
         * register write, bootmode selector, watchdog open or kernel handoff. */
        pr_emerg("gleworks_pid1_reset: RAM trial deadline; stock emergency restart\n");
        emergency_restart();
        pr_err("gleworks_pid1_reset: emergency restart unexpectedly returned\n");
    }
}
static long guard_ioctl(struct file *file,unsigned int command,unsigned long argument)
{
    struct gw_guard request;
    int result=0;
    if(!capable(CAP_SYS_BOOT)) return -EPERM;
    if(command!=GW_GUARD_STATUS && command!=GW_GUARD_TEST && command!=GW_GUARD_ARM && command!=GW_GUARD_CANCEL) return -ENOTTY;
    if(copy_from_user(&request,(void __user *)argument,sizeof(request))) return -EFAULT;
    if(memcmp(request.token,token,sizeof(token))) return -EACCES;
    mutex_lock(&guard_lock);
    if(command==GW_GUARD_CANCEL) {
        if(state==3) { result=-EBUSY; goto out; }
        state=0;
        wake_up(&guard_wait);
        if(pinned) { pinned=false; module_put(THIS_MODULE); }
    } else if(command==GW_GUARD_ARM || command==GW_GUARD_TEST) {
        /* A completed test leaves state=4 for STATUS, but it no longer holds
         * the module pin.  A real ARM may proceed from that idle-post-test
         * state; any other non-zero state is still busy. */
        if((state && !(command==GW_GUARD_ARM && state==4)) || pinned) {
            result=-EBUSY; goto out;
        }
        if(!try_module_get(THIS_MODULE)) { result=-ENODEV; goto out; }
        pinned=true;
        deadline=jiffies+(command==GW_GUARD_ARM?90:2)*HZ;
        state=command==GW_GUARD_ARM?2:1;
        wake_up(&guard_wait);
    }
    request.state=state;
    request.remaining_seconds=(state==1 || state==2) && time_before(jiffies,deadline)?(deadline-jiffies+HZ-1)/HZ:0;
    if(copy_to_user((void __user *)argument,&request,sizeof(request))) result=-EFAULT;
out:
    mutex_unlock(&guard_lock);
    return result;
}
static const struct file_operations guard_fops={.owner=THIS_MODULE,.unlocked_ioctl=guard_ioctl};
static struct miscdevice guard_device={.minor=MISC_DYNAMIC_MINOR,.name="gleworks_pid1_reset",.fops=&guard_fops};
static int __init guard_init(void)
{
    int result;
    /* No struct-module layout assertion: vermagic/modinfo already rejects an
     * incompatible module at load time. */
    worker=kthread_run(guard_thread,NULL,"gw-pid1-reset");
    if(IS_ERR(worker)) return PTR_ERR(worker);
    result=misc_register(&guard_device);
    if(result) { kthread_stop(worker); return result; }
    pr_info("gleworks_pid1_reset: idle; no deadline armed\n");
    return 0;
}
static void __exit guard_exit(void)
{
    misc_deregister(&guard_device);
    wake_up(&guard_wait);
    kthread_stop(worker);
    pr_info("gleworks_pid1_reset: normal removal\n");
}
module_init(guard_init);
module_exit(guard_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Nonce-bound RAM PID1 trial deadline; unchanged stock reset path");

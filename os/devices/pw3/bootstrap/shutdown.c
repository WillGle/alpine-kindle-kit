/* SPDX-License-Identifier: GPL-3.0-or-later
 * BusyBox init restart action: leave the nested ext3/FAT filesystems clean.
 * The static executable lives on /run tmpfs before PID1 execs it.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static void stop(const char *why) {
    dprintf(2,"GW_SHUTDOWN_STOP %s errno=%d\n",why,errno);
    /* A failed unmount must not be followed by a forced filesystem reset. */
    for(;;) pause();
}

static void directory(const char *path) {
    if(mkdir(path,0700) && errno!=EEXIST) stop(path);
}

static void journal(const char *phase) {
    int fd=open("/control/gleworks-alpine/shutdown.log",O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0) stop("shutdown journal");
    if(write(fd,phase,strlen(phase))!=(ssize_t)strlen(phase) || fsync(fd)) {
        close(fd);stop("sync shutdown journal");
    }
    close(fd);
}

int main(void) {
    if(getpid()!=1) { fputs("PID1 required\n",stderr);return 1; }
    /* BusyBox restart already killed other processes; repeat for stragglers. */
    kill(-1,SIGKILL);
    directory("/run/old-root");directory("/run/control");
    directory("/run/dev");directory("/run/proc");
    if(chdir("/run") || syscall(SYS_pivot_root,".","old-root") || chdir("/")) stop("enter shutdown tmpfs");
    if(mount("/old-root/boot/gleworks-control","/control",NULL,MS_MOVE,NULL)) stop("move backing FAT");
    if(mount("/old-root/dev","/dev",NULL,MS_MOVE,NULL)) stop("move devices");
    if(mount("/old-root/proc","/proc",NULL,MS_MOVE,NULL)) stop("move proc");
    /* Bootstrap's inherited console descriptors may refer to device inodes
     * on the Alpine image underneath its later /dev tmpfs mount. */
    for(int fd=0;fd<256;fd++) close(fd);
    int console=open("/dev/console",O_RDWR|O_NOCTTY);
    if(console<0) stop("shutdown console");
    for(int fd=0;fd<3;fd++) if(dup2(console,fd)<0) stop("shutdown console dup");
    if(console>2) close(console);
    /* These contain no backing storage and no userspace is left on them. */
    if(umount2("/old-root/sys",MNT_DETACH) || umount2("/old-root/tmp",MNT_DETACH)) stop("detach virtual mounts");
    sync();
#if defined(GW_SEPARATE_DATA) && GW_SEPARATE_DATA
    /* A nested data image must be cleanly unmounted before its backing root. */
    if(umount("/old-root/data") && errno!=EINVAL && errno!=ENOENT) stop("unmount data image");
    sync();
#endif
    if(umount("/old-root")) stop("unmount Alpine root");
    journal("GW_ALPINE_ROOT_CLEANLY_UNMOUNTED\n");
    /* Only an orderly persistent shutdown permits another normal boot. */
    if(access("/persistent-boot",F_OK)==0 &&
       unlink("/control/gleworks-alpine/boot.pending") && errno!=ENOENT)
        stop("clear clean boot marker");
    sync();
    /* Root loop AUTOCLEAR has now closed its writable image descriptor. */
    if(umount("/control")) stop("unmount backing FAT");
    dprintf(2,"GW_NESTED_FILESYSTEMS_CLEANLY_UNMOUNTED\n");
    sync();reboot(RB_AUTOBOOT);stop("reboot returned");return 1;
}

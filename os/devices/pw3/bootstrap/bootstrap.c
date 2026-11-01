/* SPDX-License-Identifier: GPL-3.0-or-later
 * PW3 rescue bootstrap and native Alpine entrypoint. Pivot pattern informed by
 * ChrisPei/PaperOS native/bootstrap.c; device ABI is checked in the emulator.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <linux/loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include "../guardian/reset-protocol.h"
#include "../guardian/reset-token.h"

#define CONTROL "/control"
#define ROOT "/newroot"
#define REQUEST CONTROL "/gleworks-alpine/boot.once"
#define ENABLED CONTROL "/gleworks-alpine/boot.enabled"
#define PENDING CONTROL "/gleworks-alpine/boot.pending"
#define REPAIRING CONTROL "/gleworks-alpine/boot.repairing"
#define IMAGE CONTROL "/gleworks-alpine/system.ext3"
#define TOKEN "gleworks-alpine-v1\n"
#define MODULES "/modules/"
_Static_assert(sizeof(struct loop_info64)==232,"stock loop_info64 ABI");
static int fat_loop=-1, root_loop=-1, part=-1, image=-1;
static int fat_mounted, root_mounted, control_moved;
static int supervised_trial;
static int rescue_repair_trial;
static int persistent_boot,guardian_armed;

static void halt(const char *why) {
    dprintf(2,"GW_BOOTSTRAP_HALT %s errno=%d\n",why,errno);
    if(supervised_trial || persistent_boot) { sync();reboot(RB_AUTOBOOT); }
    for(;;) pause();
}

/* Stock tools run before pivot, with no host export and no armed reset.
 * Do not expose a filesystem until its repair child has actually stopped. */
static int stock_tool(char *const argv[], unsigned int seconds, const char *log) {
    pid_t child=fork();int status;
    if(child<0) return -1;
    if(!child) {
        int fd=open(log,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC|O_NOFOLLOW,0600);
        if(fd<0) _exit(126);
        if(dup2(fd,1)<0 || dup2(fd,2)<0) _exit(126);
        if(fd>2) close(fd);
        execv(argv[0],argv);_exit(127);
    }
    for(unsigned int n=0;n<seconds;n++) {
        pid_t result=waitpid(child,&status,WNOHANG);
        if(result==child) return WIFEXITED(status)?WEXITSTATUS(status):-1;
        if(result<0 && errno!=EINTR) return -1;
        sleep(1);
    }
    kill(child,SIGKILL);
    while(waitpid(child,&status,0)<0 && errno==EINTR) {}
    return -1;
}

static void panel_status(const char *message) {
    if(mknod("/dev/fb0",S_IFCHR|0600,makedev(29,0)) && errno!=EEXIST) return;
    char *const argv[]={"/usr/sbin/eips","0","0",(char *)message,NULL};
    (void)stock_tool(argv,3,"/dev/null");
}

static int image_clean(void) {
    unsigned char super[104];
    return pread(image,super,sizeof(super),1024)==(ssize_t)sizeof(super) &&
        super[56]==0x53 && super[57]==0xef && super[58]==1 && super[59]==0 && !(super[96]&4);
}

static int load_module(const char *name,const char *parameters) {
    char path[160]; struct stat st; int fd,rc,saved; void *data;
    snprintf(path,sizeof(path),MODULES"%s.ko",name);
    fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0) return -1;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size>4*1024*1024) {
        close(fd);errno=EINVAL;return -1;
    }
    data=malloc(st.st_size);
    if(!data) { close(fd); return -1; }
    size_t used=0;
    while(used<(size_t)st.st_size) {
        ssize_t n=read(fd,(char*)data+used,st.st_size-used);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) { free(data);close(fd);errno=EIO;return -1; }
        used+=n;
    }
    rc=syscall(SYS_init_module,data,used,parameters);saved=errno;
    free(data);close(fd);errno=saved;return rc;
}

static void arm_trial_guard(void) {
    FILE *misc; char line[160],name[100]; unsigned int minor; int fd,found=0;
    struct gw_guard request={.token=GW_RESET_TOKEN};
    if(load_module("gleworks_pid1_reset","")) halt("early guardian module");
    misc=fopen("/proc/misc","r");
    if(!misc) halt("early guardian lookup");
    while(fgets(line,sizeof(line),misc))
        if(sscanf(line,"%u %99s",&minor,name)==2 && !strcmp(name,"gleworks_pid1_reset")) { found=1;break; }
    fclose(misc);
    if(!found || mknod("/dev/gleworks_pid1_reset",S_IFCHR|0600,makedev(10,minor))) halt("early guardian node");
    fd=open("/dev/gleworks_pid1_reset",O_RDWR|O_CLOEXEC);
    if(fd<0 || ioctl(fd,GW_GUARD_ARM,&request) || request.state!=2 || request.remaining_seconds<85)
        halt("early guardian arm");
    close(fd);
    guardian_armed=1;
    dprintf(2,"GW_BOOTSTRAP_GUARD_ARMED seconds=%u\n",request.remaining_seconds);
}

static int prepare_usb_cable(void) {
    FILE *state=fopen("/proc/asession","r");
    char line[160]; unsigned int connected=0,host=0; int fd,rc,saved;
    const char command[]="5 1\n";
    if(!state) return -1;
    while(fgets(line,sizeof(line),state)) {
        (void)sscanf(line,"max77696_charger_get_connected() %u",&connected);
        (void)sscanf(line,"max77696_uic_is_usbhost() %u",&host);
    }
    fclose(state);
    if(connected!=1 || host!=1) return 0;
    /* Prime the vendor B-session before binding the first gadget. Its initial
     * low-power path otherwise misses the already-present cable. Use write(2):
     * this old proc handler crashes on musl stdio's empty first writev vector. */
    fd=open("/proc/asession",O_WRONLY|O_CLOEXEC);
    if(fd<0) return -1;
    rc=write(fd,command,sizeof(command))==(ssize_t)sizeof(command)?0:-1;
    saved=errno;close(fd);errno=saved;return rc;
}

static int read_usb_state(const char *path,char *value,size_t size) {
    int fd=open(path,O_RDONLY|O_CLOEXEC);ssize_t n;
    if(fd<0) return -1;
    n=read(fd,value,size-1);close(fd);
    if(n<0) return -1;
    value[n]=0;return 0;
}

static void open_rescue_repair_window(void) {
    char state[160];int fd;
    struct gw_guard request={.token=GW_RESET_TOKEN};
    /* Enumerate an EMPTY removable gadget first. No host can mount/write p4
     * until the reset deadline has been cancelled successfully. */
    if(load_module("g_file_storage","removable=1 vendor=0x0525 product=0xa4a6 release=0x0100 recovery_mode=1"))
        halt("empty rescue handshake gadget");
    for(;;) {
        fd=open("/dev/gleworks_pid1_reset",O_RDWR|O_CLOEXEC);
        if(fd<0 || ioctl(fd,GW_GUARD_STATUS,&request) || request.state!=2 || request.remaining_seconds<15)
            halt("rescue handshake deadline");
        if(!read_usb_state("/sys/devices/platform/fsl-usb2-udc/gadget/connected",state,sizeof(state)) && !strcmp(state,"1\n")) {
            if(ioctl(fd,GW_GUARD_CANCEL,&request) || request.state!=0) halt("cancel rescue reset before export");
            close(fd);break;
        }
        close(fd);sleep(1);
    }
    if(syscall(SYS_delete_module,"g_file_storage",O_NONBLOCK)) halt("remove empty rescue handshake gadget");
    dprintf(2,"GW_BOOTSTRAP_RESCUE_RESET_CANCELLED_BEFORE_EXPORT\n");
}

static void rescue(const char *why) {
    int repair_window=rescue_repair_trial || !supervised_trial;
    panel_status("Alpine USB rescue - connect USB to recover");
    dprintf(2,"GW_BOOTSTRAP_RESCUE %s errno=%d\n",why,errno);
    if(control_moved) {
        if(mount(ROOT"/boot/gleworks-control",CONTROL,NULL,MS_MOVE,NULL)) halt("restore control mount");
        control_moved=0;fat_mounted=1;
    }
    if(root_mounted && umount(ROOT)) halt("unmount Alpine before USB export");
    root_mounted=0;
    if(root_loop>=0) { close(root_loop);root_loop=-1; }
    if(image>=0) { close(image);image=-1; }
    if(fat_mounted && umount(CONTROL)) halt("unmount FAT before USB export");
    fat_mounted=0;
    if(fat_loop>=0) { close(fat_loop);fat_loop=-1; }
    if(part>=0) { close(part);part=-1; }
    sync();
    if(load_module("fsl_otg_arc","") || load_module("arcotg_udc","") || prepare_usb_cable())
        halt("USB rescue controller");
    if(repair_window && guardian_armed) open_rescue_repair_window();
    if(load_module("g_file_storage",repair_window?
       "file=/dev/mmcblk0p4 removable=1 vendor=0x0525 product=0xa4a5 release=0x0200 recovery_mode=1":
       "file=/dev/mmcblk0p4 removable=1 vendor=0x0525 product=0xa4a5 release=0x0100 recovery_mode=1"))
        halt("USB rescue module");
    dprintf(2,"GW_BOOTSTRAP_USB_RESCUE_READY backing=/dev/mmcblk0p4\n");
    if(repair_window) for(;;) {
        char backing[160];
        /* SCSI eject closes the backing file only after host unmount/eject.
         * Reboot without mounting host-writable storage in this process. */
        if(!read_usb_state("/sys/devices/platform/fsl-usb2-udc/gadget/gadget-lun0/file",backing,sizeof(backing)) &&
           (!backing[0] || !strcmp(backing,"\n"))) {
            /* Let the successful SCSI eject status reach the host first. */
            sleep(2);sync();reboot(RB_AUTOBOOT);halt("rescue eject reboot returned");
        }
        sleep(1);
    }
    for(;;) pause();
}

static int attach(int backing,unsigned long long offset,int readonly,char *device,size_t size) {
    int n;
    for(n=0;n<8;n++) {
        struct loop_info64 info; int fd,saved;
        snprintf(device,size,"/dev/loop%d",n);
        if(mknod(device,S_IFBLK|0600,makedev(7,n)) && errno!=EEXIST) return -1;
        fd=open(device,O_RDWR|O_CLOEXEC);
        if(fd<0) continue;
        if(!ioctl(fd,LOOP_GET_STATUS64,&info) || errno!=ENXIO) { close(fd);continue; }
        if(ioctl(fd,LOOP_SET_FD,backing)) { close(fd);continue; }
        memset(&info,0,sizeof(info));info.lo_offset=offset;
        info.lo_flags=LO_FLAGS_AUTOCLEAR|(readonly?LO_FLAGS_READ_ONLY:0);
        if(!ioctl(fd,LOOP_SET_STATUS64,&info)) return fd;
        saved=errno;ioctl(fd,LOOP_CLR_FD,0);close(fd);errno=saved;return -1;
    }
    errno=ENOSPC;return -1;
}

int main(int argc,char **argv) {
    char fat_device[40],root_device[40],token[sizeof(TOKEN)+1];
    int fd,persistent=0; ssize_t n; struct stat st;
    if(argc==2 && !strcmp(argv[1],"--check")) { puts("gleworks-pw3-bootstrap-spike-v1");return 0; }
    if(getpid()!=1) { fputs("PID1 required\n",stderr);return 1; }
    rescue_repair_trial=argc==2 && !strcmp(argv[1],"--rescue-trial");
    supervised_trial=rescue_repair_trial || (argc==2 && !strcmp(argv[1],"--trial"));
    if(mount("proc","/proc","proc",0,NULL) || mount("sysfs","/sys","sysfs",0,NULL) ||
       mount("tmpfs","/dev","tmpfs",MS_NOSUID,"mode=0755")) halt("early mounts");
    if(mknod("/dev/console",S_IFCHR|0600,makedev(5,1)) ||
       mknod("/dev/null",S_IFCHR|0666,makedev(1,3)) ||
       mknod("/dev/mmcblk0p4",S_IFBLK|0600,makedev(179,4))) halt("early devices");
    if(supervised_trial) arm_trial_guard();
    part=open("/dev/mmcblk0p4",O_RDWR|O_CLOEXEC);
    if(part<0) rescue("open p4");
    fat_loop=attach(part,8192,0,fat_device,sizeof(fat_device));
    if(fat_loop<0) rescue("map FAT");
    if(mount(fat_device,CONTROL,"vfat",MS_NODEV|MS_NOSUID|MS_NOEXEC,"utf8,umask=0077")) rescue("mount FAT");
    fat_mounted=1;
    fd=open(REQUEST,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0 && errno==ENOENT && !supervised_trial) {
        fd=open(ENABLED,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        persistent=1;
    }
    if(fd<0) rescue("no one-shot request");
    n=read(fd,token,sizeof(token));close(fd);
    if(n!=(ssize_t)strlen(TOKEN) || memcmp(token,TOKEN,strlen(TOKEN))) rescue("invalid request");
    if(!persistent && unlink(REQUEST)) rescue("consume request");
    if(persistent) persistent_boot=1;
    sync();
    image=open(IMAGE,(persistent?O_RDWR:O_RDONLY)|O_CLOEXEC|O_NOFOLLOW);
    if(image<0) rescue("missing Alpine image");
    if(fstat(image,&st) || !S_ISREG(st.st_mode) || st.st_size<64*1024*1024 || st.st_size>1024LL*1024*1024)
        rescue("image size/type");
    if(persistent) {
        /* A repair interrupted by power loss must not retry forever. */
        if(access(REPAIRING,F_OK)==0) rescue("interrupted offline repair");
        /* Reject unknown startup records before modifying the image. */
        fd=open(PENDING,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        if(fd>=0) {
            n=read(fd,token,sizeof(token));close(fd);
            if(n!=(ssize_t)strlen(TOKEN) || memcmp(token,TOKEN,strlen(TOKEN))) rescue("invalid pending marker");
        } else if(errno!=ENOENT) rescue("read pending marker");
        if(!image_clean()) {
            char capacity[16];int battery=open("/sys/class/power_supply/max77696-battery/capacity",O_RDONLY|O_CLOEXEC);
            ssize_t count=battery<0?-1:read(battery,capacity,sizeof(capacity)-1);
            if(battery>=0) close(battery);
            if(count<=0) rescue("battery unavailable for offline repair");
            capacity[count]=0;
            if(atoi(capacity)<50) rescue("battery too low for offline repair");
            int marker=open(REPAIRING,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
            if(marker<0) rescue("record offline repair");
            if(write(marker,TOKEN,strlen(TOKEN))!=(ssize_t)strlen(TOKEN) || fsync(marker)) {
                close(marker);rescue("sync offline repair marker");
            }
            close(marker);sync();
            panel_status("Alpine checking storage - keep power connected");
            char *const check[]={"/sbin/e2fsck","-p",IMAGE,NULL};
            int result=stock_tool(check,60,CONTROL "/gleworks-alpine/repair.log");
            sync();
            if((result!=0 && result!=1) || !image_clean()) rescue("offline repair requires host");
        }
        if(access(PENDING,F_OK)==0 && access(REPAIRING,F_OK)!=0) {
            int marker=open(REPAIRING,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
            if(marker<0) rescue("record stale startup retry");
            if(write(marker,TOKEN,strlen(TOKEN))!=(ssize_t)strlen(TOKEN) || fsync(marker)) {
                close(marker);rescue("sync stale startup retry");
            }
            close(marker);sync();
        }
        /* A stale pending marker is acknowledged only after a clean image,
         * before a new guarded readonly startup. Unknown markers fail closed. */
        fd=open(PENDING,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        if(fd>=0) {
            n=read(fd,token,sizeof(token));close(fd);
            if(n!=(ssize_t)strlen(TOKEN) || memcmp(token,TOKEN,strlen(TOKEN))) rescue("invalid pending marker");
            if(unlink(PENDING)) rescue("replace pending marker");
        } else if(errno!=ENOENT) rescue("read pending marker");
        fd=open(PENDING,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
        if(fd<0) rescue("record guarded startup");
        if(write(fd,TOKEN,strlen(TOKEN))!=(ssize_t)strlen(TOKEN) || fsync(fd)) {
            close(fd);rescue("sync guarded startup");
        }
        close(fd);sync();
        arm_trial_guard();
    }
    root_loop=attach(image,0,!persistent,root_device,sizeof(root_device));
    if(root_loop<0) rescue("map Alpine");
    /* Even persistent roots start readonly under the recovery deadline.
     * Alpine cancels that deadline before enabling writes and acknowledging. */
    if(mount(root_device,ROOT,"ext3",MS_RDONLY|MS_NOATIME,NULL)) rescue("mount Alpine");
    root_mounted=1;
    if(access(ROOT"/sbin/init",X_OK) || access(ROOT"/etc/alpine-release",R_OK) ||
       access(ROOT"/old-root",F_OK) || access(ROOT"/boot/gleworks-control",F_OK)) rescue("Alpine root layout");
    if(mount(CONTROL,ROOT"/boot/gleworks-control",NULL,MS_MOVE,NULL)) rescue("move control");
    fat_mounted=0;control_moved=1;
    if(chdir(ROOT) || syscall(SYS_pivot_root,".","old-root")) {
        chdir("/");rescue("pivot root");
    }
    if(chdir("/")) halt("enter Alpine root");
    fd=open("/dev/console",O_RDWR|O_NOCTTY|O_CLOEXEC);
    if(fd<0) halt("Alpine console");
    for(int i=0;i<3;i++) if(dup2(fd,i)<0) halt("console dup");
    if(fd>2) close(fd);
    if(umount2("/old-root",MNT_DETACH)) halt("detach bootstrap root");
    for(int i=3;i<256;i++) close(i);
    dprintf(2,"GW_BOOTSTRAP_ALPINE_EXEC\n");
    if(setenv("GLEWORKS_BOOT_MODE",persistent?"persistent":"trial",1)) halt("init boot mode");
    char *const init[]={"/sbin/init",NULL};
    execv(init[0],init);
    /* The request is already consumed, so a failed exec returns to rescue. */
    dprintf(2,"GW_BOOTSTRAP_EXEC_FAILED errno=%d; resetting to rescue\n",errno);
    sync();reboot(RB_AUTOBOOT);
    halt("Alpine exec reboot returned");return 1;
}

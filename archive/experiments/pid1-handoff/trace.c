/* GPL-2.0-only: reversible child-qualified exec transfer; all target data in RAM. */
#include "sys.h"
#include "guard.h"
#define TRACE_TRACEME 0
#define TRACE_PEEK 2
#define TRACE_POKE 4
#define TRACE_CONT 7
#define TRACE_GETREGS 12
#define TRACE_SETREGS 13
#define TRACE_ATTACH 16
#define TRACE_DETACH 17
#define TRACE_OPTIONS 0x4200
#define TRACE_EXEC_OPTION 16
#define TRACE_EXEC_EVENT 4
struct registers { unsigned int r[18]; };
static int trace(int op,int pid,unsigned int at,unsigned int data) { return sys4(26,op,pid,at,data); }
static int wait_stop(int pid,int *status)
{
    int i;
    for(i=0;i<500;i++) {
        /* Stock init has a non-SIGCHLD exit signal; old kernels do not add
         * __WALL implicitly for ptrace, unlike newer wait implementations. */
        int r=sys4(114,pid,P(status),0x40000001U,0);
        if(r==pid) return 0;
        if(r<0 && r!=-4) { say("FAIL wait4 result="); hex(r); return -1; }
        pause_ms(10);
    }
    return -1;
}
static int stopped(int status) { return (status&255)==127; }
static int words(int pid,unsigned int at,unsigned int *data,unsigned int count,int writing)
{
    unsigned int i;
    for(i=0;i<count;i++)
        if(trace(writing?TRACE_POKE:TRACE_PEEK,pid,at+4*i,writing?data[i]:P(data+i))) return -1;
    return 0;
}
static unsigned int put_string(unsigned char *b,unsigned int *at,const char *s,unsigned int remote)
{
    unsigned int address=remote+*at;
    do { b[(*at)++]=*s; } while(*s++);
    return address;
}
/* Caller has a stopped single-threaded tracee. No assumptions about libc,
 * executable addresses, ARM/Thumb state or mmap addresses are hard-coded. */
static int transfer(int pid,const char *program,const char *arg1,const char *arg2,const char *arg3)
{
    struct registers before, next;
    unsigned int original_code[2],original_stack[128],data[128],patch[2]={0xef000000,0xe7f000f0};
    unsigned int code,stack,offset=0,argv[5],i;
    int status=0,code_changed=0,stack_changed=0,result=-1;
    if(length(program)>120 || length(arg1)>120 || length(arg2)>120 || length(arg3)>64) return -1;
    if(trace(TRACE_GETREGS,pid,0,P(&before))) return -1;
    if((before.r[16]&31)!=16 || before.r[13]<8192) return -1;
    /* signal.c restarts nanosleep/restart_block when PC equals its saved
     * continuation. Use a distinct aligned instruction address on that page. */
    code=(before.r[15]&~3U)^0x10U;
    stack=(before.r[13]-1024)&~7U;
    if(code<4096 || words(pid,code,original_code,2,0) || words(pid,stack,original_stack,128,0)) return -1;
    for(i=0;i<128;i++) data[i]=0;
    argv[0]=put_string((unsigned char *)data,&offset,program,stack);
    argv[1]=put_string((unsigned char *)data,&offset,arg1,stack);
    argv[2]=put_string((unsigned char *)data,&offset,arg2,stack);
    argv[3]=put_string((unsigned char *)data,&offset,arg3,stack);
    argv[4]=0;
    offset=(offset+3)&~3U;
    for(i=0;i<5;i++) data[offset/4+i]=argv[i];
    if(trace(TRACE_OPTIONS,pid,0,TRACE_EXEC_OPTION)) return -1;
    /* Arm restoration before writing: even a partially failed POKE is covered. */
    stack_changed=1;
    if(words(pid,stack,data,128,1)) goto restore;
    code_changed=1;
    if(words(pid,code,patch,2,1)) goto restore;
    next=before;
    next.r[0]=argv[0]; next.r[1]=stack+offset; next.r[2]=0;
    next.r[7]=11; next.r[15]=code;
    next.r[16]&=~(0x20U|0x01000000U|0x0600fc00U); /* ARM, no inherited Thumb IT state */
    if(trace(TRACE_SETREGS,pid,0,P(&next))) goto restore;
    if(trace(TRACE_CONT,pid,0,0)) goto restore;
    if(wait_stop(pid,&status) || !stopped(status)) {
        say("FAIL target did not stop; guardian recovery required\n");
        return -2;
    }
    if((status>>16)==TRACE_EXEC_EVENT && ((status>>8)&255)==5) {
        /* Successful exec replaced the old mappings. Do not restore old memory. */
        if(trace(TRACE_DETACH,pid,0,0)) return -2;
        say("EXEC_EVENT confirmed; target detached\n");
        return 1;
    }
    if(trace(TRACE_GETREGS,pid,0,P(&next))) goto restore;
    say("Exec rejected; restoring original state, r0="); hex(next.r[0]);
    result=0;
restore:
    if(code_changed && words(pid,code,original_code,2,1)) return -2;
    if(stack_changed && words(pid,stack,original_stack,128,1)) return -2;
    if(trace(TRACE_SETREGS,pid,0,P(&before)) || trace(TRACE_OPTIONS,pid,0,0) || trace(TRACE_DETACH,pid,0,0)) return -2;
    return result;
}
static void child_check(const char *self,int valid,int attached)
{
    int child=sys4(2,0,0,0,0),status=0,result;
    if(child<0) finish(1);
    if(!child) {
        if(attached) for(;;) pause_ms(1000);
        if(trace(TRACE_TRACEME,0,0,0)) finish(2);
        sys4(37,sys4(20,0,0,0,0),19,0,0);
        say("CHILD_RESUMED_AFTER_ROLLBACK\n"); finish(0);
    }
    if(attached && trace(TRACE_ATTACH,child,0,0)) finish(3);
    if(wait_stop(child,&status) || !stopped(status)) finish(3);
    result=transfer(child,valid?self:"/no-such-gleworks-executable","--exec-proof","child-only","");
    if(result!=(valid?1:0)) { say("FAIL child transfer\n"); sys4(37,child,9,0,0); finish(4); }
    if(attached && !valid) sys4(37,child,15,0,0);
    if(wait_stop(child,&status) || status!=(attached&&!valid?15:0)) { say("FAIL child final status\n"); finish(5); }
    say(valid?"PASS child successful exec event\n":"PASS child rejected exec rollback\n");
}
int start_c(unsigned int *initial) __attribute__((used));
int start_c(unsigned int *initial)
{
    const char **a=(const char **)(initial+1);
    if(initial[0]==4 && same(a[1],"--exec-proof") && same(a[2],"child-only") && !a[3][0]) { say("CHILD_EXEC_PROOF\n"); finish(0); }
    if(initial[0]==2 && same(a[1],"--self-check")) {
        child_check(a[0],0,0); child_check(a[0],1,0);
        child_check(a[0],0,1); child_check(a[0],1,1);
        say("PASS returning child qualification (TRACEME and ATTACH)\n"); finish(0);
    }
    if(initial[0]==3 && same(a[1],"--enter-init")) {
        const char *root="/dev/shm/gleworks-pid1-handoff/root";
        char path[128]; unsigned int i; int status=0,result;
        if(sys4(199,0,0,0,0) || length(a[2])!=32) finish(1);
        for(i=0;i<32;i++) if(!((a[2][i]>='0'&&a[2][i]<='9')||(a[2][i]>='a'&&a[2][i]<='f'))) finish(1);
        result=sys4(85,P("/proc/1/exe"),P(path),127,0);
        if(result<0 || result>=127) finish(1);
        path[result]=0;
        if(!same(path,"/sbin/init.exe") || !live_guard()) finish(1);
        /* An SSH hangup must not let exit_ptrace resume partially edited PID1.
         * SIGKILL remains unmaskable; the independent recovery deadline stays on. */
        {
            unsigned int blocked[2]={~0U,~0U};
            if(sys4(175,0,P(blocked),0,8)) finish(1);
        }
        if(trace(TRACE_ATTACH,1,0,0)) { say("FAIL PID1 attach\n"); finish(1); }
        if(wait_stop(1,&status) || !stopped(status)) { trace(TRACE_DETACH,1,0,0); say("FAIL PID1 stop\n"); finish(1); }
        result=transfer(1,"/dev/shm/gleworks-pid1-handoff/bootstrap","--enter",root,a[2]);
        if(result==-2) { say("STOP uncertain state; waiting for independent guardian reset\n"); for(;;) pause_ms(100); }
        if(result<0) trace(TRACE_DETACH,1,0,0);
        finish(result==1?0:1);
    }
    say("usage: trace --self-check | --enter-init NONCE (armed guardian required)\n"); finish(1);
}
void _start(void) __attribute__((naked));
void _start(void) { __asm__ volatile("mov r0,sp\nbl start_c\n1:b 1b"); }

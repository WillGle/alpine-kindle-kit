/* GPL-2.0-only: RAM-root PID1 bootstrap; recovery belongs to kernel guardian. */
#include "sys.h"
#include "guard.h"
static const char root[]="/dev/shm/gleworks-pid1-handoff/root";
static void failed(void) __attribute__((noreturn));
static void failed(void)
{
    say("FAIL PID1 bootstrap; awaiting independent kernel recovery deadline\n");
    for(;;) pause_ms(1000);
}
int start_c(unsigned int *initial) __attribute__((used));
int start_c(unsigned int *initial)
{
    const char **a=(const char **)(initial+1);
    char nonce[33]; int fd,n;
    char *env[]={"PATH=/bin:/sbin","HOME=/root","TERM=linux",0};
    char *args[]={"/sbin/init",0};
    if(initial[0]!=4 || !same(a[1],"--enter") || !same(a[2],root) || length(a[3])!=32 ||
       sys4(20,0,0,0,0)!=1 || sys4(199,0,0,0,0) || !live_guard()) failed();
    if(sys4(12,P(root),0,0,0)) failed();
    fd=sys4(5,P("etc/trial-nonce"),0,0,0);
    if(fd<0) failed();
    n=sys4(3,fd,P(nonce),33,0); sys4(6,fd,0,0,0);
    if(n!=33 || nonce[32]!='\n') failed();
    nonce[32]=0;
    if(!same(nonce,a[3])) failed();
    /* Kernel, IRQs, clocks, devices and old process mappings stay intact. */
    if(sys4(61,P("."),0,0,0) || sys4(12,P("/"),0,0,0)) failed();
    say("RAM Alpine root selected; exec Alpine init as PID1\n");
    sys4(11,P(args[0]),P(args),P(env),0);
    failed();
}
void _start(void) __attribute__((naked));
void _start(void) { __asm__ volatile("mov r0,sp\nbl start_c\n1:b 1b"); }

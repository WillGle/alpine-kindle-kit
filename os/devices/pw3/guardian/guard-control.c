/* Freestanding interface to the nonce-bound kernel recovery deadline. */
#include "sys.h"
#include "guard.h"
int start_c(unsigned int *initial) __attribute__((used));
int start_c(unsigned int *initial)
{
    const char **a=(const char **)(initial+1);
    unsigned int command=0;
    struct gw_guard answer;
    int result;
    if(initial[0]!=2) finish(1);
    if(same(a[1],"status")) command=GW_GUARD_STATUS;
    if(same(a[1],"test")) command=GW_GUARD_TEST;
    if(same(a[1],"arm")) command=GW_GUARD_ARM;
    if(same(a[1],"cancel")) command=GW_GUARD_CANCEL;
    if(!command) finish(1);
    result=guard_request(command,&answer);
    if(result) { say("FAIL guardian ioctl: "); hex(result); finish(2); }
    say("state="); hex(answer.state);
    say("remaining_seconds="); hex(answer.remaining_seconds);
    finish(0);
}
void _start(void) __attribute__((naked));
void _start(void) { __asm__ volatile("mov r0,sp\nbl start_c\n1:b 1b"); }

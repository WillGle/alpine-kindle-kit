/* Query the live kernel guardian; marker files cannot authorize PID1 transfer. */
#include "reset-protocol.h"
#include "reset-token.h"
static int guard_request(unsigned int command,struct gw_guard *answer)
{
    const unsigned int identity[4]=GW_RESET_TOKEN;
    int fd,result;
    memcpy(answer->token,identity,sizeof(identity));
    answer->state=0; answer->remaining_seconds=0;
    fd=sys4(5,P("/dev/gleworks_pid1_reset"),2,0,0);
    if(fd<0) return fd;
    result=sys4(54,fd,command,P(answer),0);
    sys4(6,fd,0,0,0);
    return result;
}
static int live_guard(void)
{
    struct gw_guard answer;
    return !guard_request(GW_GUARD_STATUS,&answer) && answer.state==2 && answer.remaining_seconds>=60;
}

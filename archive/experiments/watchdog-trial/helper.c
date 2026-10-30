/* GPL-2.0-only: static ARM client; final close restores stock watchdog feed. */
#include "protocol.h"
static int device = -1;
static int call3(int number, unsigned int a, unsigned int b, unsigned int c)
{
    register unsigned int r0 __asm__("r0") = a;
    register unsigned int r1 __asm__("r1") = b;
    register unsigned int r2 __asm__("r2") = c;
    register unsigned int r7 __asm__("r7") = number;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory", "cc");
    return (int)r0;
}
static void text(const char *s)
{
    unsigned int n = 0;
    while (s[n]) n++;
    while (n) {
        int wrote = call3(4, 1, (unsigned int)s, n);
        if (wrote <= 0) break;
        s += wrote; n -= wrote;
    }
}
static void finish(int status) __attribute__((noreturn));
static void finish(int status)
{
    if (device >= 0) call3(6, device, 0, 0);
    call3(1, status, 0, 0);
    for (;;) { }
}
static void expect(unsigned int command, unsigned int arg, int expected)
{
    if (call3(54, device, command, arg) != expected) {
        text("FAIL guard operation; closing lease restores feeding\n"); finish(1);
    }
}
int start_c(unsigned int *stack) __attribute__((used));
int start_c(unsigned int *stack)
{
    const char **argv = (const char **)(stack + 1);
    unsigned int last_second = ~0U;
    struct { unsigned int seconds, nanoseconds; } start, now;
    if (stack[0] != 2) { text("usage: watchdog-helper DEVICE\n"); finish(1); }
    if (call3(199, 0, 0, 0) != 0) finish(1);
    device = call3(5, (unsigned int)argv[1], 2, 0);
    if (device < 0) finish(1);
    expect(0x45584543U, 0, -25);
    expect(GW_WD_ARM, 1, -22);
    expect(GW_WD_STATE, 0, GW_WD_READY);
    /* Check callback return/state; this alone cannot prove periodic feeding. */
    expect(GW_WD_ARM, 0, 0);
    expect(GW_WD_STATE, 0, GW_WD_ARMED);
    expect(GW_WD_RESTORE, 0, 0);
    expect(GW_WD_STATE, 0, GW_WD_RESTORED);
    text("PASS immediate abort callback returned restored state; periodic feed unobserved\n");
    call3(36, 0, 0, 0);
    text("TRIAL: runnable wait avoids CPU idle; deadline restores feed while stock scheduler runs\n");
    call3(36, 0, 0, 0);
    /* Validate the monotonic clock before removing normal watchdog feeding. */
    if (call3(263, 1, (unsigned int)&start, 0) != 0) finish(1);
    expect(GW_WD_ARM, 0, 0);
    expect(GW_WD_STATE, 0, GW_WD_ARMED);
    for (;;) {
        volatile unsigned int spin;
        int observed;
        /* Stock low-bus idle calls wdg_prep(). Stay runnable, with normal
         * preemption/interrupts, rather than changing clocks or idle code. */
        for (spin = 0; spin < 262144; spin++) { }
        if (call3(263, 1, (unsigned int)&now, 0) != 0) finish(1);
        if (now.seconds - start.seconds >= GW_WD_DEADLINE + 10) break;
        if (now.seconds == last_second) continue;
        last_second = now.seconds;
        observed = call3(54, device, GW_WD_STATE, 0);
        if (observed == GW_WD_RESTORED) {
            text("NO RESET: deadline restored stock feeding; recovery remains unproven\n");
            finish(3);
        }
        if (observed != GW_WD_ARMED) finish(1);
    }
    text("ABORT recovery worker missed deadline; final close restores feeding\n");
    finish(1);
}
void _start(void) __attribute__((naked));
void _start(void)
{
    __asm__ volatile("mov r0, sp\nbl start_c\nmov r7, #1\nsvc 0");
}

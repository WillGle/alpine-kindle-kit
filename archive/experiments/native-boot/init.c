/* GPL-2.0-only: storage-free diagnostic PID1, no shell or reboot operation. */
static int sys6(int n, unsigned int a, unsigned int b, unsigned int c,
                unsigned int d, unsigned int e, unsigned int f)
{
    register unsigned int r0 __asm__("r0") = a;
    register unsigned int r1 __asm__("r1") = b;
    register unsigned int r2 __asm__("r2") = c;
    register unsigned int r3 __asm__("r3") = d;
    register unsigned int r4 __asm__("r4") = e;
    register unsigned int r5 __asm__("r5") = f;
    register unsigned int r7 __asm__("r7") = n;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r7) : "memory", "cc");
    return (int)r0;
}
#define P(x) ((unsigned int)(x))
static unsigned int length(const char *s)
{
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}
static int send(int fd, const char *s)
{
    unsigned int size = length(s);
    while (size) {
        int n = sys6(4, fd, P(s), size, 0, 0, 0);
        if (n == -4) continue;
        if (n <= 0) return -1;
        s += n; size -= n;
    }
    return 0;
}
static int mount_fs(const char *source, const char *target, const char *type)
{
    return sys6(21, P(source), P(target), P(type), 0, 0, 0);
}
struct uts { char sysname[65], nodename[65], release[65], version[65], machine[65], domain[65]; };
struct timespec32 { int seconds, nanoseconds; };

int start_init(void) __attribute__((used));
int start_init(void)
{
    struct uts info;
    struct timespec32 pause = {0, 100000000};
    char challenge[33];
    unsigned int at = 0;
    int tty = -1, ticks = 0;
    int pid = sys6(20, 0, 0, 0, 0, 0, 0);
    int mounted = !mount_fs("devtmpfs", "/dev", "devtmpfs");
    mounted = !mount_fs("proc", "/proc", "proc") && mounted;
    mounted = !mount_fs("sysfs", "/sys", "sysfs") && mounted;
    if (sys6(122, P(&info), 0, 0, 0, 0, 0)) info.release[0] = 0;
    for (;;) {
        if (tty < 0) {
            tty = sys6(5, P("/dev/ttyGS0"), 2 | 04000, 0, 0, 0, 0);
            at = 0; ticks = 0;
        }
        if (tty >= 0) {
            char in[64];
            int count, i;
            if (++ticks >= 10) {
                send(tty, "GLEWORKS_DIAGNOSTIC_READY kernel="); send(tty, info.release);
                send(tty, "\n"); ticks = 0;
            }
            count = sys6(3, tty, P(in), sizeof(in), 0, 0, 0);
            if (count > 0) for (i = 0; i < count; i++) {
                char c = in[i];
                if (c == '\n') {
                    if (at == 32 && pid == 1 && mounted) {
                        challenge[32] = 0;
                        send(tty, "GLEWORKS_NATIVE_BOOT kernel="); send(tty, info.release);
                        send(tty, " pid=1 challenge="); send(tty, challenge); send(tty, "\n");
                    }
                    at = 0;
                } else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
                    if (at < 32) challenge[at++] = c;
                    else at = 0;
                } else {
                    at = 0;
                }
            }
            /* Gadget disconnect hangs up this fd: EOF requires a fresh open. */
            if (count == 0 || (count < 0 && count != -11 && count != -4)) {
                sys6(6, tty, 0, 0, 0, 0, 0); tty = -1;
            }
        }
        sys6(162, P(&pause), 0, 0, 0, 0, 0);
    }
}
void _start(void) __attribute__((naked));
void _start(void)
{
    __asm__ volatile("bl start_init\n1: b 1b");
}

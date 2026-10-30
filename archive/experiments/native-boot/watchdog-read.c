/* GPL-2.0-only: read WDOG1/SRC state; never open /dev/watchdog or write MMIO. */
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
static void text(const char *s)
{
    unsigned int n = 0;
    while (s[n]) n++;
    sys6(4, 1, (unsigned int)s, n, 0, 0, 0);
}
static void value(const char *label, unsigned int number)
{
    char out[10]; unsigned int i;
    text(label);
    for (i = 0; i < 8; i++) out[i] = "0123456789abcdef"[(number >> (28 - i*4)) & 15];
    out[8] = '\n'; out[9] = 0; text(out);
}
int read_watchdog(void) __attribute__((used));
int read_watchdog(void)
{
    int fd, mapped;
    volatile const unsigned short *registers;
    fd = sys6(5, (unsigned int)"/dev/mem", 0, 0, 0, 0, 0);
    if (fd < 0) { value("STOP devmem_open_errno_hex=", -fd); return 1; }
    mapped = sys6(192, 0, 4096, 1, 1, fd, 0x020bc000U >> 12);
    if ((unsigned int)mapped >= (unsigned int)-4095) {
        value("STOP readonly_mmap_errno_hex=", -mapped);
        sys6(6, fd, 0, 0, 0, 0, 0); return 1;
    }
    registers = (volatile const unsigned short *)(unsigned int)mapped;
    value("WDOG1_WCR=", registers[0]);
    value("WDOG1_WRSR=", registers[2]);
    value("WDOG1_WICR=", registers[3]);
    sys6(91, mapped, 4096, 0, 0, 0, 0);
    mapped = sys6(192, 0, 4096, 1, 1, fd, 0x020d8000U >> 12);
    if ((unsigned int)mapped >= (unsigned int)-4095) {
        value("STOP readonly_src_mmap_errno_hex=", -mapped);
        sys6(6, fd, 0, 0, 0, 0, 0); return 1;
    }
    value("SRC_GPR9=", ((volatile const unsigned int *)(unsigned int)mapped)[0x40/4]);
    value("SRC_GPR10=", ((volatile const unsigned int *)(unsigned int)mapped)[0x44/4]);
    sys6(91, mapped, 4096, 0, 0, 0, 0);
    sys6(6, fd, 0, 0, 0, 0, 0);
    text("COMPLETE: read-only MMIO snapshot; watchdog device never opened\n");
    return 0;
}
void _start(void) __attribute__((naked));
void _start(void)
{
    __asm__ volatile("bl read_watchdog\nmov r7, #1\nsvc 0");
}

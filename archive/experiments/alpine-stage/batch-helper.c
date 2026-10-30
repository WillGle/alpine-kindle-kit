/* GPL-2.0-only: freestanding ARM EABI client, no libc/interpreter. */
#include "protocol.h"
static unsigned char payload[GW_LIMIT] __attribute__((aligned(4096)));
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
        int written = call3(4, 1, (unsigned int)s, n);
        if (written <= 0) break;
        s += written; n -= written;
    }
}
static void hex(unsigned int x)
{
    char out[9]; unsigned int i;
    for (i = 0; i < 8; i++) out[i] = "0123456789abcdef"[(x >> (28 - 4*i)) & 15];
    out[8] = 0; text(out);
}
static void finish(int status) __attribute__((noreturn));
static void finish(int status)
{
    if (device >= 0) call3(6, device, 0, 0);
    call3(1, status, 0, 0);
    for (;;) { }
}
static void expect(unsigned int command, unsigned int arg, int expected, const char *label)
{
    int result = call3(54, device, command, arg);
    if (result != expected) {
        text("FAIL "); text(label); text(" result=0x"); hex(result); text("\n"); finish(1);
    }
    text("PASS "); text(label); text("\n");
}
static struct gw_segment segment;
static struct gw_request request;
static unsigned int checksum(unsigned int size)
{
    unsigned int i, sum = 2166136261U, padded = (size + 4095) & ~4095U;
    for (i = 0; i < padded; i++) sum = (sum ^ (i < size ? payload[i] : 0)) * 16777619U;
    return sum;
}
static void configure(unsigned int size)
{
    segment.buf = (unsigned int)payload; segment.bufsz = size;
    segment.mem = GW_DEST; segment.memsz = (size + 4095) & ~4095U;
    request.entry = GW_DEST; request.count = 1;
    request.segments = (unsigned int)&segment; request.flags = 0;
}

int start_c(unsigned int *stack) __attribute__((used));
int start_c(unsigned int *stack)
{
    const char **argv = (const char **)(stack + 1);
    unsigned int size = 0, i, sizes[5];
    int input, result, child, status = 0;
    struct gw_status observed;
    if (stack[0] != 3) { text("usage: batch-helper DEVICE IMAGE\n"); finish(1); }
    if (call3(199, 0, 0, 0) != 0) { text("FAIL requires root\n"); finish(1); }
    input = call3(5, (unsigned int)argv[2], 0, 0);
    if (input < 0) { text("FAIL image open\n"); finish(1); }
    while (size < GW_LIMIT) {
        result = call3(3, input, (unsigned int)(payload + size), GW_LIMIT - size);
        if (result == -4) continue;
        if (result < 0) { text("FAIL image read\n"); finish(1); }
        if (!result) break;
        size += result;
    }
    call3(6, input, 0, 0);
    if (size <= 1048576 || size >= GW_LIMIT) { text("FAIL image size\n"); finish(1); }
    device = call3(5, (unsigned int)argv[1], 2, 0);
    if (device < 0) { text("FAIL device open\n"); finish(1); }
    result = call3(5, (unsigned int)argv[1], 2, 0);
    if (result != -16) { text("FAIL exclusive open\n"); finish(1); }
    text("PASS exclusive open\n");
    expect(0x45584543U, 0, -25, "execute command absent");
    expect(GW_LOAD, 0, -14, "invalid request pointer");
    expect(GW_VERIFY, (unsigned int)&observed, -2, "initial empty state");
    configure(4096);
    request.flags = 1;
    expect(GW_LOAD, (unsigned int)&request, -22, "crash/unknown flags denied");
    configure(4096); request.count = 2;
    expect(GW_LOAD, (unsigned int)&request, -22, "multiple segments denied");
    configure(4096); segment.mem++;
    expect(GW_LOAD, (unsigned int)&request, -22, "misaligned destination denied");
    configure(4096); segment.mem = 0xfffff000U; segment.memsz = 8192;
    expect(GW_LOAD, (unsigned int)&request, -22, "overflowing destination denied");
    configure(4096); segment.bufsz = GW_LIMIT + 1; segment.memsz = GW_LIMIT + 4096;
    expect(GW_LOAD, (unsigned int)&request, -22, "oversized image denied");
    configure(4096); segment.buf = 0;
    expect(GW_LOAD, (unsigned int)&request, -14, "bad payload pointer cleanup");
    expect(GW_UNLOAD, 0, 0, "zero pages after bad payload");
    for (i = 0; i < 8; i++) {
        configure(65536);
        text("FAULT budget=0x"); hex(i); text("\n");
        expect(GW_BUDGET, i, 0, "set bounded allocation failure");
        expect(GW_LOAD, (unsigned int)&request, -12, "allocation failure handled");
        expect(GW_UNLOAD, 0, 0, "zero pages after allocation failure");
    }
    expect(GW_BUDGET, 1024, 0, "restore bounded allocation budget");
    child = call3(2, 0, 0, 0);
    if (child < 0) { text("FAIL fork\n"); finish(1); }
    if (!child) {
        if (call3(213, 65534, 0, 0) || call3(54, device, GW_UNLOAD, 0) != -1) finish(1);
        finish(0);
    }
    /* ARM wait4 has a fourth argument: explicitly supply NULL rusage. */
    {
        register unsigned int r0 __asm__("r0") = child;
        register unsigned int r1 __asm__("r1") = (unsigned int)&status;
        register unsigned int r2 __asm__("r2") = 0;
        register unsigned int r3 __asm__("r3") = 0;
        register unsigned int r7 __asm__("r7") = 114;
        do {
            r0 = child;
            __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory", "cc");
        } while ((int)r0 == -4);
        if ((int)r0 != child || status) { text("FAIL CAP_SYS_BOOT enforcement\n"); finish(1); }
    }
    text("PASS CAP_SYS_BOOT denied on inherited fd after dropping root\n");
    sizes[0] = 4096; sizes[1] = 65536; sizes[2] = 262144; sizes[3] = 1048576; sizes[4] = size;
    for (i = 0; i < 5; i++) {
        configure(sizes[i]);
        expect(GW_LOAD, (unsigned int)&request, 0, "stage image");
        expect(GW_LOAD, (unsigned int)&request, -16, "second load denied");
        expect(GW_VERIFY, (unsigned int)&observed, 0, "read back staging pages");
        if (observed.bytes != sizes[i] || observed.checksum != checksum(sizes[i]) ||
            !observed.owned_pages || observed.peak_pages > 1024) {
            text("FAIL staged data or accounting\n"); finish(1);
        }
        text("DATA bytes=0x"); hex(observed.bytes); text(" fnv1a=0x"); hex(observed.checksum);
        text(" owned=0x"); hex(observed.owned_pages); text(" peak=0x"); hex(observed.peak_pages); text("\n");
        expect(GW_UNLOAD, 0, 0, "release image and assert zero owned pages");
        expect(GW_VERIFY, (unsigned int)&observed, -2, "empty after unload");
    }
    expect(GW_PREPARE, 0, -2, "prepare rejects empty image");
    configure(4096);
    expect(GW_LOAD, (unsigned int)&request, 0, "stage wrong-size image");
    expect(GW_PREPARE, 0, -22, "prepare rejects wrong size");
    expect(GW_UNLOAD, 0, 0, "wrong-size cleanup");
    configure(size);
    expect(GW_LOAD, (unsigned int)&request, 0, "stage fixed diagnostic image");
    expect(GW_PREPARE, 1, -22, "prepare rejects unknown argument");
    expect(GW_PREPARE, 0, 0, "owned chain fixed SHA256 control page prepared");
    expect(GW_PREPARE, 0, 0, "repeat preparation remains returning");
    expect(GW_VERIFY, (unsigned int)&observed, 0, "verify after preparation");
    if (observed.checksum != checksum(size)) { text("FAIL changed image after preparation\n"); finish(1); }
    expect(GW_UNLOAD, 0, 0, "prepared image cleanup");
    payload[0] ^= 1;
    expect(GW_LOAD, (unsigned int)&request, 0, "stage corrupted image");
    expect(GW_PREPARE, 0, -22, "prepare rejects changed byte via SHA256");
    expect(GW_UNLOAD, 0, 0, "corrupted image cleanup");
    payload[0] ^= 1;
    configure(4096);
    expect(GW_LOAD, (unsigned int)&request, 0, "stage close-cleanup case");
    if (call3(6, device, 0, 0)) { text("FAIL device close\n"); finish(1); }
    device = call3(5, (unsigned int)argv[1], 2, 0);
    if (device < 0) { text("FAIL reopen\n"); finish(1); }
    expect(GW_VERIFY, (unsigned int)&observed, -2, "close automatically released image");
    expect(GW_UNLOAD, 0, 0, "zero owned pages after close");
    text("COMPLETE: all batch checks passed; no kernel execution requested\n");
    finish(0);
}
void _start(void) __attribute__((naked));
void _start(void)
{
    __asm__ volatile("mov r0, sp\nbl start_c\nmov r7, #1\nsvc 0");
}

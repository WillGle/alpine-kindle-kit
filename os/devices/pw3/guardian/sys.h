/* GPL-2.0-only: ARM EABI freestanding userspace, no kernel/module patch. */
#define P(x) ((unsigned int)(x))
void *memcpy(void *to,const void *from,unsigned int n)
{
    unsigned char *d=to; const unsigned char *s=from;
    while(n--) *d++=*s++;
    return to;
}
static int sys4(int n, unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
    register unsigned int r0 __asm__("r0") = a;
    register unsigned int r1 __asm__("r1") = b;
    register unsigned int r2 __asm__("r2") = c;
    register unsigned int r3 __asm__("r3") = d;
    register unsigned int r7 __asm__("r7") = n;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory", "cc");
    return (int)r0;
}
static unsigned int length(const char *s) { unsigned int n=0; while(s[n]) n++; return n; }
static int same(const char *a, const char *b) { while(*a && *a==*b) { a++; b++; } return *a==*b; }
static void say(const char *s) { unsigned int n=length(s); while(n) { int r=sys4(4,1,P(s),n,0); if(r==-4) continue; if(r<=0) return; s+=r; n-=r; } }
static void hex(unsigned int v) { char b[10]; unsigned int i; for(i=0;i<8;i++) b[i]="0123456789abcdef"[(v>>(28-4*i))&15]; b[8]='\n'; b[9]=0; say(b); }
static void finish(int v) __attribute__((noreturn));
static void finish(int v) { sys4(1,v,0,0,0); for(;;) {} }
static void pause_ms(unsigned int ms) { unsigned int t[2]={ms/1000,(ms%1000)*1000000}; sys4(162,P(t),0,0,0); }
static int touch(const char *p) { int fd=sys4(5,P(p),1|64|512,0600,0); if(fd<0) return fd; return sys4(6,fd,0,0,0); }
static int exists(const char *p) { return sys4(33,P(p),0,0,0)==0; }
static int monotonic(void) { unsigned int t[2]; if(sys4(263,1,P(t),0,0)) return -1; return (int)t[0]; }

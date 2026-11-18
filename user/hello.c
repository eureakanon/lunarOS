/* user/hello.c */
#define SYS_EXIT  1
#define SYS_WRITE 4

static inline int syscall3(int n, int a, int b, int c)
{
    int r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(n), "b"(a), "c"(b), "d"(c));
    return r;
}

static void u_puts(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    syscall3(SYS_WRITE, 1, (int)s, n);
}

void _start(void)
{
    u_puts("Hello from user ELF!\n");
    u_puts("I'm running at Ring 3 with my own address space.\n");
    syscall3(SYS_EXIT, 0, 0, 0);
    for (;;);
}

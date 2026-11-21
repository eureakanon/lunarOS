#include "types.h"
#include "output.h"
#include "proc.h"
#include "fs.h"
#include "elf.h"
#include "paging.h"
#include "pmm.h"
#include "keyboard.h"
#include "fifo.h"
#include "desctbl.h"
#include "ioctl.h"
#include "klibc.h"
extern struct FIFO8 keyfifo;

static char g_exec_filename[128];

static int str_eq(const char *a, const char *b)
{
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

/* 键盘扫描码表（只处理可打印 + 回车 + 退格） */
static const char keymap[128] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,' ',
};

/* 从键盘读一行（阻塞） */
static int read_line(char *buf, int max)
{
    int n = 0;
    for (;;) {
        int c = fifo8_get(&keyfifo);
        if (c < 0) {
            __asm__ volatile ("hlt");   /* 等键盘中断唤醒 */
            continue;
        }
        if (c == 0x1C) {   /* Enter */
            buf[n] = 0;
            putchar('\n');
            return n;
        }
        if (c == 0x0E && n > 0) {   /* Backspace */
            n--;
            putchar('\b'); putchar(' '); putchar('\b');
            continue;
        }
        if (c < 128 && keymap[c] && n < max - 1) {
            buf[n++] = keymap[c];
            putchar(keymap[c]);
        }
    }
}

/* exec_child_entry：在子进程内核上下文里执行，负责加载 ELF 并 iret 到 Ring 3 */
static void exec_child_entry(void)
{
    struct proc *p = myproc();

    int fd = fs_open(g_exec_filename);
    if (fd < 0) {
        puts("[exec] open failed: "); puts(g_exec_filename); putchar('\n');
        exit(1);
    }

    static uint8 elf_buf[64 * 1024];
    int n = fs_read(fd, elf_buf, sizeof(elf_buf));
    fs_close(fd);
    if (n <= 0) { puts("[exec] read failed\n"); exit(1); }

    uint32 *new_pgd = paging_new_proc_pgd();
    if (!new_pgd) exit(1);

    uint32 entry;
    if (elf_load_image(new_pgd, elf_buf, (uint32)n, &entry) < 0) {
        puts("[exec] elf_load failed\n");
        free_addr_space(new_pgd);
        exit(1);
    }

    void *stack_phys = pmm_alloc_page();
    if (!stack_phys) { free_addr_space(new_pgd); exit(1); }
    memset(stack_phys, 0, PAGE_SIZE);
    if (paging_map_user(new_pgd, USER_STACK_TOP - PAGE_SIZE,
                        (uint32)stack_phys) < 0) {
        pmm_free_page(stack_phys);
        free_addr_space(new_pgd);
        exit(1);
    }

    uint32 *old_pgd = p->pgd;
    p->pgd = new_pgd;
    paging_switch(new_pgd);
    free_addr_space(old_pgd);

    puts("[exec] entering user mode @0x"); put_hex(entry); putchar('\n');
    enter_user_mode(entry, USER_STACK_TOP);

    /* 不会到这里 */
    for (;;) __asm__ volatile ("hlt");
}

void shell_main(void)
{
    char line[128];

    puts("\n[shell] ready. Type 'help'.\n");

    for (;;) {
        puts("$ ");
        int n = read_line(line, sizeof(line));
        if (n == 0) continue;

        /* 拆分 cmd / arg */
        char *cmd = line;
        char *arg = 0;
        for (int i = 0; line[i]; i++) {
            if (line[i] == ' ') { line[i] = 0; arg = &line[i + 1]; break; }
        }

        if (str_eq(cmd, "help")) {
            puts("  ls           list files\n");
            puts("  cat FILE     show file\n");
            puts("  echo TEXT    print text\n");
            puts("  run FILE     exec ELF file\n");
            puts("  ps           list processes\n");
            puts("  exit         quit shell\n");
        }
        else if (str_eq(cmd, "ls")) {
            fs_list();
        }
        else if (str_eq(cmd, "cat") && arg) {
            int fd = fs_open(arg);
            if (fd < 0) { puts("no such file\n"); continue; }
            char buf[256];
            int r;
            while ((r = fs_read(fd, buf, sizeof(buf))) > 0) {
                for (int i = 0; i < r; i++) putchar(buf[i]);
            }
            putchar('\n');
            fs_close(fd);
        }
        else if (str_eq(cmd, "echo") && arg) {
            puts(arg); putchar('\n');
        }
        else if (str_eq(cmd, "ps")) {
            ps_list();
        }
        else if (str_eq(cmd, "exit")) {
            exit(0);
        }
        else if (str_eq(cmd, "run") && arg) {
            int i;
            for (i = 0; arg[i] && i < 127; i++) g_exec_filename[i] = arg[i];
            g_exec_filename[i] = 0;

            int pid = kernel_fork(exec_child_entry);
            if (pid < 0) { puts("fork failed\n"); continue; }

            puts("[shell] child pid="); put_dec(pid); puts(" started\n");
            int exited = do_wait();
            puts("[shell] pid="); put_dec(exited); puts(" exited\n");
        }
        else {
            puts("unknown: "); puts(cmd); putchar('\n');
        }
    }
}

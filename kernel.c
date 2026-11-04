#include "types.h"
#include "output.h"
#include "desctbl.h"
#include "int.h"
#include "ioctl.h"
#include "keyboard.h"
#include "timer.h"
#include "tss.h"
#include "pmm.h"
#include "paging.h"
#include "proc.h"

extern char __bss_start[], __bss_end[], _kernel_end[];
extern char user_prog_start[], user_prog_end[];

#define PHYS_END   0x02000000

/* 建立一个新的用户进程的地址空间并加载用户程序 */
static uint32 *setup_user_mm(void)
{
    uint32 *pgd = paging_new_proc_pgd();
    if (!pgd) return 0;

    void *code_phys  = pmm_alloc_page();
    void *stack_phys = pmm_alloc_page();
    if (!code_phys || !stack_phys) return 0;

    if (paging_map_user(pgd, USER_CODE_VADDR, (uint32)code_phys) < 0) return 0;
    if (paging_map_user(pgd, USER_STACK_TOP - PAGE_SIZE,
                        (uint32)stack_phys) < 0) return 0;

    uint32 size = (uint32)(user_prog_end - user_prog_start);
    if (size > PAGE_SIZE) size = PAGE_SIZE;
    for (uint32 i = 0; i < size; i++)
        ((char *)code_phys)[i] = user_prog_start[i];

    return pgd;
}

static void user_entry(void)
{
    struct proc *p = myproc();
    /* 保留旧的 pid 写栈逻辑，虽然新程序不用它，但保持兼容 */
    *(uint32 *)(USER_STACK_TOP - 4) = (uint32)p->pid;
    enter_user_mode(USER_CODE_VADDR, USER_STACK_TOP);
    for (;;) __asm__ volatile ("hlt");
}

__attribute__((section(".text.start")))
void kernel_main(void)
{
    for (char *p = __bss_start; p < __bss_end; p++) *p = 0;
    __asm__ volatile ("movl $0x0007FFFC, %esp");

    clear_screen();
    puts("Simple OS v0.8 - fork & wait\n");
    puts("============================\n\n");

    init_gdtidt();
    init_pic();
    init_keyboard();
    init_timer();
    pmm_init((uint32)_kernel_end, PHYS_END);
    paging_init();

    /* 演示期间关掉定时器抢占，方便观察 fork/wait 的语义 */
    io_out8(0x21, 0xff);
    io_out8(0xA1, 0xff);
    io_sti();

    /* ---- 创建两个初始用户进程 ---- */
    uint32 *pgd_a = setup_user_mm();
    uint32 *pgd_b = setup_user_mm();
    if (!pgd_a || !pgd_b) {
        puts("setup failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    proc_init();
    struct proc *pa = proc_alloc();
    struct proc *pb = proc_alloc();

    /* ★ 手动设置初始 pid，保证用户程序看到 0 和 1 */
    pa->pid = 0;
    pb->pid = 1;
    next_pid = 2;

    proc_start(pa, user_entry, pgd_a);
    proc_start(pb, user_entry, pgd_b);

    puts("Starting scheduler...\n\n");
    scheduler();
    for (;;) __asm__ volatile ("hlt");
}

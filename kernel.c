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
#include "sem.h"

extern char __bss_start[], __bss_end[];
extern char _kernel_end[];

#define PHYS_END   0x02000000

/* ★ 两个进程共用的虚拟地址，但物理地址不同 */
#define UVADDR_DATA  0x40000000

static struct semaphore print_lock;

/* 给进程创建一个地址空间：pgd + 一个私有数据页 */
static uint32 *setup_proc_mm(void)
{
    uint32 *pgd = paging_new_proc_pgd();
    if (!pgd) return 0;

    void *phys = pmm_alloc_page();
    if (!phys) return 0;

    if (paging_map_user(pgd, UVADDR_DATA, (uint32)phys) < 0)
        return 0;

    /* 清零私有页 */
    *(uint32 *)phys = 0;
    return pgd;
}

/* ---------------- 进程 A ---------------- */
static void proc_a(void)
{
    uint32 *data = (uint32 *)UVADDR_DATA;

    *data = 0xAAAAAAAA;
    yield();            /* 让 B 有机会跑，看它会不会覆盖我们的值 */
    yield();
    yield();

    sem_wait(&print_lock);
    puts("[A] vaddr 0x"); put_hex(UVADDR_DATA);
    puts(" = 0x");      put_hex(*data);
    puts("  (expect 0xAAAAAAAA)\n");
    sem_signal(&print_lock);

    exit(0);
}

/* ---------------- 进程 B ---------------- */
static void proc_b(void)
{
    uint32 *data = (uint32 *)UVADDR_DATA;

    *data = 0xBBBBBBBB;
    yield();
    yield();
    yield();

    sem_wait(&print_lock);
    puts("[B] vaddr 0x"); put_hex(UVADDR_DATA);
    puts(" = 0x");      put_hex(*data);
    puts("  (expect 0xBBBBBBBB)\n");
    sem_signal(&print_lock);

    exit(0);
}

__attribute__((section(".text.start")))
void kernel_main(void)
{
    for (char *p = __bss_start; p < __bss_end; p++) *p = 0;
    __asm__ volatile ("movl $0x0007FFFC, %esp");

    clear_screen();
    puts("Simple OS v0.6 - per-proc address space\n");
    puts("=======================================\n\n");

    init_gdtidt();
    init_pic();
    init_keyboard();
    init_timer();

    pmm_init((uint32)_kernel_end, PHYS_END);
    paging_init();

    /* 演示期间关中断，避免定时器抢占干扰协作式切换 */
    io_out8(0x21, 0xff);
    io_out8(0xA1, 0xff);
    io_sti();

    /* ---- 为两个进程各自建立独立地址空间 ---- */
    uint32 *pgd_a = setup_proc_mm();
    uint32 *pgd_b = setup_proc_mm();
    if (!pgd_a || !pgd_b) {
        puts("setup_proc_mm failed!\n");
        for (;;) __asm__ volatile ("hlt");
    }
    puts("pgd_a = 0x"); put_hex((uint32)pgd_a);
    puts("  pgd_b = 0x"); put_hex((uint32)pgd_b); putchar('\n');

    sem_init(&print_lock, 1);
    proc_init();

    struct proc *pa = proc_alloc();
    struct proc *pb = proc_alloc();
    proc_start(pa, proc_a, pgd_a);
    proc_start(pb, proc_b, pgd_b);

    puts("\nStarting scheduler...\n\n");
    scheduler();
    for (;;) __asm__ volatile ("hlt");
}

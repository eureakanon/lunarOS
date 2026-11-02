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

extern char __bss_start[], __bss_end[];
extern char _kernel_end[];              /* ← 来自 kernel.ld */
extern char user_prog_start[], user_prog_end[];

#define PHYS_END         0x02000000     /* 假定 32MB 物理内存 */
#define USER_PROG_ADDR   0x00400000
#define USER_STACK_TOP   0x00800000
#define KERNEL_ESP0      0x00060000

__attribute__((section(".text.start")))
void kernel_main(void)
{
    for (char *p = __bss_start; p < __bss_end; p++) *p = 0;
    __asm__ volatile ("movl $0x0007FFFC, %esp");

    clear_screen();
    puts("Simple OS v0.5 - paging\n");
    puts("=======================\n\n");

    /* ---- 先建中断和 GDT，因为后面出异常需要它们 ---- */
    init_gdtidt();
    init_pic();
    init_keyboard();
    init_timer();

    /* ---- 物理内存管理 ---- */
    pmm_init((uint32)_kernel_end, PHYS_END);

    /* ---- 分页（这一步之后就进入虚拟地址世界了） ---- */
    paging_init();

    /* ---- 演示期间关闭中断，避免干扰 ---- */
    io_out8(0x21, 0xff);
    io_out8(0xA1, 0xff);
    io_sti();

    /* ---- 拷贝用户程序 ---- */
    uint32 size = (uint32)(user_prog_end - user_prog_start);
    for (uint32 i = 0; i < size; i++)
        *(char *)(USER_PROG_ADDR + i) = user_prog_start[i];

    puts("User program @0x");
    put_hex(USER_PROG_ADDR);
    puts(" size=");
    put_dec((int)size);
    puts(" bytes\n");

    tss_set_kernel_stack(KERNEL_ESP0);

    puts("\nEntering user mode...\n\n");

    enter_user_mode(USER_PROG_ADDR, USER_STACK_TOP);

    for (;;) __asm__ volatile ("hlt");
}

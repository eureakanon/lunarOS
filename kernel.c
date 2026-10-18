#include "types.h"
#include "output.h"
#include "desctbl.h"
#include "int.h"
#include "ioctl.h"
#include "keyboard.h"
#include "timer.h"
#include "tss.h"

extern char __bss_start[], __bss_end[];
extern char user_prog_start[], user_prog_end[];

#define USER_PROG_ADDR   0x00400000     /* 用户程序加载地址 */
#define USER_STACK_TOP   0x00800000     /* 用户栈顶（4KB 空间） */
#define KERNEL_ESP0      0x00060000     /* 用户态进内核时用的栈（与主内核栈 0x7FFFC 分开） */

__attribute__((section(".text.start")))
void kernel_main(void)
{
    for (char *p = __bss_start; p < __bss_end; p++) *p = 0;

    __asm__ volatile ("movl $0x0007FFFC, %esp");

    clear_screen();
    puts("Simple OS v0.4 - user mode\n");
    puts("==========================\n\n");

    init_gdtidt();          
    init_pic();
    init_keyboard();
    init_timer();

    /* 演示期间屏蔽所有 IRQ（避免定时器抢占干扰演示） */
    io_out8(0x21, 0xff);
    io_out8(0xA1, 0xff);
    /* 如果想保留键盘演示，改成 io_out8(0x21, 0xfd) */

    io_sti();

    /* ---- 拷贝用户程序到 0x00400000 ---- */
    uint size = (uint)(user_prog_end - user_prog_start);
    for (uint i = 0; i < size; i++)
        *(char *)(USER_PROG_ADDR + i) = user_prog_start[i];

    puts("User program loaded at 0x");
    put_hex(USER_PROG_ADDR);
    puts(", size=");
    put_dec((int)size);
    puts(" bytes\n");

    /* ---- 设置 TSS 的内核栈 ---- */
    tss_set_kernel_stack(KERNEL_ESP0);

    puts("\nEntering user mode...\n\n");

    /* ---- 跳入 Ring 3（不返回） ---- */
    enter_user_mode(USER_PROG_ADDR, USER_STACK_TOP);

    /* 不会到这里 */
    for (;;) __asm__ volatile ("hlt");
}

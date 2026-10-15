#include "types.h"
#include "output.h"
#include "desctbl.h"
#include "int.h"
#include "ioctl.h"
#include "keyboard.h"
#include "fifo.h"
#include "timer.h"

extern char __bss_start[];
extern char __bss_end[];

__attribute__((section(".text.start")))
void kernel_main(void)
{
    /* 清 BSS */
    for (char *p = __bss_start; p < __bss_end; p++) *p = 0;

    /* 建立内核栈（放在低地址，远离 0x00280000 的 kernel） */
    __asm__ volatile ("movl $0x0007FFFC, %esp");

    clear_screen();
    puts("Simple OS v0.3 - trapframe\n");
    puts("==========================\n\n");

    init_gdtidt();          /* 建立 GDT + 装满 256 项 IDT */
    init_pic();             /* PIC 重映射 + 打开 IRQ1 */
    init_timer();           /* 初始化计时器 */
    init_keyboard();        /* 键盘控制器 */

    io_sti();               /* ★ 开中断 ★ */

    puts("Kernel ready. Press any key...\n");
    puts("(key scan codes will be printed as [xx])\n\n");

    for (;;) {
        static uint32 last=0;
        if(timer_ticks-last>=10)
        {
            last=timer_ticks;
            puts("[tick ");
            put_dec(timer_ticks);
            puts("]\n");
        }
        __asm__ volatile ("hlt");   /* 睡到下一个中断 */
    }
}

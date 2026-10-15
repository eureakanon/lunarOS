#include "trapframe.h"
#include "ioctl.h"
#include "int.h"
#include "output.h"
#include "timer.h"

/* 来自 keyboard.c */
extern void keyboard_handler(void);

static const char *exception_names[] = {
    "Divide Error",                  /* 0 */
    "Debug",                         /* 1 */
    "NMI Interrupt",                 /* 2 */
    "Breakpoint",                    /* 3 */
    "Overflow",                      /* 4 */
    "BOUND Range Exceeded",          /* 5 */
    "Invalid Opcode",                /* 6 */
    "Device Not Available",          /* 7 */
    "Double Fault",                  /* 8 */
    "Coprocessor Segment Overrun",   /* 9 */
    "Invalid TSS",                   /* 10 */
    "Segment Not Present",           /* 11 */
    "Stack-Segment Fault",           /* 12 */
    "General Protection Fault",      /* 13 */
    "Page Fault",                    /* 14 */
    "Reserved",                      /* 15 */
    "x87 FPU Error",                 /* 16 */
    "Alignment Check",               /* 17 */
    "Machine Check",                 /* 18 */
    "SIMD FP Exception",             /* 19 */
};

void trap(struct trapframe *tf)
{
    /* ---- 1. CPU 异常（0~19）：打印现场并停机 ---- */
    if (tf->trapno < 20) {
        puts("\n\n*** EXCEPTION: ");
        puts(exception_names[tf->trapno]);
        puts(" (");
        put_dec(tf->trapno);
        puts(")\n  errcode = ");
        put_hex(tf->errcode);
        puts("\n  eip     = ");
        put_hex(tf->eip);
        puts("\n  cs      = ");
        put_hex(tf->cs);
        puts("\n  eflags  = ");
        put_hex(tf->eflags);
        puts("\n*** SYSTEM HALTED ***\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    /* ---- 2. IRQ（0x20~0x2F） ---- */
    if (tf->trapno >= 0x20 && tf->trapno < 0x30) {
        switch (tf->trapno) {
        case 0x20:      /* 定时器（暂不处理） */
            timer_handler();
            break;
        case 0x21:      /* 键盘 */
            keyboard_handler();
            break;
        default:
            break;
        }

        /* 发 EOI */
        if (tf->trapno >= 0x28) {
            io_out8(PIC1_OCW2, 0x20);   /* 从 PIC */
        }
        io_out8(PIC0_OCW2, 0x20);       /* 主 PIC */
        return;
    }

    /* ---- 3. 未注册的中断 ---- */
    puts("\nUnhandled trap: ");
    put_dec(tf->trapno);
    putchar('\n');
}

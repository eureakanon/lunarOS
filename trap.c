#include "trapframe.h"
#include "ioctl.h"
#include "int.h"
#include "output.h"
#include "timer.h"
#include "proc.h"
#include "paging.h"
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
static void syscall_handler(struct trapframe* tf)
{
    if(tf->eax==1)/*sys_exit*/
    {
        exit((int)tf->ebx);
    }
    else if(tf->eax==2)/*sys_fork*/
    {
        tf->eax=(uint)do_fork(tf);
    }
    else if(tf->eax==3)
    {
        tf->eax=(uint)do_wait();
    }
    else if(tf->eax==4)/*sys_write(fd,buf,len)*/
    {
        int fd = (int)tf->ebx;
        const char *buf = (const char *)tf->ecx;
        int len = (int)tf->edx;
        if (fd == 1 || fd == 2) {
            for (int i = 0; i < len; i++) putchar(buf[i]);
            tf->eax = (uint)len;
        } else {
            tf->eax = (uint)-1;
        }
    }
    else if(tf->eax==5)/*sys_yield*/
    {
        yield();
        tf->eax=0;
    }
    else if(tf->eax==6)
    {
        if (do_exec((const char *)tf->ebx, tf) < 0)
        {
            tf->eax = (uint)-1;
        }
    }
    else
    {
        puts("\nUnknown syscall: ");
        put_dec((int)tf->eax);
        putchar('\n');
        tf->eax = (uint)-1;
    }
}
void trap(struct trapframe *tf)
{
    /* ---- 1. CPU 异常（0~19）：打印现场并停机 ---- */
    if (tf->trapno == 14)
    {
        uint32 cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

        /* COW 缺页：P=1, W=1, U=1 */
        if ((tf->errcode & 0x7) == 0x7)
        {
            struct proc *p = myproc();
            if (p && p->pgd && paging_copy_on_write(p->pgd, cr2) == 0)
            {
                return;
            }
        }

        puts("\n\n*** PAGE FAULT ***\n");
        puts("  eip = "); put_hex(tf->eip);
        puts("\n  cr2 = "); put_hex(cr2);
        puts("\n  err = "); put_hex(tf->errcode);
        puts("  (P="); put_dec(tf->errcode & 1);
        puts(" W=");   put_dec((tf->errcode >> 1) & 1);
        puts(" U=");   put_dec((tf->errcode >> 2) & 1);
        puts(")\n");
        puts("*** HALTED ***\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }/*PAGE FAULT*/
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
    if(tf->trapno==0x80)
    {
        syscall_handler(tf);
        return;
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
        if (need_resched && (tf->cs & 3)) {
            need_resched = 0;/*要求调度*/
            yield();/*放弃cpu*/
        }
        return;
    }

    /* ---- 3. 未注册的中断 ---- */
    puts("\nUnhandled trap: ");
    put_dec(tf->trapno);
    putchar('\n');
}

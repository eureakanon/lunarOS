#ifndef _TSS_H
#define _TSS_H

#include "types.h"

struct tss_entry {
    uint   prev_tss;    /* 上一任务（不用） */
    uint   esp0;        /* ★ Ring 0 栈指针（用户态进入内核时用） */
    uint   ss0;         /* ★ Ring 0 段选择子（0x08） */
    uint   esp1, ss1;
    uint   esp2, ss2;
    uint   cr3;
    uint   eip, eflags;
    uint   eax, ecx, edx, ebx;
    uint   esp, ebp, esi, edi;
    uint   es, cs, ss, ds, fs, gs;
    uint   ldt;
    ushort trap;
    ushort iomap_base;
} __attribute__((packed));

extern struct tss_entry tss;

void tss_init(void);
void tss_set_kernel_stack(uint esp0);

#endif

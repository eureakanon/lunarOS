#ifndef _TRAPFRAME_H
#define _TRAPFRAME_H
#include"types.h"

struct trapframe
{
    uint gs;
    uint fs;
    uint es;
    uint ds;

    uint edi;
    uint esi;
    uint ebp;
    uint esp_dummy;     /* pusha 压入的 esp 原值，我们不用 */
    uint ebx;
    uint edx;
    uint ecx;
    uint eax;

    uint trapno;/*中断号*/
    uint errcode;/*错误码*/

    uint eip;/*CPU处理时自动压入*/
    uint cs;
    uint eflags;

    uint useresp;/*只有ring3->ring0时压入*/
    uint ss;
};

void trap(struct trapframe* tf);
#endif

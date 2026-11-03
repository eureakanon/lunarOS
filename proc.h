#ifndef PROC_H
#define PROC_H

#include "types.h"
#include "paging.h"

#define NPROC        16
#define KSTACK_SIZE  4096

enum procstate {
    UNUSED,
    EMBRYO,
    RUNNABLE,
    RUNNING,
    SLEEPING,
    ZOMBIE,
};

/* 与 xv6 一致：只保存 callee-saved 寄存器 + eip */
struct context {
    uint edi;
    uint esi;
    uint ebx;
    uint ebp;
    uint eip;
};

struct proc {
    enum procstate state;
    int pid;
    char *kstack;               /* 内核栈底 */
    struct context *context;    /* 保存的上下文（在栈顶） */
    void *chan;                 /* 睡眠通道（如信号量地址） */
    uint32* pgd;                /* 独立页表 */
};

void          proc_init(void);
struct proc  *proc_alloc(void);
void          proc_start(struct proc *p, void (*entry)(void),uint32* pgd);
struct proc  *myproc(void);
void          yield(void);
void          exit(int status) __attribute__((noreturn));
void          sleep(void *chan);
void          wakeup(void *chan);
void          scheduler(void) __attribute__((noreturn));

/* 汇编实现 */
extern void swtch(struct context **old, struct context *new);

#endif

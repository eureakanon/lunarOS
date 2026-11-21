#ifndef PROC_H
#define PROC_H

#include "types.h"
#include "paging.h"
#include "trapframe.h"
#include "fs.h"
#define NPROC        16
#define KSTACK_SIZE  4096

extern int next_pid;
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
    struct proc* parent;        /* 父进程 */
    int xstate;                 /* 退出状态码 */
    struct ofile* ofile[NOFILE];
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

int do_fork(struct trapframe* tf);
int do_wait(void);
int do_exec(const char* filename,struct trapframe* tf);

int kernel_fork(void (*func)(void));

void ps_list(void);

/* 汇编实现 */
extern void swtch(struct context **old, struct context *new);
extern void resume_trapframe(void);

struct proc  *proc_next_zombie_child(struct proc *parent);
#endif

#include"proc.h"
#include"output.h"
#include"tss.h"
int next_pid=0;
static struct proc procs[NPROC];/*全局进程表*/
static char kstacks[NPROC][KSTACK_SIZE];/*全局栈表*/
static struct proc* current;/*当前进程*/
static struct context* scheduler_ctx;/*调度上下文*/
void proc_init(void)
{
    for (int i = 0; i < NPROC; i++) {
        procs[i].state   = UNUSED;
        procs[i].pid     = i;
        procs[i].kstack  = kstacks[i];
        procs[i].context = 0;
        procs[i].chan    = 0;
        procs[i].pgd     = 0;
    }
    current = 0;
    scheduler_ctx = 0;
}

struct proc* proc_alloc()
{
    for(int i=0;i<NPROC;i++)
    {
        if(procs[i].state==UNUSED)
        {
            procs[i].state=EMBRYO;
            return &procs[i];
        }
    }
    return 0;
}

void proc_start(struct proc* p,void(*entry)(void),uint32* pgd)
{
    uint *top = (uint *)((uint)(p->kstack + KSTACK_SIZE) & ~0xF);
    uint *sp=top-5;
    sp[0]=0;
    sp[1]=0;
    sp[2]=0;
    sp[3]=0;
    sp[4]=(uint)(entry);

    p->context=(struct context*)(sp);
    p->pgd=pgd;
    p->state=RUNNABLE;
}

struct proc* myproc()
{
    return current;
}

static void sched()
{
    struct proc* p=myproc();
    swtch(&p->context,scheduler_ctx);
}

void yield(void)
{
    struct proc *p = myproc();
    if (!p) return;
    p->state = RUNNABLE;
    sched();
}

void exit(int status)
{
    struct proc* p=myproc();
    if (!p) for (;;) __asm__ volatile ("hlt");

    p->xstate=status;
    p->state=ZOMBIE;

    if(p->parent)
    {
        wakeup(p->parent);
    }
    sched();
    for (;;) __asm__ volatile ("hlt");
}

void sleep(void *chan)
{
    struct proc *p = myproc();
    if (!p) return;
    p->chan  = chan;
    p->state = SLEEPING;
    sched();
    p->chan  = 0;
}

void wakeup(void *chan)
{
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &procs[i];
        if (p->state == SLEEPING && p->chan == chan)
            p->state = RUNNABLE;
    }
}

static int any_alive(void)
{
    for (int i = 0; i < NPROC; i++) {
        enum procstate s = procs[i].state;
        if (s == RUNNABLE || s == RUNNING || s == SLEEPING)
            return 1;
    }
    return 0;
}

void scheduler(void)
{
    for (;;) {
        int any = 0;
        for (int i = 0; i < NPROC; i++) {
            struct proc *p = &procs[i];
            if (p->state == RUNNABLE) {
                any = 1;
                current = p;
                p->state = RUNNING;
                if(p->pgd)
                {
                    paging_switch(p->pgd);
                }
                tss_set_kernel_stack((uint)(p->kstack + KSTACK_SIZE));
                swtch(&scheduler_ctx, p->context);
                current = 0;
            }
        }
        if (!any) {
            if (!any_alive())
                puts("\n[scheduler] all processes exited.\n");
            else
                puts("\n[scheduler] deadlock: no runnable process.\n");
            for (;;) __asm__ volatile ("hlt");
        }
    }
}

int do_fork(struct trapframe *tf)
{
    struct proc *parent = myproc();
    struct proc *child  = proc_alloc();
    if (!child) return -1;

    /* 1. 复制地址空间 */
    uint32 *child_pgd = copy_addr_space(parent->pgd);
    if (!child_pgd) { child->state = UNUSED; return -1; }

    /* 2. 复制内核栈（含 trapframe 及更低地址的栈内容） */
    for (int i = 0; i < KSTACK_SIZE; i++)
        child->kstack[i] = parent->kstack[i];

    /* 3. 定位子进程 kstack 上的 trapframe */
    uint32 tf_off = (uint32)tf - (uint32)parent->kstack;
    struct trapframe *ctf = (struct trapframe *)(child->kstack + tf_off);

    /* 4. 子进程 fork 返回 0，父进程返回 child->pid */
    ctf->eax = 0;

    /* 5. 在 trapframe 前面构造子进程 context：
     *     swtch 恢复 edi/esi/ebx/ebp 后 ret 到 resume_trapframe，
     *     此时 esp 正好指向 ctf（trapframe 起点）。 */
    uint32* sp=(uint32*) ((char*)ctf - 24);
    sp[0] = 0;                          /* edi */
    sp[1] = 0;                          /* esi */
    sp[2] = 0;                          /* ebx */
    sp[3] = 0;                          /* ebp */
    sp[4] = (uint32)resume_trapframe;   /* eip */
    sp[5] = (uint32)ctf;
    child->context = (struct context *)sp;

    child->pgd    = child_pgd;
    child->parent = parent;
    child->pid    = next_pid++;
    child->state  = RUNNABLE;

    return child->pid;
}

int do_wait(void)
{
    struct proc *p = myproc();
    for (;;) {
        int have_child = 0;
        for (int i = 0; i < NPROC; i++) {
            struct proc *c = &procs[i];
            if (c->parent != p) continue;
            have_child = 1;
            if (c->state == ZOMBIE) {
                /* 回收子进程 */
                int pid = c->pid;
                if (c->pgd) free_addr_space(c->pgd);
                c->pgd    = 0;
                c->parent = 0;
                c->state  = UNUSED;
                return pid;
            }
        }
        if (!have_child) return -1;
        sleep(p);               /* 被子进程 exit 时 wakeup */
    }
}

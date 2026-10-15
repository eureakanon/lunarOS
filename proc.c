#include"proc.h"
#include"output.h"

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

void proc_start(struct proc* p,void(*entry)(void))
{
    uint *top = (uint *)((uint)(p->kstack + KSTACK_SIZE) & ~0xF);
    uint *sp=top-5;
    sp[0]=0;
    sp[1]=0;
    sp[2]=0;
    sp[3]=0;
    sp[4]=(uint)(entry);

    p->context=(struct context*)(sp);
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
    (void)status;
    struct proc *p = myproc();
    if (p) {
        p->state = ZOMBIE;
        sched();                /* 永不返回 */
    }
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

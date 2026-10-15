#include "sem.h"
#include "proc.h"

void sem_init(struct semaphore* s,int count)
{
    s->count=count;
}

void sem_wait(struct semaphore* s)
{
    while(s->count<=0)
    {
        sleep(s);
    }
    s->count--;
}

void sem_signal(struct semaphore* s)
{
    s->count++;
    wakeup(s);
}

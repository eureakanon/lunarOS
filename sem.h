#ifndef SEM_H
#define SEM_H
/*信息量*/
struct semaphore
{
    int count;
};

void sem_init(struct semaphore* s,int count);
void sem_wait(struct semaphore* s);
void sem_signal(struct semaphore* s);
#endif

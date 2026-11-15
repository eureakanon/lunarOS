#ifndef _IRQ_LOCK_H
#define _IRQ_LOCK_H
#include"types.h"
uint32 irq_save(void);
void irq_restore(uint32 f);
#endif

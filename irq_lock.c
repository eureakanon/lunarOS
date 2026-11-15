#include"irq_lock.h"

uint32 irq_save(void) {
    uint32 f;
    __asm__ volatile ("pushf; pop %0; cli" : "=r"(f) :: "memory");
    return f;
}
void irq_restore(uint32 f) {
    __asm__ volatile ("push %0; popf" :: "r"(f) : "cc", "memory");
}


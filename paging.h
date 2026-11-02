#ifndef _PAGING_H
#define _PAGING_H
#include"types.h"

#define PAGE_SIZE 4096

/*标志位*/
#define PAGE_PRESENT 0x001
#define PAGE_WRITE 0x002
#define PAGE_USER 0x004
#define PAGE_4MB 0x080
#define PAGE_ADDR 0xFFFFF000

extern uint32* kernel_pgd;

void    paging_init(void);
uint32 *paging_new_pgd(void);           /* 为新进程分配页目录 */
int     paging_map_4k(uint32 *pgd, uint32 virt, uint32 phys, uint32 flags);
void    paging_switch(uint32 *pgd);
void    paging_enable(uint32 *pgd);     /* 首次开启分页（不返回） */



#endif

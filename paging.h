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
/*恒等映射*/
#define IDENTITY_MAP_END 0x02000000
/*虚拟地址基址*/
#define USER_VADDR_BASE 0x40000000

#define KERNEL_PDE_COUNT (IDENTITY_MAP_END >> 22)
#define USER_PGD_INDEX (USER_VADDR_BASE >> 22)

/* ★ 用户进程虚拟地址布局 ★ */
#define USER_CODE_VADDR    0x40000000     /* 用户代码段 */
#define USER_STACK_TOP     0x40003000     /* 用户栈顶（esp 初始值） */

#define PAGE_COW 0x200/*COW标志位*/
extern uint32* kernel_pgd;

void    paging_init(void);
int     paging_map_4k(uint32 *pgd, uint32 virt, uint32 phys, uint32 flags);
void    paging_switch(uint32 *pgd);
void    paging_enable(uint32 *pgd);     /* 首次开启分页（不返回） */

uint32* paging_new_proc_pgd(void);      /*为进程分配页目录*/
int     paging_map_user(uint32 *pgd, uint32 vaddr, uint32 phys);
/*在fork的时候复制父进程的地址空间*/
uint32* copy_addr_space(uint32* parent_pgd);
void free_addr_space(uint32* pgd);

int paging_copy_on_write(uint32* pgd,uint32 vaddr);
void paging_invlpg(uint32 vaddr);
void paging_flush_tlb(void);
/*解绑定*/
int paging_unmap_4k(uint32* pgd,uint32 virt);
int paging_is_mapped(uint32* pgd,uint32 virt);
uint32 paging_get_phys(uint32 *pgd, uint32 virt);
#endif

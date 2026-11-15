#ifndef _PMM_H
#define _PMM_H

#include"types.h"

#define PAGE_SIZE 4096 //4KB
void pmm_init(uint32 phys_start,uint32 phys_end);
void *pmm_alloc_page(void);
void pmm_free_page(void* addr);
void pmm_ref_inc(void* addr);/*COW 引用计数*/
uint32 pmm_ref_count(void* addr);
uint32 pmm_free_count(void);
uint32 pmm_total_count(void);
#endif

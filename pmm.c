#include"pmm.h"
#include"output.h"

#define MAX_PAGES (32*1024*1024/PAGE_SIZE)

static uint32 pmm_bitmap[MAX_PAGES/32];/*位图*/
static uint32 pmm_phys_start;/*位图的物理地址*/
static uint32 pmm_npages;/*实际管理的页数*/
static uint32 pmm_free; /*空闲的页数*/

static inline void bit_set(uint32 i) { pmm_bitmap[i / 32] |=  (1u << (i % 32)); }
static inline void bit_clr(uint32 i) { pmm_bitmap[i / 32] &= ~(1u << (i % 32)); }
static inline int  bit_get(uint32 i) { return (pmm_bitmap[i / 32] >> (i % 32)) & 1; }

void pmm_init(uint32 phys_start,uint32 phys_end)
{
    for(uint32 i=0;i<MAX_PAGES/32;i++)
    {
        pmm_bitmap[i]=0;
    }
    pmm_phys_start=(phys_start+PAGE_SIZE-1)&~(PAGE_SIZE-1);

    uint32 avail=(phys_end-pmm_phys_start)/PAGE_SIZE;
    if(avail>MAX_PAGES)
    {
        avail=MAX_PAGES;
    }
    pmm_npages=avail;
    pmm_free=avail;

    puts("[pmm] start=");
    put_hex(pmm_phys_start);
    puts(" pages=");
    put_dec(pmm_npages);
    puts(" (");
    put_dec(pmm_free);
    puts(" free)\n");
}

void* pmm_alloc_page(void)
{
    for(uint32 i=0;i<pmm_npages;i++)
    {   
        if(!bit_get(i))
        {
            bit_set(i);
            pmm_free--;
            return (void*)(pmm_phys_start+i*PAGE_SIZE);
        }
    }
    return 0;/*OOM*/
}

void pmm_free_page(void *addr)
{
    uint32 a = (uint32)addr;
    if (a < pmm_phys_start || a >= pmm_phys_start + pmm_npages * PAGE_SIZE)
        return;
    uint32 i = (a - pmm_phys_start) / PAGE_SIZE;
    if (bit_get(i)) {
        bit_clr(i);
        pmm_free++;
    }
}

uint32 pmm_free_count(void)  { return pmm_free; }
uint32 pmm_total_count(void) { return pmm_npages; }

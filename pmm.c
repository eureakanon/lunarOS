#include"pmm.h"
#include"output.h"

#define MAX_PAGES (32*1024*1024/PAGE_SIZE)

static uint32 pmm_bitmap[MAX_PAGES/32];/*位图*/
static uint16 pmm_refs[MAX_PAGES];
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
    for(uint32 i=0;i<MAX_PAGES;i++)
    {
        pmm_refs[i]=0;
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
            pmm_refs[i]=1;/*分配成功,引用计数为1*/
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
    if(!bit_get(i))
    {
        return;
    }
    if(pmm_refs[i]>1)
    {
        pmm_refs[i]--;
        return;
    }
    pmm_refs[i]=0;
    bit_clr(i);
    pmm_free++;
}
void pmm_ref_inc(void *addr)
{
    uint32 a = (uint32)addr;
    if (a < pmm_phys_start || a >= pmm_phys_start + pmm_npages * PAGE_SIZE)
        return;
    uint32 i = (a - pmm_phys_start) / PAGE_SIZE;
    if (bit_get(i) && pmm_refs[i] != 0xFFFF)
        pmm_refs[i]++;
}

uint32 pmm_ref_count(void *addr)
{
    uint32 a = (uint32)addr;
    if (a < pmm_phys_start || a >= pmm_phys_start + pmm_npages * PAGE_SIZE)
        return 0;
    uint32 i = (a - pmm_phys_start) / PAGE_SIZE;
    return bit_get(i) ? pmm_refs[i] : 0;
}

uint32 pmm_free_count(void)  { return pmm_free; }
uint32 pmm_total_count(void) { return pmm_npages; }

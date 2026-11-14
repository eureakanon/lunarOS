#include"pmm.h"
#include"output.h"

#define MAX_PAGES (32*1024*1024/PAGE_SIZE)

static uint32 pmm_bitmap[MAX_PAGES/32];
static uint16 pmm_refs[MAX_PAGES];
static uint32 pmm_phys_start;
static uint32 pmm_npages;
static uint32 pmm_free;

static inline void bit_set(uint32 i) { pmm_bitmap[i/32] |=  (1u << (i%32)); }
static inline void bit_clr(uint32 i) { pmm_bitmap[i/32] &= ~(1u << (i%32)); }
static inline int  bit_get(uint32 i) { return (pmm_bitmap[i/32] >> (i%32)) & 1; }

uint32 irq_save(void) {
    uint32 f;
    __asm__ volatile ("pushf; pop %0; cli" : "=r"(f) :: "memory");
    return f;
}
void irq_restore(uint32 f) {
    __asm__ volatile ("push %0; popf" :: "r"(f) : "cc", "memory");
}

/* 物理地址 → 页号；越界返回 (uint32)-1 */
static inline uint32 pmm_page_index(uint32 a)
{
    if (a < pmm_phys_start)
        return (uint32)-1;
    if (a >= pmm_phys_start + pmm_npages * PAGE_SIZE)
        return (uint32)-1;
    return (a - pmm_phys_start) / PAGE_SIZE;
}

void pmm_init(uint32 phys_start, uint32 phys_end)
{
    /* 启动时单核单流，无需关中断 */
    for (uint32 i = 0; i < MAX_PAGES/32; i++)
        pmm_bitmap[i] = 0;
    for (uint32 i = 0; i < MAX_PAGES; i++)
        pmm_refs[i] = 0;

    pmm_phys_start = (phys_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uint32 avail = (phys_end - pmm_phys_start) / PAGE_SIZE;
    if (avail > MAX_PAGES)
        avail = MAX_PAGES;
    pmm_npages = avail;
    pmm_free   = avail;

    puts("[pmm] start="); put_hex(pmm_phys_start);
    puts(" pages="); put_dec(pmm_npages);
    puts(" (");      put_dec(pmm_free);
    puts(" free)\n");
}

void* pmm_alloc_page(void)
{
    uint32 f = irq_save();
    void *ret = 0;

    for (uint32 i = 0; i < pmm_npages; i++) {
        if (!bit_get(i)) {
            bit_set(i);
            pmm_free--;
            pmm_refs[i] = 1;
            ret = (void*)(pmm_phys_start + i * PAGE_SIZE);
            break;
        }
    }

    irq_restore(f);
    return ret;
}

void pmm_free_page(void *addr)
{
    uint32 f = irq_save();
    uint32 i = pmm_page_index((uint32)addr);

    if (i != (uint32)-1 && bit_get(i)) {
        if (pmm_refs[i] > 1) {
            pmm_refs[i]--;
        } else {
            pmm_refs[i] = 0;
            bit_clr(i);
            pmm_free++;
        }
    }

    irq_restore(f);
}

void pmm_ref_inc(void *addr)
{
    uint32 f = irq_save();
    uint32 i = pmm_page_index((uint32)addr);

    if (i != (uint32)-1 && bit_get(i) && pmm_refs[i] != 0xFFFF)
        pmm_refs[i]++;

    irq_restore(f);
}

uint32 pmm_ref_count(void *addr)
{
    uint32 f = irq_save();
    uint32 ret = 0;
    uint32 i = pmm_page_index((uint32)addr);

    if (i != (uint32)-1 && bit_get(i))
        ret = pmm_refs[i];

    irq_restore(f);
    return ret;
}

uint32 pmm_free_count(void)
{
    uint32 f = irq_save();
    uint32 ret = pmm_free;
    irq_restore(f);
    return ret;
}

uint32 pmm_total_count(void)
{
    return pmm_npages;   /* 初始化后不再变化，无需保护 */
}
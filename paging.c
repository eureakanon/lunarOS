#include"paging.h"
#include"pmm.h"
#include"output.h"
/*启动时使用的页目录*/
#define USER_VADDR_START  0x40000000
#define USER_VADDR_END    0x40003000
static uint32 boot_pgd[1024] __attribute__((aligned(4096)));
uint32* kernel_pgd=boot_pgd;

void paging_init(void)
{
    for(uint32 addr=0;addr<IDENTITY_MAP_END;addr+=PAGE_SIZE)
    {
        uint32 flags=PAGE_PRESENT|PAGE_WRITE;
        if (paging_map_4k(boot_pgd, addr, addr, flags) < 0) {
            puts("[paging] map failed at 0x");
            put_hex(addr);
            putchar('\n');
            for (;;) __asm__ volatile ("cli; hlt");
        }
    }
    puts("[paging] pgd=");
    put_hex((uint32)boot_pgd);
    puts("  32MB identity mapped, user region 0x");
    put_hex(USER_VADDR_START);
    puts("~0x");
    put_hex(USER_VADDR_END);
    putchar('\n');

    /* 3. 开启分页 */
    paging_enable(boot_pgd);

    puts("[paging] paging enabled!\n");
}

int paging_map_4k(uint32* pgd,uint32 virt,uint32 phys,uint32 flags)
{
    uint32 pdi=(virt>>22);
    uint32 pti=(virt>>12)&0x3FF;
    if(!(pgd[pdi]&PAGE_PRESENT))
    {
        uint32* pt=(uint32*)pmm_alloc_page();
        if(!pt)
        {
            return -1;
        }
        for(uint32 i=0;i<1024;i++)
        {
            pt[i]=0;
        }
        pgd[pdi] = ((uint32)pt & PAGE_ADDR) | PAGE_PRESENT | PAGE_WRITE;
    }
    if(flags&PAGE_USER)
    {
        pgd[pdi]|=PAGE_USER;
    }
    uint32 *pt = (uint32 *)(pgd[pdi] & PAGE_ADDR);
    pt[pti] = (phys & PAGE_ADDR) | flags;
    return 0;
}

void paging_switch(uint32 *pgd)
{
    __asm__ volatile ("mov %0, %%cr3" :: "r"((uint32)pgd) : "memory");
}

void paging_enable(uint32 *pgd)
{
    /*
     * 开启分页：加载 CR3，然后置位 CR0.PG。
     *
     * 由于当前执行流的虚拟地址已经在 pgd 中被恒等映射，
     * 开启后 EIP / ESP / VGA / GDT / IDT 都能直接翻译到物理地址，
     * 不会触发 page fault。
     */
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or  $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"((uint32)pgd)
        : "eax", "memory"
    );
}

/* ============================================================
 * ★ 新增：为一个进程创建独立页目录
 *   - 内核部分（PDE 0~7，覆盖 0~32MB）直接复用 kernel_pgd 的内容
 *   - 用户部分（PDE 256，覆盖 0x40000000）留空，由调用者填
 * ============================================================ */

uint32* paging_new_proc_pgd()
{
    uint32* pgd=(uint32*)pmm_alloc_page();
    if(!pgd)
    {
        return 0;
    }
    for(uint32 i=0;i<1024;i++)
    {
        pgd[i]=0;
    }
    /*恒等映射*/
    for(uint32 i=0;i<KERNEL_PDE_COUNT;i++)
    {
        pgd[i]=kernel_pgd[i];
    }
    return pgd;
}
int paging_map_user(uint32 *pgd, uint32 vaddr, uint32 phys)
{
    return paging_map_4k(pgd, vaddr, phys,
                         PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
}

uint32* copy_addr_space(uint32* parent_pgd)
{
    uint32* child_pgd = paging_new_proc_pgd();
    if (!child_pgd)
        return 0;

    for (uint32 pdi = USER_PGD_INDEX; pdi < 1024; pdi++)
    {
        uint32 pde = parent_pgd[pdi];
        if (!(pde & PAGE_PRESENT))
            continue;
        if (!(pde & PAGE_USER))
            continue;

        uint32* parent_pt = (uint32*)(pde & PAGE_ADDR);
        for (uint32 pti = 0; pti < 1024; pti++)
        {
            uint32 pte = parent_pt[pti];
            if ((pte & (PAGE_PRESENT | PAGE_USER)) !=
                (PAGE_PRESENT | PAGE_USER))
                continue;

            void* new_phys = pmm_alloc_page();
            if (!new_phys)
                return 0;

            char *src = (char *)(pte & PAGE_ADDR);
            char *dst = (char *)new_phys;
            for (int i = 0; i < PAGE_SIZE; i++)
                dst[i] = src[i];

            uint32 vaddr = (pdi << 22) | (pti << 12);
            if (paging_map_user(child_pgd, vaddr, (uint32)new_phys) < 0)
                return 0;
        }
    }
    return child_pgd;
}

/* 释放用户地址空间（页表 + 所有用户物理页 + pgd 本身） */
void free_addr_space(uint32 *pgd)
{
    if (!pgd) return;

    for (int pdi = USER_PGD_INDEX; pdi < 1024; pdi++)
    {
        uint32 pde = pgd[pdi];
        if (!(pde & PAGE_PRESENT)) continue;
        if (!(pde & PAGE_USER))    continue;

        uint32 *pt = (uint32 *)(pde & PAGE_ADDR);
        for (int pti = 0; pti < 1024; pti++)
        {
            uint32 pte = pt[pti];
            if ((pte & (PAGE_PRESENT | PAGE_USER)) !=
                (PAGE_PRESENT | PAGE_USER))
                continue;

            pmm_free_page((void *)(pte & PAGE_ADDR));
        }
        pmm_free_page(pt);      /* 页表本身也要还 */
    }
    pmm_free_page(pgd);
}

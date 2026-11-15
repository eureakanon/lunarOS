#include"paging.h"
#include"pmm.h"
#include"output.h"
#include"irq_lock.h"
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
        if (!(pde & PAGE_PRESENT)) continue;
        if (!(pde & PAGE_USER))    continue;

        uint32* parent_pt = (uint32*)(pde & PAGE_ADDR);
        for (uint32 pti = 0; pti < 1024; pti++)
        {
            uint32 pte = parent_pt[pti];
            if ((pte & (PAGE_PRESENT | PAGE_USER)) !=
                (PAGE_PRESENT | PAGE_USER))
                continue;

            uint32 phys  = pte & PAGE_ADDR;
            uint32 vaddr = (pdi << 22) | (pti << 12);
            uint32 flags = PAGE_PRESENT | PAGE_USER;

            if (pte & PAGE_COW) {
                pmm_ref_inc((void*)phys);
                flags |= PAGE_COW;
            } else if (pte & PAGE_WRITE) {
                pmm_ref_inc((void*)phys);
                parent_pt[pti] = phys | PAGE_PRESENT | PAGE_USER | PAGE_COW;
                flags |= PAGE_COW;
            } else {
                pmm_ref_inc((void*)phys);
            }

            if (paging_map_4k(child_pgd, vaddr, phys, flags) < 0) {
                /* 这一页 ref_inc 了但没进 child 页表，手动归还 */
                pmm_free_page((void*)phys);
                /* 父进程 PTE 可能已被改成 COW 只读，TLB 必须刷 */
                paging_flush_tlb();
                /* 回收 child：已 map 的页会在这里被 put */
                free_addr_space(child_pgd);
                return 0;
            }
        }
    }
    paging_flush_tlb();
    return child_pgd;
}

/*缺页处理COW*/
int paging_copy_on_write(uint32 *pgd, uint32 vaddr)
{
    uint32 pdi = vaddr >> 22;
    uint32 pti = (vaddr >> 12) & 0x3FF;
    if (!(pgd[pdi] & PAGE_PRESENT)) return -1;
    uint32 *pt = (uint32*)(pgd[pdi] & PAGE_ADDR);
    uint32 pte = pt[pti];
    if ((pte & (PAGE_PRESENT | PAGE_COW)) != (PAGE_PRESENT | PAGE_COW))
        return -1;

    uint32 old = pte & PAGE_ADDR;

    /* ---- 临界区开始：读 refcount → 判断 → 改 PTE 必须原子 ---- */
    uint32 f=irq_save();
    uint32 ref = pmm_ref_count((void*)old);
    if (ref == 0) {
    /* PTE 指向已释放页：说明前面有 double put，拒绝处理并暴露问题 */
        irq_restore(f);
        return -1;
    }
    if (ref == 1) {
        pt[pti] = old | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
        paging_invlpg(vaddr);
        irq_restore(f);
        return 0;
    }

    void *new_phys = pmm_alloc_page();   /* 内部又 irq_save/restore，嵌套 cli 安全 */
    if (!new_phys) {
        irq_restore(f);
        return -1;
    }

    /* memcpy 在临界区里：4KB 大约几微秒，教学内核可接受。
       如果嫌中断延迟长，见下方"优化版"。 */
    char *src = (char*)old, *dst = (char*)new_phys;
    for (int i = 0; i < PAGE_SIZE; i++) dst[i] = src[i];

    pmm_free_page((void*)old);   /* put：ref-- */
    pt[pti] = ((uint32)new_phys) | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    paging_invlpg(vaddr);

    irq_restore(f);
    return 0;
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

void paging_invlpg(uint32 vaddr)
{
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
}

void paging_flush_tlb(void)
{
    uint32 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

int paging_unmap_4k(uint32* pgd,uint32 virt)
{
    uint32 pdi=virt>>22;
    uint32 pti=(virt>>12)&0x3FF;
    /* 1.不可以动内核区 */
    if (pdi < USER_PGD_INDEX)
        return -2;

    /* 2. PDE 必须存在 */
    if (!(pgd[pdi] & PAGE_PRESENT))
        return -1;
    uint32* pt=(uint32*)(pgd[pdi]&PAGE_ADDR);

    uint32 pte=pt[pti];
   
    if(!(pte&PAGE_PRESENT))
    {
        return -1;
    }
    uint32 phys=pte&PAGE_ADDR;
    pt[pti]=0;

    paging_invlpg(virt);

    /* 3. put：共享时只减引用，归零才真正释放 */
    pmm_free_page((void *)phys);

    for (int i = 0; i < 1024; i++)
        if (pt[i] & PAGE_PRESENT)
            return 0;              /* 还有别的页，保留 PT */

    pgd[pdi] = 0;
    pmm_free_page(pt);            /* PT 页每进程独有，ref=1，真释放 */
    return 0;
}

/* 返回 virt 是否已映射，1=是，0=否 */
int paging_is_mapped(uint32 *pgd, uint32 virt)
{
    uint32 pdi = virt >> 22;
    uint32 pti = (virt >> 12) & 0x3FF;
    if (!(pgd[pdi] & PAGE_PRESENT)) return 0;
    uint32 *pt = (uint32 *)(pgd[pdi] & PAGE_ADDR);
    return (pt[pti] & PAGE_PRESENT) ? 1 : 0;
}

/* 返回 virt 对应的物理地址；未映射返回 0 */
uint32 paging_get_phys(uint32 *pgd, uint32 virt)
{
    uint32 pdi = virt >> 22;
    uint32 pti = (virt >> 12) & 0x3FF;
    if (!(pgd[pdi] & PAGE_PRESENT)) return 0;
    uint32 *pt = (uint32 *)(pgd[pdi] & PAGE_ADDR);
    if (!(pt[pti] & PAGE_PRESENT)) return 0;
    return (pt[pti] & PAGE_ADDR) | (virt & 0xFFF);  /* 保留页内偏移 */
}


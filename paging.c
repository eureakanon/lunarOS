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

            uint32 phys=pte&PAGE_ADDR;
	        uint32 vaddr = (pdi << 22) | (pti << 12);
            uint32 flags = PAGE_PRESENT | PAGE_USER;
	        if(pte&PAGE_COW)/*父进程页表标志位有COW*/
            {
                pmm_ref_inc((void*)phys);/*引用计数增加*/
                flags|=PAGE_COW;
            }
            else if(pte&PAGE_WRITE)
            {
                pmm_ref_inc((void*)phys);
                parent_pt[pti] = phys | PAGE_PRESENT | PAGE_USER | PAGE_COW;
                flags |= PAGE_COW;/*如果父进程是可写的,子进程需要说明自己需要COW,并且父进程也要改为COW*/
            }
            else
            {
                pmm_ref_inc((void*)phys);
            }
            if (paging_map_4k(child_pgd, vaddr, phys, flags) < 0)
                return 0;
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

    if (!(pgd[pdi] & PAGE_PRESENT))
        return -1;

    uint32 *pt = (uint32*)(pgd[pdi] & PAGE_ADDR);
    uint32 pte = pt[pti];

    if ((pte & (PAGE_PRESENT | PAGE_COW)) != (PAGE_PRESENT | PAGE_COW))
        return -1;

    uint32 old = pte & PAGE_ADDR;

    /* 只有自己引用：直接恢复可写，省一次复制 */
    if (pmm_ref_count((void*)old) <= 1)
    {
        pt[pti] = old | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
        paging_invlpg(vaddr);
        return 0;
    }

    void *new_phys = pmm_alloc_page();
    if (!new_phys)
        return -1;

    char *src = (char*)old;
    char *dst = (char*)new_phys;
    for (int i = 0; i < PAGE_SIZE; i++)
        dst[i] = src[i];

    pmm_free_page((void*)old);  /* 引用计数 -1 */

    pt[pti] = ((uint32)new_phys) | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    paging_invlpg(vaddr);
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



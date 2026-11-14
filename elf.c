#include"elf.h"
#include"paging.h"
#include"pmm.h"
#include"klibc.h"

static int elf_check(const Elf32_Ehdr *eh, uint32 size)
{
    if (size < sizeof(Elf32_Ehdr))
        return -1;
    if (*(const uint32*)(eh->e_ident) != ELF_MAGIC)
        return -1;
    if (eh->e_ident[4] != 1)          /* ELFCLASS32 */
        return -1;
    if (eh->e_ident[5] != 1)          /* 小端 */
        return -1;
    if (eh->e_type != ET_EXEC || eh->e_machine != EM_386)
        return -1;

    /* 程序头表必须完整落在文件内（防止 uint32 溢出绕过） */
    if (eh->e_phoff > size)
        return -1;
    if (eh->e_phnum > (size - eh->e_phoff) / sizeof(Elf32_Phdr))
        return -1;

    /* 非空程序头表时，表项大小必须符合 ELF32 规范 */
    if (eh->e_phnum != 0 && eh->e_phentsize != sizeof(Elf32_Phdr))
        return -1;

    return 0;
}

int elf_load_image(uint32 *pgd, const uint8 *img, uint32 size, uint32 *entry)
{
    const Elf32_Ehdr *eh = (const Elf32_Ehdr*)img;
    if (elf_check(eh, size) < 0)
        return -1;

    for (uint16 i = 0; i < eh->e_phnum; i++)
    {
        const Elf32_Phdr *ph =
            (const Elf32_Phdr*)(img + eh->e_phoff + i * sizeof(Elf32_Phdr));

        if (ph->p_type != PT_LOAD || ph->p_memsz == 0)
            continue;

        /* ---- 文件范围检查（减法形式，防 uint32 溢出） ---- */
        if (ph->p_offset > size || ph->p_filesz > size - ph->p_offset)
            goto fail;
        if (ph->p_filesz > ph->p_memsz)
            goto fail;

        /* ---- 用户地址空间边界检查 ---- */
        if (ph->p_vaddr < USER_VADDR_BASE)
            goto fail;
        if (ph->p_vaddr >= USER_STACK_TOP - PAGE_SIZE)
            goto fail;
        /* 等价于 p_vaddr + p_memsz <= USER_STACK_TOP - PAGE_SIZE，
           但写成减法避免 p_vaddr + p_memsz 溢出 */
        if (ph->p_memsz > USER_STACK_TOP - PAGE_SIZE - ph->p_vaddr)
            goto fail;

        uint32 vstart = ph->p_vaddr & ~0xFFFu;
        uint32 vend   = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFFu;

        for (uint32 va = vstart; va < vend; va += PAGE_SIZE)
        {
            uint32 flags = PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
            void *phys;

            if (paging_is_mapped(pgd, va))
            {
                /*
                 * 段重叠：复用已有页。
                 * - 不 memset（保留前一段内容）
                 * - 不 map（PTE 已存在，重复 map 会覆盖 flags / 清掉 COW）
                 * - 不改权限（当前所有段都是 RW，若将来按段设权限，
                 *   这里应做权限合并而非直接复用）
                 */
                phys = (void *)(paging_get_phys(pgd, va) & PAGE_ADDR);
            }
            else
            {
                phys = pmm_alloc_page();
                if (!phys)
                    goto fail;

                memset(phys, 0, PAGE_SIZE);   /* BSS 部分靠清零 */
                if (paging_map_4k(pgd, va, (uint32)phys, flags) < 0)
                {
                    /* phys 还没进页表，直接 put 是安全的 */
                    pmm_free_page(phys);
                    goto fail;
                }
            }

            /* ---- 文件内容与当前页的交集 ---- */
            uint32 copy_va_start = va;
            uint32 copy_va_end   = va + PAGE_SIZE;
            if (copy_va_start < ph->p_vaddr)
                copy_va_start = ph->p_vaddr;
            if (copy_va_end > ph->p_vaddr + ph->p_filesz)
                copy_va_end = ph->p_vaddr + ph->p_filesz;

            if (copy_va_start < copy_va_end)
            {
                uint32 n       = copy_va_end - copy_va_start;
                uint32 src_off = ph->p_offset + (copy_va_start - ph->p_vaddr);
                uint32 dst_off = copy_va_start - va;
                memcpy((uint8*)phys + dst_off, img + src_off, n);
            }
        }
    }

    *entry = eh->e_entry;
    return 0;

fail:
    /*
     * 回滚：遍历整个用户区 unmap，未映射的返回 -1 被忽略。
     *
     * 前提：调用时 pgd 的用户区为空（典型场景：paging_new_proc_pgd()
     *       刚建好的新 pgd）。若在已有映射的 pgd 上复用本函数（exec），
     *       这里会误删原有映射，必须改为"记录已映射 va 再回滚"。
     */
    for (uint32 va = USER_VADDR_BASE; va < USER_STACK_TOP - PAGE_SIZE; va += PAGE_SIZE)
        paging_unmap_4k(pgd, va);
    return -1;
}

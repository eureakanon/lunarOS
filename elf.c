#include"elf.h"
#include"paging.h"
#include"pmm.h"
#include"klibc.h"

static int elf_check(const Elf32_Ehdr *eh,uint32 size)
{
    if(size<sizeof(Elf32_Ehdr))
    {
        return -1;
    }
    if(*(const uint32*)(eh->e_ident)!=ELF_MAGIC)
    {
        return -1;
    }/*检查魔数*/
    if(eh->e_ident[4]!=1)
    {
        return -1;
    }/*ELFCLASS32*/
    if(eh->e_ident[5]!=1)
    {
        return -1;
    }/*小端序*/
    if(eh->e_type!=ET_EXEC||eh->e_machine!=EM_386)
    {
        return -1;
    }
    if (eh->e_phoff > size ||
        eh->e_phoff + (uint32)eh->e_phnum * sizeof(Elf32_Phdr) > size)
    {
        return -1;
    }
    return 0;
};/*检查可执行文件的正确性*/

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

        if (ph->p_offset + ph->p_filesz > size)
            return -1;
        if (ph->p_filesz > ph->p_memsz)
            return -1;

        /* 只允许加载到用户区，且不要覆盖栈页 */
        if (ph->p_vaddr < USER_VADDR_BASE)
            return -1;
        if (ph->p_vaddr + ph->p_memsz > USER_STACK_TOP - PAGE_SIZE)
            return -1;

        uint32 vstart = ph->p_vaddr & ~0xFFFu;
        uint32 vend   = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFFu;

        for (uint32 va = vstart; va < vend; va += PAGE_SIZE)
        {
            void *phys = pmm_alloc_page();
            if (!phys)
                return -1;

            memset(phys, 0, PAGE_SIZE);

            /* 文件内容与当前页的交集 */
            uint32 copy_va_start = va;
            uint32 copy_va_end   = va + PAGE_SIZE;
            if (copy_va_start < ph->p_vaddr)
                copy_va_start = ph->p_vaddr;
            if (copy_va_end > ph->p_vaddr + ph->p_filesz)
                copy_va_end = ph->p_vaddr + ph->p_filesz;

            if (copy_va_start < copy_va_end)
            {
                uint32 n = copy_va_end - copy_va_start;
                uint32 src_off = ph->p_offset + (copy_va_start - ph->p_vaddr);
                uint32 dst_off = copy_va_start - va;
                memcpy((uint8*)phys + dst_off, img + src_off, n);
            }

            /*
             * 为了简单，所有 PT_LOAD 都映射为可写。
             * 想更严格可以按 ph->p_flags & 2 决定是否加 PAGE_WRITE，
             * 但要在链接脚本里按页对齐 text/data。
             */
            uint32 flags = PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
            if (paging_map_4k(pgd, va, (uint32)phys, flags) < 0)
                return -1;
        }
    }

    *entry = eh->e_entry;
    return 0;
}

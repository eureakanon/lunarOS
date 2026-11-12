#ifndef _ELF_H
#define _ELF_H
#include"types.h"
typedef struct
{
    uint8 e_ident[16];
    uint16 e_type;
    uint16 e_machine;
    uint32 e_version;
    uint32 e_entry;
    uint32 e_phoff;
    uint32 e_shoff;
    uint32 e_flags;
    uint16 e_ehsize;
    uint16 e_phentsize;
    uint16 e_phnum;
    uint16 e_shentsize;
    uint16 e_shnum;
    uint16 e_shstrndx;
}Elf32_Ehdr;
/*Elf头*/
typedef struct {
    uint32 p_type;
    uint32 p_offset;
    uint32 p_vaddr;
    uint32 p_paddr;
    uint32 p_filesz;
    uint32 p_memsz;
    uint32 p_flags;
    uint32 p_align;
} Elf32_Phdr;

#define ELF_MAGIC   0x464C457F
#define ET_EXEC     2
#define EM_386      3
#define PT_LOAD     1

int elf_load_image(uint32 *pgd, const uint8 *img, uint32 size, uint32 *entry);

#endif

#include"tss.h"
#include"desctbl.h"

struct tss_entry tss;

void tss_init(void)
{
    char* p=(char*)(&tss);
    for(unsigned int i=0;i<sizeof(tss);i++)
    {
        p[i]=0;
    }
    /*强制进行清除操作*/
    tss.ss0=0x08;/*内核态数据段*/
    tss.esp0=0;/*还未设置内核态栈指针*/
    tss.iomap_base=sizeof(tss);

    struct SegmentDescriptor* gdt=(struct SegmentDescriptor*)(ADR_GDT);
    set_segmdesc(gdt+5,sizeof(tss)-1,(uint)&tss,AR_TSS32);

    load_tr(5*8);
}

void tss_set_kernel_stack(uint esp0)
{
    tss.esp0=esp0;
}

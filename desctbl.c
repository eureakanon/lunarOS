#include"desctbl.h"
#include"int.h"
#include"tss.h"
extern unsigned int isr_stub_table[];
void init_gdtidt(void)
{
    struct SegmentDescriptor* gdt=(struct SegmentDescriptor*)(ADR_GDT);
    struct GateDescriptor* idt=(struct GateDescriptor*)(ADR_IDT);

    for(int i=0;i<=LIMIT_GDT/8;i++)
    {
        set_segmdesc(gdt+i,0,0,0);
    }
    set_segmdesc(gdt+1,0xffffffff,0x00000000,AR_DATA32_RW);
    set_segmdesc(gdt+2,0xffffffff,0x00000000,AR_CODE32_ER);
    set_segmdesc(gdt+3,0xffffffff,0x00000000,AR_DATA32_RW_USER);/*增加用户态数据段*/
    set_segmdesc(gdt+4,0xffffffff,0x00000000,AR_CODE32_ER_USER);/*增加用户态代码段*/
    load_gdtr(LIMIT_GDT,ADR_GDT);
    for(int i=0;i<=LIMIT_IDT/8;i++)
    {
        set_gatedesc(idt+i,0,0,0);
    }
    for(int i=0;i<256;i++)
    {
        set_gatedesc(idt+i,isr_stub_table[i],2*8,AR_INTGATE32);
    }
    set_gatedesc(idt+0x80,isr_stub_table[0x80],2*8,AR_INTGATE32_USER);/*开放0x80作为用户中断*/
    load_idtr(LIMIT_IDT,ADR_IDT);

    tss_init();
}

void set_segmdesc(struct SegmentDescriptor* sd,unsigned int limit,int base,int ar)
{
    if(limit>0xfffff)
    {
        ar|=0x8000;
        limit/=0x1000;/*按照4KB表示段界限*/
    }/*数据段打开粒度位*/
    sd->limit_low=limit&0xffff;
    sd->base_low=base&0xffff;
    sd->base_mid=(base>>16)&0xff;
    sd->base_high=(base>>24)&0xff;
    sd->access_right=ar&0xff;
    sd->limit_high=((limit>>16)&0x0f)|((ar>>8)&0xf0);
}

void set_gatedesc(struct GateDescriptor *gd, int offset, int selector, int ar) {
    gd->offset_low = offset & 0xffff;
    gd->selector = selector;
    gd->dw_count = (ar >> 8) & 0xff;
    gd->access_right = ar & 0xff;
    gd->offset_high = (offset >> 16) & 0xffff;
}


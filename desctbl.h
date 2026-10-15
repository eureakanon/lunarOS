#ifndef _DESCTBL_H
#define _DESCTBL_H

#define ADR_IDT 0x0026f800
#define LIMIT_IDT 0x00007ff
#define ADR_GDT 0x00270000
#define LIMIT_GDT 0x0000ffff

#define AR_DATA32_RW 0x4092
#define AR_CODE32_ER 0x409a
#define AR_INTGATE32 0x008e

#include"types.h"

struct SegmentDescriptor
{
    short limit_low,base_low;
    char base_mid,access_right;
    char limit_high,base_high;
};

struct GateDescriptor
{
    short offset_low,selector;
    char dw_count,access_right;
    short offset_high;
};

void init_gdtidt(void);
void set_segmdesc(struct SegmentDescriptor* sd,uint limit,int base,int ar);
void set_gatedesc(struct GateDescriptor* sd,int offset,int selector,int ar);

void load_gdtr(int limit,int addr);
void load_idtr(int limit,int addr);

#endif

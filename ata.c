#include"ata.h"
#include"ioctl.h"

#define ATA_DATA 0x1F0 /*数据口,从该端口阅读可以读到2字节数据*/
#define ATA_ERROR 0x1F1 /*错误信息口*/
#define ATA_SECCNT 0x1F2 /*读写扇区数量*/

#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5/*扇区地址的低 24 位*/

#define ATA_DRIVE 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_CMD 0x1F7

#define ATA_SR_BSY 0x80 /*忙*/
#define ATA_SR_DRDY 0x40/*硬盘准备完成*/
#define ATA_SR_DRQ 0x08/*可以传输数据*/
#define ATA_SR_ERR 0x01/*错误*/

static void ata_delay400ns(void)
{
    for(int i=0;i<4;i++)
    {
        (void)io_in8(ATA_STATUS);
    }
}
/*查询硬盘状态4次*/

static int ata_wait_busy(void)
{
    for(uint32 i=0;i<1000000;i++)
    {
        if(!(io_in8(ATA_STATUS)&ATA_SR_BSY))
        {
            return 0;/*忙等*/
        }
    }
    return -1;/*超时*/
}

static int ata_wait_drq(void)
{
    for(uint32 i=0;i<1000000;i++)
    {
        uint8 st=io_in8(ATA_STATUS);
        if(st&ATA_SR_ERR)
        {
            return -1;
        }
        if(!(st&ATA_SR_BSY)&&(st&ATA_SR_DRQ))
        {
            return 0;
        }
    }
    return -1;
}
/*等待数据就绪*/

int ata_read_sector(uint32 lba,void* buf)
{
    if(ata_wait_bsy()<0)
    {
        return -1;
    }
    io_out8(ATA_DRIVE,0xE0|((lba>>24)&0x0F));/*0xE0将高三位置为1(lba模式),然后拼上高四位lba,选择主盘还是从盘*/
    ata_delay400ns();
    io_out8(ATA_SECCNT,1);
    io_out8(ATA_LBA0,(uint8)(lba&0xFF));
    io_out8(ATA_LBA1,(uint8)((lba>>8)&0xFF));
    io_out8(ATA_LBA2,(uint8)((lba>>16)&0xFF));
    io_out8(ATA_CMD,0x20);/*读扇区*/

    if(ata_wait_bsy()<0)
    {
        return -1;
    }
    if(io_in8(ATA_STATUS)&ATA_SR_ERR)
    {
        return -1;
    }
    if(ata_wait_drq()<0)
    {
        return -1;
    }
    uint16* p=(uint16*)buf;
    for(int i=0;i<256;i++)
    {
        p[i]=(uint16)io_in16(ATA_DATA);
    }
    return 0;
}

int ata_write_sector(uint32 lba,const void* buf)
{
    if(ata_wait_bsy()<0)
    {
        return -1;
    }
    io_out8(ATA_DRIVE,0xE0|((lba>>24)&0x0F));
    ata_delay400ns();
    io_out8(ATA_SECCNT, 1);
    io_out8(ATA_LBA0, (uint8)(lba & 0xFF));
    io_out8(ATA_LBA1, (uint8)((lba >> 8) & 0xFF));
    io_out8(ATA_LBA2, (uint8)((lba >> 16) & 0xFF));
    io_out8(ATA_CMD, 0x30);         /* WRITE SECTORS */

    if(ata_wait_bsy()<0)
    {
        return -1;
    }
    if(ata_wait_drq()<0)
    {
        return -1;
    }
    const uint16* p=(const uint16*)buf;
    for(int i=0;i<256;i++)
    {
        io_out16(ATA_DATA,p[i]);
    }
    io_out8(ATA_CMD,0xE7);
    if(ata_wait_bsy()<0)
    {
        return -1;
    }
    return 0;
}



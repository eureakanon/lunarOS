#ifndef _ATA_H
#define _ATA_H
#include"types.h"

int ata_read_sector(uint32 lba,void* buf);/*阅读扇区到buf*/
int ata_write_sector(uint32 lba,const void* buf);/*写扇区*/

#endif

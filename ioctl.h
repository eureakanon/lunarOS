#ifndef _IOCTL_H
#define _IOCTL_H

void io_hlt(void);
void io_cli(void);
void io_sti(void);
void io_stihlt(void);

int io_in8(int port);/*向指定端口读取某个大小的数据*/
int io_in16(int port);
int io_in32(int port);

void io_out8(int port, int data);/*向指定端口写入某个大小的数据*/
void io_out16(int port, int data);
void io_out32(int port, int data);

int io_load_eflags(void);/*加载和保存标志位寄存器*/
void io_store_eflags(int eflags);

int io_load_cr0(void);/*加载和保存cr0*/
void io_store_cr0(int cr0);

#endif

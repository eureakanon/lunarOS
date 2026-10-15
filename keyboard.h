#include"fifo.h"

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#define PORT_KEYDAT 0x0060/*这是键盘的数据端口,如果读这个端口可以读取键盘扫描码*/
#define PORT_KEYSTA 0x0064/*获取键盘状态的端口*/
#define PORT_KEYCMD 0x0064/*键盘控制端口*/

#define KEYSTA_SEND_NOTREADY 0x02/*输入缓冲区满,需要等待*/
#define KEYCMD_WRITE_MODE 0x60/*设置为写入模式*/
#define KBC_MODE 0x47

#define KEY_FIFO_BUF_SIZE 32/*缓冲区大小*/
extern struct FIFO8 keyfifo;
extern unsigned char keybuf[KEY_FIFO_BUF_SIZE];

void wait_KBC_sendready(void);/*等待键盘控制器发送就绪*/
void init_keyboard(void);/*初始化键盘*/
void keyboard_handler();
#endif


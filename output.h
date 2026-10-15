#ifndef OUTPUT_H
#define OUTPUT_H
#include"types.h"

void clear_screen(void);/*清除屏幕*/
void putchar(char c);/*打印字符*/
void puts(const char* s);/*打印字符串*/
void put_dec(int v);/*打印整数*/
void put_hex(uint v);/*打印16进制*/

#endif

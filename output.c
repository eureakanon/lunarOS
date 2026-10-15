#include"types.h"
#include"output.h"

#define VIDEO_MEM ((volatile unsigned char*)0xB8000)
#define COLS 80
#define ROWS 25
#define ATTR 0x0f

static int cursor=0;

void clear_screen()
{
    for(int i=0;i<COLS*ROWS;i++)
    {
        VIDEO_MEM[2*i]=' ';
        VIDEO_MEM[2*i+1]=ATTR;
    }
    cursor=0;
}

void putchar(char c)
{
    if (c == '\n') {
        cursor += COLS - (cursor % COLS);
    } else if (c == '\r') {
        cursor -= (cursor % COLS);
    } else if (c == '\t') {
        int n = 4 - (cursor % 4);
        while (n--) putchar(' ');
    } else {
        VIDEO_MEM[cursor * 2]     = (unsigned char)c;
        VIDEO_MEM[cursor * 2 + 1] = ATTR;
        cursor++;
    }
    if (cursor >= COLS * ROWS)
        cursor = 0;
}

void puts(const char* s)
{
    while(*s)
    {
        putchar(*s++);
    }
}

void put_dec(int v)
{
    char buf[12];
    int i = 0;
    unsigned int u;
    if (v < 0) { putchar('-'); u = (unsigned int)(-v); }
    else       { u = (unsigned int)v; }
    if (u == 0) { putchar('0'); return; }
    while (u > 0) { buf[i++] = '0' + (u % 10); u /= 10; }
    while (i > 0)  putchar(buf[--i]);
}

void put_hex(uint v)
{
    const char *hex = "0123456789abcdef";
    char buf[8];
    int i;
    for (i = 0; i < 8; i++) { buf[i] = hex[v & 0xF]; v >>= 4; }
    for (i = 7; i >= 0; i--) putchar(buf[i]);
}





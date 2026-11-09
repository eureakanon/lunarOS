#ifndef _KLIBC_H
#define _KLIBC_H
#include"types.h"

void* memcpy(void* dst,const void* src,uint32 n);
void* memset(void* dst,int c,uint32 n);
int    memcmp(const void *a, const void *b, uint32 n);
uint32 strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, uint32 n);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, uint32 n);

#endif

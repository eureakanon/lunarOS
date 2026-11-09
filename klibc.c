#include "klibc.h"

void *memcpy(void *dst, const void *src, uint32 n)
{
    uint8 *d = (uint8*)dst;
    const uint8 *s = (const uint8*)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, uint32 n)
{
    uint8 *d = (uint8*)dst;
    while (n--) *d++ = (uint8)c;
    return dst;
}

int memcmp(const void *a, const void *b, uint32 n)
{
    const uint8 *x = (const uint8*)a;
    const uint8 *y = (const uint8*)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}

uint32 strlen(const char *s)
{
    uint32 n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(uint8)*a - (int)(uint8)*b;
}

int strncmp(const char *a, const char *b, uint32 n)
{
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(uint8)*a - (int)(uint8)*b;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++)) ;
    return dst;
}

char *strncpy(char *dst, const char *src, uint32 n)
{
    uint32 i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

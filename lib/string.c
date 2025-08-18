/*
 * libc string functions
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License version 2.
 */

#include "lib/avatar_string.h"
#include "avatar_types.h"

size_t
strlen(const char *buf)
{
    size_t len = 0;
    while (*buf++)
        ++len;
    return len;
}

char *
strcat(char *dest, const char *src)
{
    char *p = dest;
    while (*p)
        ++p;
    while ((*p++ = *src++) != 0)
        ;
    return dest;
}

char *
strcpy(char *dest, const char *src)
{
    *dest = 0;
    return strcat(dest, src);
}

int32_t
strncmp(const char *a, const char *b, size_t n)
{
    for (; n--; ++a, ++b)
        if (*a != *b || *a == '\0')
            return *a - *b;
    return 0;
}

int32_t
strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (uint8_t) *a - (uint8_t) *b;
}

char *
strchr(const char *s, int32_t c)
{
    while (*s != (char) c)
        if (*s++ == '\0')
            return NULL;
    return (char *) s;
}

char *
strstr(const char *s1, const char *s2)
{
    size_t l1, l2;
    l2 = strlen(s2);
    if (!l2)
        return (char *) s1;
    l1 = strlen(s1);
    while (l1 >= l2) {
        l1--;
        if (!memcmp(s1, s2, l2))
            return (char *) s1;
        s1++;
    }
    return NULL;
}

void *
memset(void *s, int32_t c, size_t n)
{
    size_t i;
    char  *a = s;
    for (i = 0; i < n; ++i)
        a[i] = c;
    return s;
}

void *
memcpy(void *dest, const void *src, size_t n)
{
    size_t      i;
    char       *a = dest;
    const char *b = src;
    for (i = 0; i < n; ++i)
        a[i] = b[i];
    return dest;
}

int32_t
memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *a = s1, *b = s2;
    int32_t        ret = 0;
    while (n--) {
        ret = *a - *b;
        if (ret)
            break;
        ++a, ++b;
    }
    return ret;
}

void *
memmove(void *dest, const void *src, size_t n)
{
    const uint8_t *s = src;
    uint8_t       *d = dest;
    if (d <= s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n, s += n;
        while (n--)
            *--d = *--s;
    }
    return dest;
}

void *
memchr(const void *s, int32_t c, size_t n)
{
    const uint8_t *str = s, chr = (uint8_t) c;
    while (n--)
        if (*str++ == chr)
            return (void *) (str - 1);
    return NULL;
}

int64_t
atol(const char *ptr)
{
    int64_t     acc = 0;
    const char *s   = ptr;
    int32_t     neg, c;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-') {
        neg = 1;
        s++;
    } else {
        neg = 0;
        if (*s == '+')
            s++;
    }
    while (*s) {
        if (*s < '0' || *s > '9')
            break;
        c   = *s - '0';
        acc = acc * 10 + c;
        s++;
    }
    if (neg)
        acc = -acc;
    return acc;
}

char *
get_file_name(char *name)
{
    char *s = name;
    while (*s != '\0') {
        s++;
    }
    while ((*s != '\\') && (*s != '/') && (s >= name)) {
        s--;
    }
    return s + 1;
}

char *
strncpy(char *dest, const char *src, size_t n)
{
    size_t i = 0;

    // 先拷贝 src
    for (; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    // 如果 src 结束了，补 '\0'
    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}
char *
strrchr(const char *s, int c)
{
    const char *last = NULL;

    while (*s) {
        if (*s == (char) c) {
            last = s;
        }
        s++;
    }

    // 还要检查 '\0'
    if (c == '\0') {
        return (char *) s;
    }

    return (char *) last;
}
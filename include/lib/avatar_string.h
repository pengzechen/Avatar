/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file avatar_string.h
 * @brief Implementation of avatar_string.h
 * @author Avatar Project Team
 * @date 2024
 */

/*
 * Header for libc string functions
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License version 2.
 */
#ifndef __STRING_H
#define __STRING_H

#include "avatar_types.h"

extern size_t
strlen(const char *buf);
extern char *
strcat(char *dest, const char *src);
extern char *
strcpy(char *dest, const char *src);
extern int32_t
strcmp(const char *a, const char *b);
extern int32_t
strncmp(const char *a, const char *b, size_t n);
extern char *
strchr(const char *s, int32_t c);
extern char *
strstr(const char *haystack, const char *needle);
extern void *
memset(void *s, int32_t c, size_t n);
extern void *
memcpy(void *dest, const void *src, size_t n);
extern int32_t
memcmp(const void *s1, const void *s2, size_t n);
extern void *
memmove(void *dest, const void *src, size_t n);
extern void *
memchr(const void *s, int32_t c, size_t n);
extern char *
strncpy(char *dest, const char *src, size_t n);
extern char *
strrchr(const char *s, int c);

int64_t
atol(const char *ptr);
char *
get_file_name(char *name);
#endif /* _STRING_H */

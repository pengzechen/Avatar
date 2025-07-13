
#ifndef __TYPES_H__
#define __TYPES_H__

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

typedef _Bool bool;

#define false 0
#define true 1

typedef unsigned long long size_t;
typedef long long ssize_t;

#define SIZE_MAX (9223372036854775807UL)

#define NULL ((void *)0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define vaddr_t uint64_t
#define paddr_t uint64_t

typedef long long off_t;
#endif
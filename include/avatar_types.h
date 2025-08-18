
#ifndef __TYPES_H__
#define __TYPES_H__

#ifndef __TYPEDEF_STDINT
#define __TYPEDEF_STDINT
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long long uint64_t;

typedef signed char    int8_t;
typedef short          int16_t;
typedef int            int32_t;
typedef long long      int64_t;
#endif

#ifndef __TYPEDEF_BOOL
#define __TYPEDEF_BOOL
typedef _Bool bool;
#define true 1
#define false 0
#endif

typedef  uint64_t uintptr_t;

// size_t: 无符号的、与平台指针大小相同的整数类型
typedef unsigned long long size_t;
// ssize_t: 是有符号版本
typedef long long ssize_t;

#define SIZE_MAX ((size_t)-1)

#define NULL ((void *)0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef uint64_t vaddr_t;
typedef uint64_t paddr_t;


typedef long long off_t;
#endif
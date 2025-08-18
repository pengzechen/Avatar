

#ifndef __MMIO_H__
#define __MMIO_H__
#include "mem/barrier.h"
#include "avatar_types.h"


// 低层裸访问（无 barrier）
// --------------------------------------------

static inline uint8_t
read8(const volatile void *addr)
{
    uint8_t val;
    __asm__ volatile("ldrb %w0, [%1]" : "=r"(val) : "r"(addr));
    return val;
}

static inline void
write8(uint8_t value, volatile void *addr)
{
    __asm__ volatile("strb %w0, [%1]" : : "r"(value), "r"(addr));
}

static inline uint16_t
read16(const volatile void *addr)
{
    uint16_t val;
    __asm__ volatile("ldrh %w0, [%1]" : "=r"(val) : "r"(addr));
    return val;
}

static inline void
write16(uint16_t value, volatile void *addr)
{
    __asm__ volatile("strh %w0, [%1]" : : "r"(value), "r"(addr));
}

static inline uint32_t
read32(const volatile void *addr)
{
    uint32_t val;
    __asm__ volatile("ldr %w0, [%1]" : "=r"(val) : "r"(addr));
    return val;
}

static inline void
write32(uint32_t value, volatile void *addr)
{
    __asm__ volatile("str %w0, [%1]" : : "r"(value), "r"(addr));
}

static inline uint64_t
read64(const volatile void *addr)
{
    uint64_t val;
    __asm__ volatile("ldr %0, [%1]" : "=r"(val) : "r"(addr));
    return val;
}

static inline void
write64(uint64_t value, volatile void *addr)
{
    __asm__ volatile("str %0, [%1]" : : "r"(value), "r"(addr));
}

// --------------------------------------------
// 上层安全访问封装（带 barrier）
// 推荐用于设备 MMIO 操作
// --------------------------------------------

// 写屏障：确保之前写完成
static inline void
mmio_write32(uint32_t value, volatile void *addr)
{
    dsb(sy);  // 确保前序写完成
    write32(value, addr);
    dsb(sy);  // 可选，加了可确保写立即生效（强同步）
}

static inline uint32_t
mmio_read32(const volatile void *addr)
{
    dsb(sy);  // 确保之前访问完成
    uint32_t val = read32(addr);
    dsb(sy);  // 确保这个读完成
    return val;
}

// 同理封装 8/16/64 版本
static inline void
mmio_write8(uint8_t value, volatile void *addr)
{
    dsb(sy);
    write8(value, addr);
    dsb(sy);
}

static inline uint8_t
mmio_read8(const volatile void *addr)
{
    dsb(sy);
    uint8_t val = read8(addr);
    dsb(sy);
    return val;
}

static inline void
mmio_write16(uint16_t value, volatile void *addr)
{
    dsb(sy);
    write16(value, addr);
    dsb(sy);
}

static inline uint16_t
mmio_read16(const volatile void *addr)
{
    dsb(sy);
    uint16_t val = read16(addr);
    dsb(sy);
    return val;
}

static inline void
mmio_write64(uint64_t value, volatile void *addr)
{
    dsb(sy);
    write64(value, addr);
    dsb(sy);
}

static inline uint64_t
mmio_read64(const volatile void *addr)
{
    dsb(sy);
    uint64_t val = read64(addr);
    dsb(sy);
    return val;
}


#endif  // __MMIO_H__
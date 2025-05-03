

#ifndef __EPT_H__
#define __EPT_H__

#define LPAE_SHIFT 9
#define LPAE_ENTRIES (1 << LPAE_SHIFT)
#define LPAE_L1_SIZE (4)
#define LPAE_L2_SIZE (LPAE_L1_SIZE * LPAE_ENTRIES)
#define LPAE_L3_SIZE (LPAE_L2_SIZE * LPAE_ENTRIES)

#define PAR_F                   (1UL << 0)               // PAR 寄存器中的故障标志位
#define PADDR_BITS              40
#define PADDR_MASK              ((1ULL << PADDR_BITS)-1)
#define PAGE_MASK               (1 << 12)                // 页对齐掩码

#include <mem/page.h>
#include <exception.h>

void guest_ept_init(void);

lpae_t * get_ept_entry(paddr_t gpa);

void apply_ept(void *ept);

void data_abort_handler(ept_violation_info_t *info, trap_frame_t *);

int gva_to_ipa(uint64_t va, uint64_t *paddr);


static inline uint64_t read_par(void)
{
  uint64_t par;
  asm volatile("mrs %0, PAR_EL1" : "=r"(par));
  return par;
}

static inline void write_par(uint64_t value)
{
  asm volatile("msr PAR_EL1, %0" : : "r"(value));
}

static inline void write_ats1cpr(uint64_t va)
{
  asm volatile("at s1e1r, %0" : : "r"(va));
}

#endif // __EPT_H__
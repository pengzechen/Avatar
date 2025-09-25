
#ifndef __SYS_REGS__

#define __SYS_REGS__

#define WFI() __asm__ volatile("wfi" : : : "memory")


#endif  // __SYS_REGS__
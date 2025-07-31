
#ifndef __SMP_H__
#define __SMP_H__

#include "aj_types.h"

static inline uint32_t smc_call(uint32_t function_id, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    register uint64_t reg0 __asm__("x0") = function_id;
    register uint64_t reg1 __asm__("x1") = arg0;
    register uint64_t reg2 __asm__("x2") = arg1;
    register uint64_t reg3 __asm__("x3") = arg2;
    int32_t ret = 0;

    asm volatile("smc    #0\n" : "+r"(reg0) : "r"(reg1), "r"(reg2), "r"(reg3));

    ret = (int32_t)reg0;
    return ret;
}

static inline uint32_t hvc_call(uint32_t function_id, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    register uint64_t reg0 __asm__("x0") = function_id;
    register uint64_t reg1 __asm__("x1") = arg0;
    register uint64_t reg2 __asm__("x2") = arg1;
    register uint64_t reg3 __asm__("x3") = arg2;
    int32_t ret = 0;

    asm volatile("hvc    #0\n" : "+r"(reg0) : "r"(reg1), "r"(reg2), "r"(reg3));

    ret = (int32_t)reg0;
    return ret;
}

/*
    uint32_t result = smc_call(PSCI_CPU_ON, 3, 0x40080000, 0x40083000);
    if (result != 0) {
        uart_putstr("start core 1 failed!\n");
    }

*/

void start_secondary_cpus();

#endif
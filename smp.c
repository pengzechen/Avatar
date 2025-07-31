
#include "smp.h"
#include "psci.h"
#include "os_cfg.h"
#include "io.h"
#include "thread.h"
#include "lib/aj_string.h"

extern void second_entry();
extern void second_entry_el2();
extern void _stack_top();
extern void _stack_top_second();


void thread_info_init(struct thread_info *ti, uint32_t flags, int32_t id)
{
    memset(ti, 0, sizeof(struct thread_info));
    ti->cpu = id;
    ti->flags = flags;
}

void start_secondary_cpus()
{
    thread_info_init((struct thread_info *)(_stack_top - STACK_SIZE), 0, 0);
    logger("core 0 thread info addr: %llx\n", (struct thread_into *)(void *)(_stack_top - STACK_SIZE));

    for (int32_t i = 1; i < SMP_NUM; i++)
    {
        logger("\n");
#if HV
        int32_t result = smc_call(PSCI_0_2_FN64_CPU_ON, i, (uint64_t)(void *)second_entry_el2,
                              (uint64_t)(_stack_top_second - STACK_SIZE * (i - 1)));
        if (result != 0)
        {
            logger("smc_call failed!\n");
        }
#else
        int32_t result = hvc_call(PSCI_0_2_FN64_CPU_ON, i, (uint64_t)(void *)second_entry,
                              (uint64_t)(_stack_top_second - STACK_SIZE * (i - 1)));
#endif
        if (result != 0)
        {
            logger("hvc_call failed!\n");
        }

        // 做一点休眠 保证第二个核 初始化完成
        for (int32_t j = 0; j < 10; j++)
            for (int32_t k = 0; k < 0xfffff; k++)
                ;

        thread_info_init((struct thread_info *)(_stack_top_second - STACK_SIZE * i), 0, i);
        logger("core %d thread info addr: %llx\n", i, (void *)(_stack_top_second - STACK_SIZE * i));
    }
}
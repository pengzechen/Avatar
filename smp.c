
#include "smp.h"
#include "psci.h"
#include "os_cfg.h"
#include "io.h"
#include "thread.h"
#include "mem/aj_string.h"

extern void second_entry();
extern void _stack_top();
extern void _stack_top_second();

void thread_info_init(struct thread_info *ti, unsigned int flags, int id)
{
    memset(ti, 0, sizeof(struct thread_info));
    ti->cpu = id;
    ti->flags = flags;
}

void start_secondary_cpus()
{
    thread_info_init((struct thread_info *)(_stack_top - STACK_SIZE), 0, 0);
    printf("core 0 thread info addr: %x\n", (struct thread_into *)(void *)(_stack_top - STACK_SIZE));

    for (int i = 1; i < SMP_NUM; i++)
    {
        printf("\n");
        int result = hvc_call(PSCI_0_2_FN64_CPU_ON, i, (uint64_t)(void *)second_entry,
                              (uint64_t)(_stack_top_second - STACK_SIZE * (i - 1)));
        if (result != 0)
        {
            printf("smc_call failed!\n");
        }

        // 做一点休眠 保证第二个核 初始化完成
        for (int j = 0; j < 10; j++)
            for (int k = 0; k < 0xfffff; k++)
                ;
        
        thread_info_init((struct thread_info *)(_stack_top_second - STACK_SIZE * i), 0, i);
        printf("core %d thread info addr: %x\n", i, (void *)(_stack_top_second - STACK_SIZE * i));
    }
}
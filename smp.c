
#include "smp.h"
#include "psci.h"
#include "os_cfg.h"
#include "io.h"
#include "thread.h"
#include "lib/avatar_string.h"

extern void
second_entry();
extern void
second_entry_el2();
extern void
_stack_top();
extern void
_stack_bottom();
extern void
_stack_top_second();
extern void
_stack_bottom_second();

void
thread_info_init(struct thread_info *ti, uint32_t flags, int32_t id)
{
    memset(ti, 0, sizeof(struct thread_info));
    ti->cpu   = id;
    ti->flags = flags;
}

void
show_stack_layout(void)
{
    uint64_t total_secondary_stack = STACK_SIZE * (SMP_NUM - 1);

    logger_info("=== Stack Layout Information ===\n");
    logger("SMP_NUM: %d\n", SMP_NUM);
    logger("STACK_SIZE: %d bytes (%d KB)\n", STACK_SIZE, STACK_SIZE / 1024);
    logger("Primary CPU stack: [%llx - %llx]\n", (uint64_t) _stack_bottom, (uint64_t) _stack_top);

    if (SMP_NUM > 1) {
        logger("Secondary stack pool: [%llx - %llx] (%llu bytes)\n",
               (uint64_t) _stack_bottom_second,
               (uint64_t) _stack_top_second,
               total_secondary_stack);
        logger("Max supported CPUs: %d\n", SMP_NUM);

        for (int i = 1; i < SMP_NUM; i++) {
            uint64_t stack_top    = (uint64_t) _stack_top_second - STACK_SIZE * (i - 1);
            uint64_t stack_bottom = stack_top - STACK_SIZE;
            logger("  CPU %d stack: [%llx - %llx]\n", i, stack_bottom, stack_top);
        }
    }
    logger_info("================================\n\n");
}

void
start_secondary_cpus()
{
    // 显示栈布局信息
    show_stack_layout();

    // 初始化主核(CPU 0)
    thread_info_init((struct thread_info *) (_stack_top - STACK_SIZE), 0, 0);
    logger("core 0 thread info addr: %llx\n",
           (struct thread_into *) (void *) (_stack_top - STACK_SIZE));

    // 检查SMP配置
    if (SMP_NUM <= 1) {
        logger("SMP disabled (SMP_NUM=%d), skipping secondary CPU startup\n", SMP_NUM);
        return;
    }

    logger("Starting %d secondary CPUs...\n", SMP_NUM - 1);

    for (int32_t i = 1; i < SMP_NUM; i++) {
        // 计算从核栈地址 - 每个从核分配一个独立的栈
        uint64_t secondary_stack_top    = (uint64_t) _stack_top_second - STACK_SIZE * (i - 1);
        uint64_t secondary_stack_bottom = secondary_stack_top - STACK_SIZE;

        // 栈边界检查 - 确保不会超出分配的栈空间
        uint64_t available_stack_space =
            (uint64_t) _stack_top_second - (uint64_t) _stack_bottom_second;
        uint64_t required_stack_space = STACK_SIZE * i;

        if (required_stack_space > available_stack_space) {
            logger("ERROR: CPU %d stack overflow!\n", i);
            logger("       Required: %llu bytes, Available: %llu bytes\n",
                   required_stack_space,
                   available_stack_space);
            logger("       Max supported CPUs with current stack: %llu\n",
                   available_stack_space / STACK_SIZE + 1);
            continue;
        }

        logger("Starting CPU %d, stack: [%llx - %llx]\n",
               i,
               secondary_stack_bottom,
               secondary_stack_top);

#if HV
        int32_t result = smc_call(PSCI_0_2_FN64_CPU_ON,
                                  i,
                                  (uint64_t) (void *) second_entry_el2,
                                  secondary_stack_top);
        if (result != 0) {
            logger("ERROR: smc_call failed for CPU %d, result: %d\n", i, result);
            continue;
        }
#else
        int32_t result = hvc_call(PSCI_0_2_FN64_CPU_ON,
                                  i,
                                  (uint64_t) (void *) second_entry,
                                  secondary_stack_top);
        if (result != 0) {
            logger("ERROR: hvc_call failed for CPU %d, result: %d\n", i, result);
            continue;
        }
#endif

        // 等待从核启动完成
        for (int32_t j = 0; j < CPU_STARTUP_WAIT_LOOPS; j++)
            for (int32_t k = 0; k < CPU_STARTUP_INNER_LOOPS; k++)
                ;

        // 初始化从核的thread info
        thread_info_init((struct thread_info *) (secondary_stack_top - STACK_SIZE), 0, i);
        logger("CPU %d started successfully, thread info addr: %llx\n",
               i,
               (void *) (secondary_stack_top - STACK_SIZE));
    }

    logger("Secondary CPU startup complete: %d/%d CPUs active\n", SMP_NUM, SMP_NUM);
}
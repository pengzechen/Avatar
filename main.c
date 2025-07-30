
#include "aj_types.h"
#include "io.h"
#include "smp.h"
#include "psci.h"
#include "gic.h"
#include "timer.h"
#include "thread.h"
#include "task/task.h"
#include "spinlock.h"
#include "uart_pl011.h"
#include "lib/aj_string.h"
#include "mem/mem.h"
#include "app/app.h"
#include "pro.h"
#include "ramfs.h"

void test_mem()
{
    uint32_t mask = 97;
    void *addr = (void *)0x9000000;
    logger("addr: 0x%llx\n", addr);
    logger("before value: 0x%llx\n", *(const volatile uint32_t *)((addr)));
    *(volatile uint32_t *)addr = mask;
    logger("after  value: 0x%llx\n", *(const volatile uint32_t *)((addr)));

    while (1)
        ;
}

void test_types()
{
    logger("sizeof (uint32_t): %d\n", sizeof(uint32_t));
    logger("sizeof (uint64_t): %d\n", sizeof(uint64_t));
    logger("sizeof int: %d\n", sizeof(int));

    logger("sizeof char: %d\n", sizeof(char));
    logger("sizeof short: %d\n", sizeof(short));
    logger("sizeof long: %d\n", sizeof(long));

    logger("sizeof void: %d\n", sizeof(void));
    logger("sizeof void * %d\n", sizeof(void *));

    while (1)
        ;
}

int inited_cpu_num = 0;
spinlock_t lock;

void main_entry()
{
    logger("main entry: get_current_cpu_id: %d\n", get_current_cpu_id());
    if (get_current_cpu_id() == 0)
    {
        alloctor_init();
        // kmem_test();
        schedule_init();
        task_manager_init();
        // ramfs_test();

        process_t *pro1 = alloc_process("system");
        process_init(pro1, __testapp_bin_start, 1); // 将来替换 add
        run_process(pro1);

        // process_t * pro2 = alloc_process("sub");
        // process_init(pro2, __sub_bin_start, 2);
        // run_process(pro2);

        print_current_task_list();
    }
    el1_idle_init(); // idle 任务每个核都有自己的el1栈， 代码公用
    spin_lock(&lock);
    inited_cpu_num++;
    spin_unlock(&lock);

    while (inited_cpu_num != SMP_NUM)
        wfi();

    // uint64_t __sp = (uint64_t)app_el1_stack + 4096 - sizeof(trap_frame_t);
    // void * _sp = (void *)__sp;
    // schedule_init_local(task1, _sp);  // 任务管理器任务当前在跑第一个任务

    // asm volatile("mov sp, %0" :: "r"(_sp));
    // extern void el0_task_entry();
    // el0_task_entry();

    uint64_t __sp = get_idle_sp_top() - sizeof(trap_frame_t);
    void *_sp = (void *)__sp;
    schedule_init_local(get_idle(), NULL); // 任务管理器任务当前在跑idle任务

    asm volatile("mov sp, %0" ::"r"(_sp));
    extern void el0_task_entry();
    el0_task_entry();

    // int x;
    // while (1)
    // {
    //     for(int i = 0; i < 100000000; i++);
    //     x++;
    //     int y = get_current_cpu_id();
    //     logger("cpu %d running %d\n", y, x);
    // }
}

void kernel_main(void)
{
    logger_info("starting primary core 0 ...\n");
    io_early_init();
    gic_init();

    timer_init();
    logger_info("core 0 starting is done.\n\n");
    spinlock_init(&lock);
    // io_init();

    start_secondary_cpus();

    main_entry();
    // can't reach here !
}

void second_kernel_main()
{
    logger_info("starting core");
    logger(" %d ", get_current_cpu_id());
    logger_info("...\n");

    // 第二个核要初始化 gicc
    gicc_init();
    // 输出当前 gic 初始化情况
    gic_test_init();
    // 第二个核要初始化 timer
    timer_init_second();

    logger_info("core");
    logger(" %d ", get_current_cpu_id());
    logger_info("starting is done.\n\n");

    main_entry();
    // can't reach here !
}

// 100100 10000000000000000001001101
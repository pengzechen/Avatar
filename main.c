
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
#include "mem/aj_string.h"
#include "mem/mem.h"
#include "app/app.h"

void test_mem()
{
    uint32_t mask = 97;
    void *addr = (void *)0x9000000;
    printf("addr: 0x%x\n", addr);
    printf("before value: 0x%x\n", *(const volatile uint32_t *)((addr)));
    *(volatile uint32_t *)addr = mask;
    printf("after  value: 0x%x\n", *(const volatile uint32_t *)((addr)));

    while (1)
        ;
}

void test_types()
{
    printf("sizeof (uint32_t): %d\n", sizeof(uint32_t));
    printf("sizeof (uint64_t): %d\n", sizeof(uint64_t));
    printf("sizeof int: %d\n", sizeof(int));

    printf("sizeof char: %d\n", sizeof(char));
    printf("sizeof short: %d\n", sizeof(short));
    printf("sizeof long: %d\n", sizeof(long));

    printf("sizeof void: %d\n", sizeof(void));
    printf("sizeof void * %d\n", sizeof(void *));

    while (1)
        ;
}


int inited_cpu_num = 0;
spinlock_t lock;

void copy_app_shell(void)
{
    size_t size = (size_t)(__shell_bin_end - __shell_bin_start);
    unsigned long *from = (unsigned long *)__shell_bin_start;
    unsigned long *to = (unsigned long *)0x80000000;
    printf("Copy app image from %x to %x (%d bytes): 0x%x / 0x%x\n",
           from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}

void copy_app_testapp(void)
{
    size_t size = (size_t)(__testapp_bin_end - __testapp_bin_start);
    unsigned long *from = (unsigned long *)__testapp_bin_start;
    unsigned long *to = (unsigned long *)0x90000000;
    printf("Copy app image from %x to %x (%d bytes): 0x%x / 0x%x\n"
           , from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}

static uint8_t  app_el1_stack[4096] __attribute__((aligned(16384)));
static uint8_t  app_el2_stack[4096] __attribute__((aligned(4096)));



void main_entry()
{
    printf("main entry: get_current_cpu_id: %d\n", get_current_cpu_id());
    tcb_t * task1 = NULL;
    tcb_t * task2 = NULL;
    memset(app_el1_stack, 0, sizeof(app_el1_stack));
    memset(app_el2_stack, 0, sizeof(app_el2_stack));
    if (get_current_cpu_id() == 0)
    {
        alloctor_init();
        copy_app_shell();
        copy_app_testapp();
        schedule_init();
        task_manager_init();
        
        task1 = create_task((void*)0x80000000, ((uint64_t)app_el1_stack + 4096), 1);
        task2 = create_task((void*)0x90000000, ((uint64_t)app_el2_stack + 4096), 2);
        
        print_current_task_list();
        task_set_ready(task1);
        task_set_ready(task2);
    }
    el1_idle_init();        // idle 任务每个核都有自己的el1栈， 代码公用
    spin_lock(&lock);
    inited_cpu_num ++;
    spin_unlock(&lock);

    while(inited_cpu_num != SMP_NUM)
        wfi();
    
    // uint64_t __sp = (uint64_t)app_el1_stack + 4096 - sizeof(trap_frame_t);
    // void * _sp = (void *)__sp;
    // schedule_init_local(task1, _sp);  // 任务管理器任务当前在跑第一个任务
    
    // asm volatile("mov sp, %0" :: "r"(_sp));
    // extern void el0_tesk_entry();
    // el0_tesk_entry();

    uint64_t __sp = get_idle_sp_top() - sizeof(trap_frame_t);
    void * _sp = (void *)__sp;
    schedule_init_local(get_idle(), NULL);  // 任务管理器任务当前在跑idle任务
    
    asm volatile("mov sp, %0" :: "r"(_sp));
    extern void el0_tesk_entry();
    el0_tesk_entry();
    
    // int x;
    // while (1)
    // {
    //     for(int i = 0; i < 100000000; i++);
    //     x++;
    //     int y = get_current_cpu_id();
    //     printf("cpu %d running %d\n", y, x);
    // }
}

void kernel_main(void)
{
    print_info("starting primary core 0 ...\n");
    io_early_init();
    gic_init();

    timer_init();
    print_info("core 0 starting is done.\n\n");
    spinlock_init(&lock);
    // io_init();

    start_secondary_cpus();

    main_entry();
    // can't reach here !
}

void second_kernel_main()
{
    print_info("starting core");
    printf(" %d ", get_current_cpu_id());
    print_info("...\n");

    // 第二个核要初始化 gicc
    gicc_init();
    // 输出当前 gic 初始化情况
    gic_test_init();
    // 第二个核要初始化 timer
    timer_init_second();

    print_info("core");
    printf(" %d ", get_current_cpu_id());
    print_info("starting is done.\n\n");

    main_entry();
    // can't reach here !
}

// 100100 10000000000000000001001101
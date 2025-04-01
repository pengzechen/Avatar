
#include "aj_types.h"
#include "io.h"
#include "smp.h"
#include "psci.h"
#include "gic.h"
#include "timer.h"
#include "thread.h"
#include "task.h"
#include "spinlock.h"
#include "uart_pl011.h"
#include "mem/aj_string.h"



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

extern void __app1_bin_start();
extern void __app1_bin_end();
extern void __app2_bin_start();
extern void __app2_bin_end();

void copy_app(void)
{
    size_t size = (size_t)(__app1_bin_end - __app1_bin_start);
    unsigned long *from = (unsigned long *)__app1_bin_start;
    unsigned long *to = (unsigned long *)0x80000000;
    printf("Copy app image from %x to %x (%d bytes): 0x%x / 0x%x\n",
           from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}
void copy_app2(void)
{
    size_t size = (size_t)(__app2_bin_end - __app2_bin_start);
    unsigned long *from = (unsigned long *)__app2_bin_start;
    unsigned long *to = (unsigned long *)0x90000000;
    printf("Copy app image from %x to %x (%d bytes): 0x%x / 0x%x\n"
           , from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}
void copy_app3(void)
{
    size_t size = (size_t)(__app2_bin_end - __app2_bin_start);
    unsigned long *from = (unsigned long *)__app2_bin_start;
    unsigned long *to = (unsigned long *)0x83000000;
    printf("Copy app image from %x to %x (%d bytes): 0x%x / 0x%x\n"
           , from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}
void copy_app4(void)
{
    size_t size = (size_t)(__app2_bin_end - __app2_bin_start);
    unsigned long *from = (unsigned long *)__app2_bin_start;
    unsigned long *to = (unsigned long *)0x84000000;
    printf("Copy app image from %x to %x (%d bytes): 0x%x / 0x%x\n"
           , from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}
void copy_app5(void)
{
    size_t size = (size_t)(__app2_bin_end - __app2_bin_start);
    unsigned long *from = (unsigned long *)__app2_bin_start;
    unsigned long *to = (unsigned long *)0x85000000;
    printf("Copy app image from %x to %x (%d bytes): 0x%x / 0x%x\n"
           , from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}



void main_entry()
{
    printf("main entry: get_current_cpu_id: %d\n", get_current_cpu_id());
    
    if (get_current_cpu_id() == 0)
    {
        copy_app();
        copy_app2();
        // copy_app3();
        // copy_app4();
        // copy_app5();
        schedule_init();

        create_task((void*)0x80000000, (void*)(0x80001000));
        create_task((void*)0x90000000, (void*)(0x90001000));
        // create_task((void*)0x83000000, (void*)(0x83001000));
        // create_task((void*)0x84000000, (void*)(0x84001000));
        // create_task((void*)0x85000000, (void*)(0x85001000));
        
        print_current_task_list();
    }
    spin_lock(&lock);
    inited_cpu_num ++;
    spin_unlock(&lock);

    while(inited_cpu_num != SMP_NUM)
        wfi();

    schedule_init_local();
    enable_interrupts();
    
    while (1)
        ;
}

void kernel_main(void)
{
    print_info("starting primary core 0 ...\n");
    io_early_init();
    gic_init();

    *(int*)(void*)0x8000404 = 0x1;
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
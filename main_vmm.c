

#include "io.h"
#include "gic.h"
#include "timer.h"
#include "mem/mmu.h"
#include "sys/vtcr.h"
#include "mem/stage2page.h"
#include "mem/page.h"
#include "task/task.h"
#include "lib/avatar_string.h"
#include "vmm/vm.h"
#include "vmm/vtimer.h"
#include "vmm/vpl011.h"
#include "os_cfg.h"
#include "thread.h"
#include "mem/mem.h"
#include "smp.h"
#include "uart_pl011.h"

void print_avatar_logo(void)
{

    logger_info("\n");
    logger_info("                    _             \n");
    logger_info("     /\\            | |            \n");
    logger_info("    /  \\__   ____ _| |_ __ _ _ __ \n");
    logger_info("   / /\\ \\ \\ / / _` | __/ _` | '__|\n");
    logger_info("  / ____ \\ V / (_| | || (_| | |   \n");
    logger_info(" /_/    \\_\\_/ \\__,_|\\__\\__,_|_|   \n");
    logger_info("                                  \n");
    logger_info("                                            \n");
    logger_info("-- Lightweight Virtualization Core --       \n");
    logger_info("-- Version: 0.1.0     \n");
    logger_info("-- Build: %s %s \n", __DATE__, __TIME__);
    logger_info("\n");
}

void vtcr_init(void)
{
    logger_info("Initialize vtcr...\n");
    uint64_t vtcr_val = VTCR_VS_8BIT | VTCR_PS_MASK_36_BITS |
                        VTCR_TG0_4K | VTCR_SH0_IS | VTCR_ORGN0_WBWA | VTCR_IRGN0_WBWA;

    vtcr_val |= VTCR_T0SZ(64 - 32); /* 32 bit IPA */
    vtcr_val |= VTCR_SL0(0x1);      /* P2M starts at first level */

    logger("vtcr val: 0x%llx\n", vtcr_val);
    write_vtcr_el2(vtcr_val);
}

void vtimer_init_el2(void)
{
    logger_info("Initialize virtual timer offset...\n");

    // 初始化 CNTVOFF_EL2 为 0，这样 Guest 看到的虚拟时间就等于物理时间
    // Guest 读取 CNTVCT_EL0 时看到的是：CNTPCT_EL0 - CNTVOFF_EL2
    write_cntvoff_el2(0);

    uint64_t cntvoff = read_cntvoff_el2();
    logger_info("CNTVOFF_EL2 initialized to: 0x%llx\n", cntvoff);
}

extern size_t cacheline_bytes;
int32_t inited_cpu_num_el2 = 0;
spinlock_t lock_el2;

void main_entry_el2()
{
    spin_lock(&lock_el2);
    inited_cpu_num_el2++;
    spin_unlock(&lock_el2);

    while (inited_cpu_num_el2 != SMP_NUM)
        wfi();
    
    logger("main entry: get_current_cpu_id: %d\n", get_current_cpu_id());

    vtcr_init();
    vtimer_init_el2();
    guest_ept_init();

    if (get_current_cpu_id() == 0)
    {
        schedule_init();
        alloctor_init();
        logger_info("cacheline_bytes: %d\n", cacheline_bytes);
        task_manager_init();

        // 初始化虚拟化组件
        vtimer_global_init();
        vpl011_global_init();

        struct _vm_t *vm = alloc_vm();
        if (vm == NULL)
        {
            logger_error("Failed to allocate vm\n");
            return;
        }
        vm_init(vm, 0); // 初始化一个虚拟机
        run_vm(vm);

        // vm = alloc_vm();
        // if (vm == NULL)
        // {
        //     logger_error("Failed to allocate vm\n");
        //     return;
        // }
        // vm_init(vm, 1); // 初始化第二个虚拟机
        // run_vm(vm);

        logger("\nHello VMM:\nthere's some vmm tests: \n");
        logger("scrlr_el2: 0x%llx\n", read_sctlr_el2());
        logger("hcr_el2: 0x%llx\n", read_hcr_el2());
        logger("read_vttbr_el2: 0x%llx\n", read_vttbr_el2());
        logger("\n");

        print_current_task_list();
    }

    el2_idle_init(); // idle 任务每个核都有自己的el1栈， 代码公用
    
    uint64_t __sp = get_idle_sp_top() - sizeof(trap_frame_t);
    void *_sp = (void *)__sp;
    schedule_init_local(get_idle(), NULL); // 任务管理器任务当前在跑idle任务

    enable_interrupts();

    // while(1) {
    //     if (uart_rx_available()) {
    //         char c;
    //         while (uart_getchar_nb(&c)) {
    //             logger_info("(cpu: %d) Received char: '%c' (0x%02x)\n", get_current_cpu_id(), c, (unsigned char)c);
    //         }
    //     }
    //     // 让出CPU给其他任务
    //     wfi();
    // }

    asm volatile("mov sp, %0" ::"r"(_sp));
    extern void guest_entry();
    guest_entry();
}

void vmm_main()
{
    io_early_init();
    // run_printf_tests();
    logger_info("starting primary core 0 ...\n");
    
    print_avatar_logo();

    gic_virtual_init();
    timer_init();
    spinlock_init(&lock_el2);

    io_init();
    
    logger_info("core 0 starting is done.\n\n");

    start_secondary_cpus();
    main_entry_el2();
    // can't reach here !
}

void second_kernel_main_el2()
{
    // 在 EL2 中的第二个内核入口
    logger_info("starting core");
    logger(" %d ", get_current_cpu_id());
    logger_info("...\n");

    // 第二个核要初始化 gicc
    gicc_el2_init();
    // 输出当前 gic 初始化情况
    gic_test_init();
    // 第二个核要初始化 timer
    timer_init_second();

    logger_info("core");
    logger(" %d ", get_current_cpu_id());
    logger_info("starting is done.\n\n");

    main_entry_el2();
    // can't reach here !
}
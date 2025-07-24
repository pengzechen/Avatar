

#include "io.h"
#include "gic.h"
#include "timer.h"
#include "mem/mmu.h"
#include "sys/vtcr.h"
#include "mem/ept.h"
#include "mem/page.h"
#include "task/task.h"
#include "lib/aj_string.h"
#include "hyper/vm.h"
#include "os_cfg.h"
#include "thread.h"
#include "mem/mem.h"
#include "smp.h"

void test_mem_hypervisor()
{
    lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_ARREA);
    avr_entry->p2m.read = 0;
    avr_entry->p2m.write = 0;
    apply_ept(avr_entry);
    *(uint64_t *)0x50000000 = 0x1234;
}

static inline uint64_t read_sctlr_el2()
{
    uint64_t value;
    asm volatile(
        "mrs %0, sctlr_el2"
        : "=r"(value));
    return value;
}

static inline uint64_t read_hcr_el2()
{
    uint64_t value;
    asm volatile(
        "mrs %0, hcr_el2"
        : "=r"(value));
    return value;
}

static inline uint64_t read_vttbr_el2()
{
    uint64_t value;
    asm volatile(
        "mrs %0, vttbr_el2"
        : "=r"(value));
    return value;
}

void vtcr_init(void)
{
    logger_info("    Initialize vtcr...\n");
    uint64_t vtcr_val = VTCR_VS_8BIT | VTCR_PS_MASK_36_BITS |
                        VTCR_TG0_4K | VTCR_SH0_IS | VTCR_ORGN0_WBWA | VTCR_IRGN0_WBWA;

    vtcr_val |= VTCR_T0SZ(64 - 32); /* 32 bit IPA */
    vtcr_val |= VTCR_SL0(0x1);      /* P2M starts at first level */

    logger("vtcr val: 0x%llx\n", vtcr_val);
    write_vtcr_el2(vtcr_val);
}

static void guest_trap_init(void)
{
    logger_info("    Initialize trap...\n");
    unsigned long hcr;
    hcr = read_hcr_el2();
    // WRITE_SYSREG(hcr | HCR_TGE, HCR_EL2);
    // hcr = READ_SYSREG(HCR_EL2);
    logger("HCR : 0x%llx\n", hcr);
    isb();
}

extern void __guset_bin_start();
extern void __guset_bin_end();
extern void __guset_dtb_start();
extern void __guset_dtb_end();
extern void __guset_fs_start();
extern void __guset_fs_end();

extern void test_guest();

extern size_t cacheline_bytes;
int inited_cpu_num_el2 = 0;
spinlock_t lock_el2;

static uint8_t guest1_el2_stack[8192] __attribute__((aligned(16384)));
static uint8_t guest2_el2_stack[8192] __attribute__((aligned(8192)));

void copy_guest(void)
{
    size_t size = (size_t)(__guset_bin_end - __guset_bin_start);
    unsigned long *from = (unsigned long *)__guset_bin_start;
    unsigned long *to = (unsigned long *)GUEST_KERNEL_START;
    logger("Copy guest kernel image from %llx to %llx (%d bytes): 0x%llx / 0x%llx\n",
           from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    logger("Copy end : 0x%llx / 0x%llx\n", to[0], to[1]);
}

void copy_dtb(void)
{
    size_t size = (size_t)(__guset_dtb_end - __guset_dtb_start);
    unsigned long *from = (unsigned long *)__guset_dtb_start;
    unsigned long *to = (unsigned long *)GUEST_DTB_START;
    logger("Copy guest dtb from %llx to %llx (%d bytes): 0x%llx / 0x%llx\n",
           from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    logger("Copy end : 0x%llx / 0x%llx\n", to[0], to[1]);
}

void copy_fs(void)
{
    size_t size = (size_t)(__guset_fs_end - __guset_fs_start);
    unsigned long *from = (unsigned long *)__guset_fs_start;
    unsigned long *to = (unsigned long *)GUEST_FS_START;
    logger("Copy guest fs from %llx to %llx (%d bytes): 0x%llx / 0x%llx\n",
           from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    logger("Copy end : 0x%llx / 0x%llx\n", to[0], to[1]);
}

void mmio_map_gicd()
{
    for (int i = 0; i < 16; i++)
    {
        lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_AREA_GICD + 0x1000 * i); // 0800 0000 - 0801 0000  gicd
        avr_entry->p2m.read = 0;
        avr_entry->p2m.write = 0;
        apply_ept(avr_entry);
    }
}

void mmio_map_gicc()
{
    for (int i = 0; i < 16; i++)
    {
        lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_AREA_GICC + 0x1000 * i); // 0800 0000 - 0801 0000  gicd
        avr_entry->p2m.read = 0;
        avr_entry->p2m.write = 0;
        apply_ept(avr_entry);
    }
}


void main_entry_el2()
{
    logger("main entry: get_current_cpu_id: %d\n", get_current_cpu_id());

    vtcr_init();
    guest_ept_init();

    if (get_current_cpu_id() == 0)
    {

        // 准备启动 guest ...
        guest_trap_init();
        copy_dtb();
        copy_guest();
        copy_fs();
        mmio_map_gicd();
        mmio_map_gicc();
        vm_init();

        logger("\nHello Hyper:\nthere's some hyper tests: \n");
        logger("scrlr_el2: 0x%llx\n", read_sctlr_el2());
        logger("hcr_el2: 0x%llx\n", read_hcr_el2());
        logger("read_vttbr_el2: 0x%llx\n", read_vttbr_el2());
        logger("\n");

        schedule_init(); // 设置当前 task 为 task0（test_guest）
        alloctor_init();
        task_manager_init();

        tcb_t *task1 = craete_vm_task(test_guest, (uint64_t)guest1_el2_stack + 8192, (1 << 0));
        tcb_t *task2 = craete_vm_task((void *)GUEST_KERNEL_START, (uint64_t)guest2_el2_stack + 8192, (1 << 0));

        task_set_ready(task1);
        task_set_ready(task2);

        print_current_task_list();
    }

    // *(uint64_t*)0x8040004 = 0x1; // 测试写入 MMIO 区域

    el2_idle_init(); // idle 任务每个核都有自己的el1栈， 代码公用
    spin_lock(&lock_el2);
    inited_cpu_num_el2++;
    spin_unlock(&lock_el2);

    while (inited_cpu_num_el2 != SMP_NUM)
        wfi();

    // uint64_t __sp = (uint64_t)guest1_el2_stack + 8192 - sizeof(trap_frame_t);
    // void * _sp = (void *)__sp;
    // schedule_init_local(task1, _sp);  // 任务管理器任务当前在跑第一个任务

    // asm volatile("mov sp, %0" :: "r"(_sp));
    // extern void guest_entry();
    // guest_entry();

    uint64_t __sp = get_idle_sp_top() - sizeof(trap_frame_t);
    void *_sp = (void *)__sp;
    schedule_init_local(get_idle(), NULL); // 任务管理器任务当前在跑idle任务

    asm volatile("mov sp, %0" ::"r"(_sp));
    extern void guest_entry();
    guest_entry();
}

void hyper_main()
{
    run_printf_tests();
    logger_info("starting primary core 0 ...\n");
    io_early_init();
    gic_virtual_init();
    timer_init();
    logger("cacheline_bytes: %d\n", cacheline_bytes);
    logger_info("core 0 starting is done.\n\n");

    spinlock_init(&lock_el2);
    // io_init();

    start_secondary_cpus();

    main_entry_el2();
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
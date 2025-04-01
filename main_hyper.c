

#include "io.h"
#include "gic.h"
#include "timer.h"
#include "mem/mmu.h"
#include "sys/vtcr.h"
#include "mem/ept.h"
#include "mem/page.h"
#include "task.h"
#include "mem/aj_string.h"
#include "hyper/vm.h"
#include "os_cfg.h"

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
    print_info("    Initialize vtcr...\n");
    uint64_t vtcr_val = VTCR_VS_8BIT | VTCR_PS_MASK_36_BITS |
                        VTCR_TG0_4K | VTCR_SH0_IS | VTCR_ORGN0_WBWA | VTCR_IRGN0_WBWA;

    vtcr_val |= VTCR_T0SZ(64 - 32); /* 32 bit IPA */
    vtcr_val |= VTCR_SL0(0x1);      /* P2M starts at first level */

    printf("vtcr val: 0x%x\n", vtcr_val);
    write_vtcr_el2(vtcr_val);
}

static void guest_trap_init(void)
{
    print_info("    Initialize trap...\n");
    unsigned long hcr;
    hcr = read_hcr_el2();
    // WRITE_SYSREG(hcr | HCR_TGE, HCR_EL2);
    // hcr = READ_SYSREG(HCR_EL2);
    printf("HCR : 0x%x\n", hcr);
    isb();
}

extern void __guset_bin_start();
extern void __guset_bin_end();
extern void __guset_dtb_start();
extern void __guset_dtb_end();
extern void __guset_fs_start();
extern void __guset_fs_end();

void copy_guest(void)
{
    size_t size = (size_t)(__guset_bin_end - __guset_bin_start);
    unsigned long *from = (unsigned long *)__guset_bin_start;
    unsigned long *to = (unsigned long *)GUEST_KERNEL_START;
    printf("Copy guest kernel image from %x to %x (%d bytes): 0x%x / 0x%x\n",
           from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}

void copy_dtb(void)
{
    size_t size = (size_t)(__guset_dtb_end - __guset_dtb_start);
    unsigned long *from = (unsigned long *)__guset_dtb_start;
    unsigned long *to = (unsigned long *)GUEST_DTB_START;
    printf("Copy guest dtb from %x to %x (%d bytes): 0x%x / 0x%x\n",
           from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}

void copy_fs(void)
{
    size_t size = (size_t)(__guset_fs_end - __guset_fs_start);
    unsigned long *from = (unsigned long *)__guset_fs_start;
    unsigned long *to = (unsigned long *)GUEST_FS_START;
    printf("Copy guest fs from %x to %x (%d bytes): 0x%x / 0x%x\n",
           from, to, size, from[0], from[1]);
    memcpy(to, from, size);
    printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
}

extern void test_guest();
extern void guest_start();

extern size_t cacheline_bytes;

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

void hyper_main()
{

    io_early_init();
    gic_virtual_init();
    timer_init();
    printf("io, gic, timer, init ok...\n\n");

    // 准备启动 guest ...
    vtcr_init();
    guest_ept_init();
    guest_trap_init();
    copy_dtb();
    copy_guest();
    copy_fs();
    mmio_map_gicd();
    mmio_map_gicc();
    vm_init();

    printf("\nHello Hyper:\nthere's some hyper tests: \n");
    printf("scrlr_el2: 0x%x\n", read_sctlr_el2());
    printf("hcr_el2: 0x%x\n", read_hcr_el2());
    printf("read_vttbr_el2: 0x%x\n", read_vttbr_el2());
    printf("cacheline_bytes: %d\n", cacheline_bytes);
    printf("\n");

    lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_ARREA);
    avr_entry->p2m.read = 0;
    avr_entry->p2m.write = 0;
    apply_ept(avr_entry);
    *(uint64_t *)0x50000000 = 0x1234;

    schedule_init(); // 设置当前 task 为 task0（test_guest）
    craete_vm_task(test_guest);
    craete_vm_task((void *)GUEST_KERNEL_START);
    
    schedule_init_local();
    print_current_task_list();

    guest_start();

    while (1)
        ;
}

#include <hyper/vgic.h>
#include <hyper/hyper_cfg.h>
#include <aj_types.h>
#include "gic.h"
#include "mem/page.h"
#include "lib/aj_string.h"
#include "mem/ept.h"
#include "io.h"
#include "hyper/vm.h"
#include "mem/mem.h"
#include "task/task.h"

// qemu 准备启动4个核
// 每个核跑两个 vcpu， 共8个vcpu
// 跑4个vm每个vm使用2个vcpu 或者 跑2个vm每个vm使用4个vcpu

static struct vm_t vms[VM_NUM_MAX];
static uint32_t vm_num = 0;

#define HV_TIMER_VECTOR 27
#define PL011_INT 33

void v_timer_handler()
{
    // logger("v_timer_handler\n");
    vgic_inject(HV_TIMER_VECTOR);
}

void fake_console()
{
    // logger("fake console\n");
    vgic_inject(PL011_INT);
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

void test_mem_hypervisor()
{
    lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_ARREA);
    avr_entry->p2m.read = 0;
    avr_entry->p2m.write = 0;
    apply_ept(avr_entry);
    *(uint64_t *)0x50000000 = 0x1234;
}

// 一个vm必定有多个vcpu，一个vgic。 先在这里初始化
struct vm_t *alloc_vm()
{
    if (vm_num >= VM_NUM_MAX)
    {
        logger_error("No more vm can be allocated!\n");
        return NULL;
    }
    struct vm_t *vm = &vms[vm_num++];
    memset(vm, 0, sizeof(struct vm_t));
    vm->id = vm_num - 1;

    // 初始化 vcpu 列表
    list_init(&vm->vpus);

    // 获取对应的 vgic 结构体
    vm->vgic = get_vgic(vm->id);

    logger_info("alloc vm: %d\n", vm->id);
    return vm;
}

// 首核以外的核先跑这个。
extern void test_guest();

static void guest_trap_init(void)
{
    logger_info("    Initialize trap...\n");
    unsigned long hcr;
    hcr = read_hcr_el2();
    // WRITE_SYSREG(hcr | HCR_TGE, HCR_EL2);
    // hcr = READ_SYSREG(HCR_EL2);
    logger_warn("HCR : 0x%llx\n", hcr);
    isb();
}

// 初始化虚拟机
void vm_init(struct vm_t *vm, int vcpu_num)
{
    //(1) 设置hcr
    guest_trap_init();

    //(2) 设置ttbr,和vtcr
    // 这里在main_hyper.c中已经设置了

    //(3) 分配vcpu
    // 创建两个 guest 任务

    //(3.1) 首核
    void *stack = kalloc_pages(2);
    if (stack == NULL)
    {
        logger_error("Failed to allocate stack for primary vcpu \n");
        return;
    }
    tcb_t *task = craete_vm_task((void *)GUEST_KERNEL_START, (uint64_t)stack + 8192, (1 << 0));
    if (task == NULL)
    {
        logger_error("Failed to create vcpu task\n");
        return;
    }
    list_insert_last(&vm->vpus, &task->vm_node);
    task->vm = vm;           // 设置当前虚拟机
    task->cpu_info->sys_reg->mpidr_el1 = (1ULL << 31) | (uint64_t)(0 & 0xff);
    vm->primary_vcpu = task; // 设置主 vcpu

    //(3.2) 其它核
    for (int i = 1; i < vcpu_num; i++)
    {
        void *stack = kalloc_pages(2);
        if (stack == NULL)
        {
            logger_error("Failed to allocate stack for vcpu %d\n", i);
            return;
        }
        tcb_t *task = craete_vm_task(test_guest, (uint64_t)stack + 8192, (1 << 1));
        if (task == NULL)
        {
            logger_error("Failed to create vcpu task %d\n", i);
            return;
        }
        list_insert_last(&vm->vpus, &task->vm_node);
        task->vm = vm; // 设置当前虚拟机

        task->cpu_info->sys_reg->mpidr_el1 = (1ULL << 31) | (uint64_t)(i & 0xff);

        // dev use
        // task_set_ready(task);
    }

    // test
    list_node_t *iter = list_first(&vm->vpus);
    tcb_t *taskt = NULL;
    while (iter)
    {
        taskt = list_node_parent(iter, tcb_t, vm_node);
        logger_info("vcpu task id: %d, mpidr_el1: 0x%x\n", taskt->id, taskt->cpu_info->sys_reg->mpidr_el1);
        
        iter = list_node_next(iter);
    }

    // 拷贝guest镜像
    copy_dtb();
    copy_guest();
    copy_fs();

    // 映射 MMIO 区域
    mmio_map_gicd();
    mmio_map_gicc();

    // 初始化虚拟 GIC （未完善）
    virtual_gic_register_int(get_vgic(0), HV_TIMER_VECTOR, HV_TIMER_VECTOR);

    virtual_gic_register_int(get_vgic(0), PL011_INT, PL011_INT);

    irq_install(HV_TIMER_VECTOR, v_timer_handler);

    irq_install(PL011_INT, fake_console);
}

void run_vm(struct vm_t *vm)
{
    if (vm == NULL)
    {
        logger_error("run_vm: vm is NULL\n");
        return;
    }

    task_set_ready(vm->primary_vcpu);
}
/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file vm.c
 * @brief Implementation of vm.c
 * @author Avatar Project Team
 * @date 2024
 */


#include "vmm/vgic.h"
#include "vmm/vtimer.h"
#include "vmm/vpl011.h"
#include "vmm/vmm_cfg.h"
#include "avatar_types.h"
#include "os_cfg.h"
#include "gic.h"
#include "mem/page.h"
#include "lib/avatar_string.h"
#include "mem/stage2page.h"
#include "io.h"
#include "vmm/vm.h"
#include "mem/mem.h"
#include "task/task.h"

#include "../guest/guest_manifest.h"
#include "vmm/guest_loader.h"
#include "mmio.h"


// qemu 准备启动4个核
// 每个核跑两个 vcpu， 共8个vcpu
// 跑4个vm每个vm使用2个vcpu 或者 跑2个vm每个vm使用4个vcpu

static struct _vm_t vms[VM_NUM_MAX];
static uint32_t     vm_num = 0;

void
fake_timer()
{
    // logger("fake timer\n");
    vgic_hw_inject_test(TIMER_VECTOR);
}

void
fake_console()
{
    // logger("fake console\n");
    vgic_passthrough_irq(PL011_INT);
}

void
paththrough_irq79()
{
    // logger("passthrough irq\n");
    vgic_passthrough_irq(79);
}

void
paththrough_irq78()
{
    // logger("passthrough irq\n");
    vgic_passthrough_irq(78);
}

void
mmio_map_gicd()
{
    for (int32_t i = 0; i < MMIO_PAGES_GICD; i++) {
        lpae_t *avr_entry    = get_ept_entry((uint64_t) MMIO_AREA_GICD + MMIO_PAGE_SIZE * i);
        avr_entry->p2m.read  = 0;
        avr_entry->p2m.write = 0;
        apply_ept(avr_entry);
    }
}

void
mmio_map_gicc()
{
    for (int32_t i = 0; i < MMIO_PAGES_GICC; i++) {
        lpae_t *avr_entry    = get_ept_entry((uint64_t) MMIO_AREA_GICC + MMIO_PAGE_SIZE * i);
        avr_entry->p2m.read  = 0;
        avr_entry->p2m.write = 0;
        apply_ept(avr_entry);
    }
}

void
mmio_map_pl011()
{
    for (int32_t i = 0; i < MMIO_PAGES_PL011; i++) {
        lpae_t *avr_entry    = get_ept_entry((uint64_t) MMIO_AREA_PL011 + MMIO_PAGE_SIZE * i);
        avr_entry->p2m.read  = 0;
        avr_entry->p2m.write = 0;
        apply_ept(avr_entry);
    }
}

// 旧的load_guest_image函数已被guest_load_from_manifest替代

void
test_mem_hypervisor()
{
    lpae_t *avr_entry    = get_ept_entry((uint64_t) MMIO_ARREA);
    avr_entry->p2m.read  = 0;
    avr_entry->p2m.write = 0;
    apply_ept(avr_entry);
    *(uint64_t *) MMIO_ARREA = 0x1234;
}

// 一个vm必定有多个vcpu，一个vgic。 先在这里初始化
struct _vm_t *
alloc_vm()
{
    if (vm_num >= VM_NUM_MAX) {
        logger_error("No more vm can be allocated!\n");
        return NULL;
    }
    struct _vm_t *vm = &vms[vm_num++];
    memset(vm, 0, sizeof(struct _vm_t));
    vm->vm_id = vm_num - 1;

    // 初始化 vcpu 列表
    list_init(&vm->vcpus);

    // 获取对应的 vgic 结构体
    vm->vgic = alloc_vgic();

    // 获取对应的 vtimer 结构体
    vm->vtimer = alloc_vtimer();

    // 获取对应的 vpl011 结构体
    vm->vpl011 = alloc_vpl011();

    logger_warn("alloc vm: %d\n", vm->vm_id);
    return vm;
}

// 首核以外的核先跑这个。
extern void
test_guest();

static void
guest_trap_init(void)
{
    logger_info("Initialize trap...\n");
    unsigned long hcr;
    hcr = read_hcr_el2();
    // WRITE_SYSREG(hcr | HCR_TGE, HCR_EL2);
    // hcr = READ_SYSREG(HCR_EL2);
    logger_info("HCR : 0x%llx\n", hcr);
    isb();
}

// 旧的vm_init函数已被vm_init_with_manifest替代

void
run_vm(struct _vm_t *vm)
{
    if (vm == NULL) {
        logger_error("run_vm: vm is NULL\n");
        return;
    }

    // task_add_to_readylist_tail(vm->primary_vcpu);
    task_add_to_readylist_tail_remote(vm->primary_vcpu, vm->primary_vcpu->affinity - 1);
}

// 新的VM初始化函数，使用Guest配置清单
void
vm_init_with_manifest(struct _vm_t *vm, const guest_manifest_t *manifest)
{
    if (!vm || !manifest) {
        logger_error("Invalid parameters for VM initialization\n");
        return;
    }

    logger_info("Initializing VM with manifest: %s (type: %s)\n",
                manifest->name,
                guest_type_to_string(manifest->type));

    // 保存配置清单引用
    vm->manifest   = manifest;
    vm->guest_type = manifest->type;

    // 从文件系统加载Guest镜像
    vm->load_result = guest_load_from_manifest(manifest);
    if (vm->load_result.error != GUEST_LOAD_SUCCESS) {
        logger_error("Failed to load guest manifest: %s (error: %s)\n",
                     manifest->name,
                     guest_load_error_to_string(vm->load_result.error));
        return;
    }

    // 设置VM配置
    strncpy(vm->vm_name, manifest->name, VM_NAME_MAX - 1);
    vm->vm_name[VM_NAME_MAX - 1] = '\0';

    // 继续原有的初始化逻辑
    int32_t  vcpu_num   = manifest->smp_num;
    uint64_t entry_addr = manifest->bin_loadaddr;
    uint64_t dtb_addr   = manifest->dtb_loadaddr;

    logger_info("VM%d: Name set to '%s', entry=0x%llx, vcpus=%d\n",
                vm->vm_id,
                vm->vm_name,
                entry_addr,
                vcpu_num);

    //(1) 设置hcr
    guest_trap_init();

    //(2) 设置ttbr,和vtcr
    // 这里在main_vmm.c中已经设置了

    //(3) 分配vcpu
    vm->vcpu_cnt = vcpu_num;

    //(3.1) 首核 - 所有VM的首核都绑定到pCPU 0
    void *stack = kalloc_pages(VM_STACK_PAGES);
    if (stack == NULL) {
        logger_error("Failed to allocate stack for primary vcpu\n");
        return;
    }

    tcb_t *task = create_vm_task((void *) entry_addr,
                                 (uint64_t) stack + VM_STACK_SIZE,
                                 PRIMARY_VCPU_PCPU_MASK,
                                 dtb_addr);
    if (task == NULL) {
        logger_error("Failed to create vcpu task\n");
        kfree_pages(stack, VM_STACK_PAGES);
        return;
    }

    list_insert_last(&vm->vcpus, &task->vm_node);
    task->curr_vm                      = vm;
    task->cpu_info->sys_reg->mpidr_el1 = (1ULL << 31) | (uint64_t) (0 & 0xff);
    vm->primary_vcpu                   = task;

    //(3.2) 其它核 - 所有VM的其他核都绑定到pCPU 1
    for (int32_t i = 1; i < vcpu_num; i++) {
        void *stack = kalloc_pages(VM_STACK_PAGES);
        if (stack == NULL) {
            logger_error("Failed to allocate stack for vcpu %d\n", i);
            return;
        }
        tcb_t *task = create_vm_task(test_guest,
                                     (uint64_t) stack + VM_STACK_SIZE,
                                     SECONDARY_VCPU_PCPU_MASK,
                                     0);
        if (task == NULL) {
            logger_error("Failed to create vcpu task %d\n", i);
            kfree_pages(stack, VM_STACK_PAGES);
            return;
        }
        list_insert_last(&vm->vcpus, &task->vm_node);
        task->curr_vm                      = vm;
        task->cpu_info->sys_reg->mpidr_el1 = (1ULL << 31) | (uint64_t) (i & 0xff);
    }

    // 映射 MMIO 区域
    mmio_map_gicd();
    mmio_map_gicc();
    mmio_map_pl011();

    // 初始化虚拟 GIC
    vm->vgic->vm = vm;
    for (int32_t i = 0; i < vm->vcpu_cnt; i++) {
        vgic_core_state_t *state = alloc_gicc();

        state->id          = i;
        state->vmcr        = mmio_read32((void *) GICH_VMCR);
        state->saved_elsr0 = mmio_read32((void *) GICH_ELSR0);
        state->saved_apr   = mmio_read32((void *) GICH_APR);
        state->saved_hcr   = 0x1;

        vm->vgic->core_state[i] = state;
    }

    // for (int32_t i = 0; i < vm->vcpu_cnt; i++)
    //     vgicc_dump(vm->vgic->core_state[i]);

    // 初始化虚拟定时器
    vm->vtimer->vm       = vm;
    vm->vtimer->vcpu_cnt = vm->vcpu_cnt;
    for (int32_t i = 0; i < vm->vcpu_cnt; i++) {
        vtimer_core_state_t *vtimer_state = alloc_vtimer_core_state(i);
        if (vtimer_state) {
            vm->vtimer->core_state[i] = vtimer_state;
            logger_info("Allocated vtimer core state for vCPU %d\n", i);
        } else {
            logger_error("Failed to allocate vtimer core state for vCPU %d\n", i);
        }
    }

    // 初始化虚拟串口
    vm->vpl011->vm               = vm;
    vpl011_state_t *vpl011_state = alloc_vpl011_state();
    if (vpl011_state) {
        vm->vpl011->state = vpl011_state;
        logger_info("Allocated vpl011 state for VM %d\n", vm->vm_id);
    } else {
        logger_error("Failed to allocate vpl011 state for VM %d\n", vm->vm_id);
    }

    // 安装中断处理程序
    irq_install(79, paththrough_irq79);
    irq_install(78, paththrough_irq78);

    logger_info("VM %s initialization completed successfully\n", vm->vm_name);
}
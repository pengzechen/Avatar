
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
#include "../guest/guests.h"
#include "mmio.h"

// qemu 准备启动4个核
// 每个核跑两个 vcpu， 共8个vcpu
// 跑4个vm每个vm使用2个vcpu 或者 跑2个vm每个vm使用4个vcpu

static struct _vm_t vms[VM_NUM_MAX];
static uint32_t vm_num = 0;


void fake_timer() {
    // logger("fake timer\n");
    vgic_hw_inject_test(TIMER_VECTOR);
}

void fake_console()
{
    // logger("fake console\n");
    vgic_passthrough_irq(PL011_INT);
}

void mmio_map_gicd()
{
    for (int32_t i = 0; i < MMIO_PAGES_GICD; i++)
    {
        lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_AREA_GICD + MMIO_PAGE_SIZE * i);
        avr_entry->p2m.read = 0;
        avr_entry->p2m.write = 0;
        apply_ept(avr_entry);
    }
}

void mmio_map_gicc()
{
    for (int32_t i = 0; i < MMIO_PAGES_GICC; i++)
    {
        lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_AREA_GICC + MMIO_PAGE_SIZE * i);
        avr_entry->p2m.read = 0;
        avr_entry->p2m.write = 0;
        apply_ept(avr_entry);
    }
}

void mmio_map_pl011()
{
    for (int32_t i = 0; i < MMIO_PAGES_PL011; i++)
    {
        lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_AREA_PL011 + MMIO_PAGE_SIZE * i);
        avr_entry->p2m.read = 0;
        avr_entry->p2m.write = 0;
        apply_ept(avr_entry);
    }
}

void load_guest_image(int32_t vm_id)
{
    guest_image_t *img = &guest_configs[vm_id].image;
    uint64_t guest_kernel_start = guest_configs[vm_id].bin_loadaddr;
    uint64_t guest_dtb_start = guest_configs[vm_id].dtb_loadaddr;
    uint64_t guest_fs_start = guest_configs[vm_id].fs_loadaddr;

    // 加载 guest kernel：必须存在
    size_t size = img->bin_end - img->bin_start;
    memcpy((void *)guest_kernel_start, img->bin_start, size);
    logger_info("VM%d: Kernel loaded (%zu bytes)\n", vm_id, size);

    // 加载 dtb：可选
    size = img->dtb_end - img->dtb_start;
    if (size > 0) {
        memcpy((void *)guest_dtb_start, img->dtb_start, size);
        logger_info("VM%d: DTB loaded (%zu bytes)\n", vm_id, size);
    } else {
        logger_warn("VM%d: No DTB, skipping\n", vm_id);
    }

    // 加载 fs：可选
    size = img->fs_end - img->fs_start;
    if (size > 0) {
        memcpy((void *)guest_fs_start, img->fs_start, size);
        logger_info("VM%d: FS loaded (%zu bytes)\n", vm_id, size);
    } else {
        logger_warn("VM%d: No FS, skipping\n", vm_id);
    }
}

void test_mem_hypervisor()
{
    lpae_t *avr_entry = get_ept_entry((uint64_t)MMIO_ARREA);
    avr_entry->p2m.read = 0;
    avr_entry->p2m.write = 0;
    apply_ept(avr_entry);
    *(uint64_t *)MMIO_ARREA = 0x1234;
}

// 一个vm必定有多个vcpu，一个vgic。 先在这里初始化
struct _vm_t *alloc_vm()
{
    if (vm_num >= VM_NUM_MAX)
    {
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
extern void test_guest();

static void guest_trap_init(void)
{
    logger_info("Initialize trap...\n");
    unsigned long hcr;
    hcr = read_hcr_el2();
    // WRITE_SYSREG(hcr | HCR_TGE, HCR_EL2);
    // hcr = READ_SYSREG(HCR_EL2);
    logger_info("HCR : 0x%llx\n", hcr);
    isb();
}

// 初始化虚拟机
void vm_init(struct _vm_t *vm, int32_t configured_vm_id)
{
    int32_t vcpu_num = guest_configs[configured_vm_id].smp_num;
    
    uint64_t entry_addr = guest_configs[configured_vm_id].bin_loadaddr;
    
    //(1) 设置hcr
    guest_trap_init();

    //(2) 设置ttbr,和vtcr
    // 这里在main_vmm.c中已经设置了

    //(3) 分配vcpu
    // 创建两个 guest 任务
    vm->vcpu_cnt = vcpu_num;

    //(3.1) 首核 - 所有VM的首核都绑定到pCPU 0
    void *stack = kalloc_pages(VM_STACK_PAGES);
    if (stack == NULL)
    {
        logger_error("Failed to allocate stack for primary vcpu \n");
        return;
    }
    tcb_t *task = create_vm_task((void *)entry_addr, (uint64_t)stack + VM_STACK_SIZE, PRIMARY_VCPU_PCPU_MASK);
    if (task == NULL)
    {
        logger_error("Failed to create vcpu task\n");
        return;
    }
    list_insert_last(&vm->vcpus, &task->vm_node);
    task->curr_vm = vm; // 设置当前虚拟机
    task->cpu_info->sys_reg->mpidr_el1 = (1ULL << 31) | (uint64_t)(0 & 0xff);
    vm->primary_vcpu = task; // 设置主 vcpu

    //(3.2) 其它核 - 所有VM的其他核都绑定到pCPU 1
    for (int32_t i = 1; i < vcpu_num; i++)
    {
        void *stack = kalloc_pages(VM_STACK_PAGES);
        if (stack == NULL)
        {
            logger_error("Failed to allocate stack for vcpu %d\n", i);
            return;
        }
        tcb_t *task = create_vm_task(test_guest, (uint64_t)stack + VM_STACK_SIZE, SECONDARY_VCPU_PCPU_MASK);
        if (task == NULL)
        {
            logger_error("Failed to create vcpu task %d\n", i);
            return;
        }
        list_insert_last(&vm->vcpus, &task->vm_node);
        task->curr_vm = vm; // 设置当前虚拟机

        task->cpu_info->sys_reg->mpidr_el1 = (1ULL << 31) | (uint64_t)(i & 0xff);

        // dev use
        // task_add_to_readylist_tail(task);
    }

    // test
    list_node_t *iter = list_first(&vm->vcpus);
    tcb_t *taskt = NULL;
    while (iter)
    {
        taskt = list_node_parent(iter, tcb_t, vm_node);
        logger_info("vcpu task id: %d, mpidr_el1: 0x%x\n", taskt->task_id, taskt->cpu_info->sys_reg->mpidr_el1);
        logger_debug("vcpu task id: %d, sp_elx: 0x%x\n", taskt->task_id, taskt->ctx.sp_elx);

        iter = list_node_next(iter);
    }

    // 拷贝guest镜像
    load_guest_image(configured_vm_id);

    // 映射 MMIO 区域
    mmio_map_gicd();
    mmio_map_gicc();
    mmio_map_pl011();

    // 初始化虚拟 GIC
    vm->vgic->vm = vm;
    for (int32_t i=0; i<vm->vcpu_cnt; i++) {
        vgic_core_state_t * state = alloc_gicc();

        state->id = i;
        state->vmcr = mmio_read32((void *)GICH_VMCR);
        // state->vmcr = 0x1;
        state->saved_elsr0 = mmio_read32((void *)GICH_ELSR0);
        state->saved_apr = mmio_read32((void *)GICH_APR);
        // state->saved_hcr = mmio_read32((void *)GICH_HCR);
        state->saved_hcr = 0x1;

        vm->vgic->core_state[i] = state;
    }
    
    for (int32_t i=0; i<vm->vcpu_cnt; i++) 
        vgicc_dump(vm->vgic->core_state[i]);

    // 初始化虚拟定时器
    vm->vtimer->vm = vm;  // 建立双向关联
    vm->vtimer->vcpu_cnt = vm->vcpu_cnt;
    for (int32_t i = 0; i < vm->vcpu_cnt; i++) {
        vtimer_core_state_t *vtimer_state = alloc_vtimer_core_state(i);
        if (vtimer_state) {
            vm->vtimer->core_state[i] = vtimer_state;  // 类似 vgic 的方式
            logger_info("Allocated vtimer core state for vCPU %d\n", i);
        } else {
            logger_error("Failed to allocate vtimer core state for vCPU %d\n", i);
        }
    }

    // 初始化虚拟串口
    vm->vpl011->vm = vm;  // 建立双向关联
    vpl011_state_t *vpl011_state = alloc_vpl011_state();
    if (vpl011_state) {
        vm->vpl011->state = vpl011_state;
        logger_info("Allocated vpl011 state for VM %d\n", vm->vm_id);
    } else {
        logger_error("Failed to allocate vpl011 state for VM %d\n", vm->vm_id);
    }

    // 这两个 fake 都可以去掉了！
    // irq_install(HV_TIMER_VECTOR, fake_timer);
    // irq_install(PL011_INT, fake_console);
}

void run_vm(struct _vm_t *vm)
{
    if (vm == NULL)
    {
        logger_error("run_vm: vm is NULL\n");
        return;
    }

    // task_add_to_readylist_tail(vm->primary_vcpu);
    task_add_to_readylist_tail_remote(vm->primary_vcpu, vm->primary_vcpu->priority - 1);
}
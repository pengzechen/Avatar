/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file vgic.c
 * @brief Implementation of vgic.c
 * @author Avatar Project Team
 * @date 2024
 */



/*     目前只准备实现 vgic    */

#include "vmm/vgic.h"
#include "avatar_types.h"
#include "vmm/vm.h"
#include "exception.h"
#include "vmm/vmm_cfg.h"
#include "io.h"
#include "mem/barrier.h"
#include "thread.h"
#include "mmio.h"
#include "lib/avatar_string.h"
#include "lib/bit_utils.h"
#include "exception.h"

static vgic_t   _vgic[VM_NUM_MAX];
static uint32_t _vgic_num = 0;

static vgic_core_state_t _state[VCPU_NUM_MAX];
static uint32_t          _state_num = 0;


vgic_core_state_t *
get_vgicc_by_vcpu(tcb_t *task);  // if task==NULL, return current task's core state structure

vgic_t *
alloc_vgic()
{
    if (_vgic_num >= VM_NUM_MAX) {
        logger_error("No more VGIC can be allocated!\n");
        return NULL;
    }
    vgic_t *vgic = &_vgic[_vgic_num++];
    memset(vgic, 0, sizeof(vgic_t));
    return vgic;
}

vgic_core_state_t *
alloc_gicc()
{
    return &_state[_state_num++];
}

vgic_core_state_t *
get_vgicc_by_vcpu(tcb_t *task)
{
    if (!task) {
        task = curr_task_el2();
    }
    struct _vm_t      *vm    = task->curr_vm;
    vgic_t            *vgic  = vm->vgic;
    vgic_core_state_t *vgicc = NULL;

    list_node_t *iter = list_first(&vm->vcpus);
    int32_t      i    = 0;
    while (iter) {
        tcb_t *iter_vcpu = list_node_parent(iter, tcb_t, vm_node);
        if (task == iter_vcpu) {
            vgicc = vgic->core_state[i];
            break;  // 找到了，直接退出
        }
        i++;
        iter = list_node_next(iter);
    }
    if (!vgicc) {
        logger_error("VGIC: failed to find vgicc for task %d in vm %d\n", task->task_id, vm->vm_id);
        while (1) {
            ;
        }
        return NULL;
    }
    return vgicc;
}

// --------------------------------------------------------
// ==================      Debug 使用     ==================
// --------------------------------------------------------
void
vgicc_dump(vgic_core_state_t *vgicc)
{
    logger_info("====== VGICC Dump (vCPU ID: %u) ======\n", vgicc->id);

    logger_info("VMCR  = 0x%08x\n", vgicc->vmcr);
    logger_info("ELSR0 = 0x%08x\n", vgicc->saved_elsr0);
    logger_info("APR   = 0x%08x\n", vgicc->saved_apr);
    logger_info("HCR   = 0x%08x\n", vgicc->saved_hcr);

    for (int32_t i = 0; i < GICH_LR_NUM; i++) {
        logger_info("LR[%1d] = 0x%08x\n", i, vgicc->saved_lr[i]);
    }

    logger_info("Pending IRQs:\n");
    for (int32_t i = 0; i < SPI_ID_MAX; i++) {
        if (vgic_is_irq_pending(vgicc, i)) {
            logger_info("  IRQ %d is pending\n", i);
        }
    }

    logger_info("======================================\n");
}

void
vgicc_hw_dump(void)
{
    logger_info("====== VGICC HW Dump ======\n");

    uint32_t vmcr  = mmio_read32((void *) (GICH_VMCR));
    uint32_t elsr0 = mmio_read32((void *) (GICH_ELSR0));
    uint32_t elsr1 = mmio_read32((void *) (GICH_ELSR1));
    uint32_t apr   = mmio_read32((void *) (GICH_APR));
    uint32_t hcr   = mmio_read32((void *) (GICH_HCR));
    uint32_t vtr   = mmio_read32((void *) (GICH_VTR));
    uint32_t misr  = mmio_read32((void *) (GICH_MISR));

    logger_info("VMCR  = 0x%08x\n", vmcr);
    logger_info("ELSR0 = 0x%08x\n", elsr0);
    logger_info("ELSR1 = 0x%08x\n", elsr1);
    logger_info("APR   = 0x%08x\n", apr);
    logger_info("HCR   = 0x%08x\n", hcr);
    logger_info("VTR   = 0x%08x\n", vtr);
    logger_info("MISR  = 0x%08x\n", misr);

    for (int32_t i = 0; i < GICH_LR_NUM; i++) {
        uint32_t lr = mmio_read32((void *) (GICH_LR(i)));
        if (lr != 0) {
            uint32_t vid   = lr & 0x3ff;
            uint32_t pid   = (lr >> 10) & 0x3ff;
            uint32_t pri   = (lr >> 23) & 0x1f;
            uint32_t state = (lr >> 28) & 0x3;
            uint32_t grp1  = (lr >> 30) & 0x1;
            uint32_t hw    = (lr >> 31) & 0x1;

            logger_info("LR[%1d] = 0x%08x (VID=%d, PID=%d, PRI=%d, STATE=%d, GRP1=%d, HW=%d)\n",
                        i,
                        lr,
                        vid,
                        pid,
                        pri,
                        state,
                        grp1,
                        hw);
        } else {
            logger_info("LR[%1d] = 0x%08x (empty)\n", i, lr);
        }
    }

    logger_info("======================================\n");
}

static void
vgicc_dump_if_changed(vgic_core_state_t *state, uint32_t vcpu_id, uint32_t task_id, uint32_t vm_id)
{
    static uint32_t last_vmcr[8]            = {0};
    static uint32_t last_elsr0[8]           = {0};
    static uint32_t last_hcr[8]             = {0};
    static uint32_t last_lr[8][GICH_LR_NUM] = {0};
    static bool     initialized[8]          = {false};

    if (vcpu_id >= 8)
        return;

    bool changed = false;

    // 首次初始化或检测变化
    if (!initialized[vcpu_id]) {
        changed              = true;
        initialized[vcpu_id] = true;
    } else {
        if (last_vmcr[vcpu_id] != state->vmcr || last_elsr0[vcpu_id] != state->saved_elsr0 ||
            last_hcr[vcpu_id] != state->saved_hcr) {
            changed = true;
        }

        if (!changed) {
            for (int32_t i = 0; i < GICH_LR_NUM; i++) {
                if (last_lr[vcpu_id][i] != state->saved_lr[i]) {
                    changed = true;
                    break;
                }
            }
        }
    }

    if (changed) {
        logger_vgic_debug("====== vCPU %d Hardware State Changed ======\n", vcpu_id);
        logger_vgic_debug("Task ID: %d, VM ID: %d\n", task_id, vm_id);
        logger_vgic_debug("VMCR: 0x%08x, ELSR0: 0x%08x, HCR: 0x%08x\n",
                          state->vmcr,
                          state->saved_elsr0,
                          state->saved_hcr);

        for (int32_t i = 0; i < GICH_LR_NUM; i++) {
            if (state->saved_lr[i] != 0) {
                logger_vgic_debug("LR[%d]: 0x%08x\n", i, state->saved_lr[i]);
            }
        }
        logger_vgic_debug("==========================================\n");

        // 更新缓存
        last_vmcr[vcpu_id]  = state->vmcr;
        last_elsr0[vcpu_id] = state->saved_elsr0;
        last_hcr[vcpu_id]   = state->saved_hcr;
        for (int32_t i = 0; i < GICH_LR_NUM; i++) {
            last_lr[vcpu_id][i] = state->saved_lr[i];
        }
    }
}

// 检查虚拟中断注入的完整状态
void
vgic_check_injection_status(void)
{
    tcb_t *curr = curr_task_el2();

    logger_info("====== VGIC Injection Status Check ======\n");

    if (!curr) {
        logger_error("No current task\n");
        return;
    }

    logger_info("Current task ID: %d\n", curr->task_id);

    if (!curr->curr_vm) {
        logger_warn("Current task is not a VM task\n");
        return;
    }

    logger_info("Current VM ID: %d\n", curr->curr_vm->vm_id);

    // 检查 HCR_EL2 设置
    uint64_t hcr_el2 = read_hcr_el2();
    logger_info("HCR_EL2: 0x%llx\n", hcr_el2);
    logger_info("  IMO (bit 4): %s\n", (hcr_el2 & (1ULL << 4)) ? "enabled" : "disabled");
    logger_info("  FMO (bit 3): %s\n", (hcr_el2 & (1ULL << 3)) ? "enabled" : "disabled");
    logger_info("  VM (bit 0): %s\n", (hcr_el2 & (1ULL << 0)) ? "enabled" : "disabled");

    // 检查 DAIF 状态
    uint32_t daif = get_daif();
    logger_info("DAIF: 0x%x\n", daif);
    logger_info("  I (IRQ masked): %s\n", (daif & (1 << 7)) ? "yes" : "no");
    logger_info("  F (FIQ masked): %s\n", (daif & (1 << 6)) ? "yes" : "no");

    // 检查 GICH 状态
    vgicc_hw_dump();

    logger_info("==========================================\n");
}

// --------------------------------------------------------
// ==================       中断注入       ==================
// --------------------------------------------------------
// 给指定vcpu注入一个sgi中断
void
vgic_inject_sgi(tcb_t *task, int32_t int_id)
{
    // 参数检查
    if (int_id < 0 || int_id > 15) {
        logger_error("Invalid SGI ID: %d (must be 0-15)\n", int_id);
        return;
    }

    if (!task) {
        logger_error("Invalid task for SGI injection\n");
        return;
    }

    if (!task->curr_vm) {
        logger_error("Task is not a VM task\n");
        return;
    }

    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);
    if (!vgicc) {
        logger_error("Failed to get VGICC for task %d\n", task->task_id);
        return;
    }

    // 如果已 pending，不重复注入
    if (vgic_is_irq_pending(vgicc, int_id)) {
        logger_vgic_debug("SGI %d already pending on vCPU %d, skip inject\n",
                          int_id,
                          task->task_id);
        return;
    }

    // 检查中断是否使能
    if (!(vgicc->sgi_ppi_isenabler & (1U << int_id))) {
        logger_vgic_debug("SGI %d is disabled on vCPU %d, still inject to pending\n",
                          int_id,
                          task->task_id);
    }

    // 标记为 pending
    vgic_set_irq_pending(vgicc, int_id);

    logger_info("Inject SGI %d to vCPU %d (task %d) on pCPU %d\n",
                int_id,
                get_vcpuid(task),
                task->task_id,
                get_current_cpu_id());

    // 如果当前正在运行此 vCPU，尝试立即注入
    if (task == curr_task_el2()) {
        logger_vgic_debug("Try inject pending SGI for running vCPU %d (task %d)\n",
                          get_vcpuid(task),
                          task->task_id);
        vgic_try_inject_pending(task);
    }

    // vgicc_dump(vgicc);
}

// 给指定vcpu注入一个ppi中断
void
vgic_inject_ppi(tcb_t *task, int32_t irq_id)
{
    // irq_id: 16~31

    // 参数检查
    if (irq_id < 16 || irq_id > 31) {
        logger_error("Invalid PPI ID: %d (must be 16-31)\n", irq_id);
        return;
    }

    if (!task) {
        logger_error("Invalid task for PPI injection\n");
        return;
    }

    if (!task->curr_vm) {
        logger_error("Task is not a VM task\n");
        return;
    }

    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);
    if (!vgicc) {
        logger_error("Failed to get VGICC for task %d\n", task->task_id);
        return;
    }

    // 检查中断是否已经 pending
    if (vgic_is_irq_pending(vgicc, irq_id)) {
        logger_vgic_debug("PPI %d already pending on vCPU %d, skip inject\n",
                          irq_id,
                          task->task_id);
        return;
    }

    // 检查中断是否使能
    if (!(vgicc->sgi_ppi_isenabler & (1U << irq_id))) {
        logger_vgic_debug("PPI %d is disabled on vCPU %d, abort inject\n", irq_id, task->task_id);
        return;
    }

    // 标记为 pending
    vgic_set_irq_pending(vgicc, irq_id);

    logger_vgic_debug("Inject PPI %d to vCPU %d (task %d) on pCPU %d\n",
                      irq_id,
                      get_vcpuid(task),
                      task->task_id,
                      get_current_cpu_id());

    // 如果当前正在运行此 vCPU，尝试立即注入到 GICH_LR
    if (task == curr_task_el2()) {
        logger_vgic_debug("Try inject pending PPI for running vCPU %d (task %d)\n",
                          get_vcpuid(task),
                          task->task_id);
        vgic_try_inject_pending(task);
    }
}

// 给指定vcpu注入一个spi中断
void
vgic_inject_spi(tcb_t *task, int32_t irq_id)
{
    // irq_id: 32~1019
    if (irq_id < 32 || irq_id > 1019) {
        logger_error("Invalid SPI ID: %d (must be 32-1019)\n", irq_id);
        return;
    }
    if (!task) {
        logger_error("Invalid task for SPI injection\n");
        return;
    }
    if (!task->curr_vm) {
        logger_error("Task is not a VM task\n");
        return;
    }

    vgic_t *vgic = task->curr_vm->vgic;
    // 检查中断是否使能
    if (!(vgic->gicd_scenabler[(irq_id / 32) - 1] & (1U << (irq_id % 32)))) {
        logger_vgic_debug("SPI %d is disabled on vCPU %d, abort inject\n", irq_id, task->task_id);
        // return;
    }

    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);
    if (!vgicc) {
        logger_error("Failed to get VGICC for task %d\n", task->task_id);
        return;
    }

    // 检查中断是否已经 pending
    if (vgic_is_irq_pending(vgicc, irq_id)) {
        logger_vgic_debug("SPI %d already pending on vCPU %d, skip inject\n",
                          irq_id,
                          task->task_id);
        return;
    }

    // 标记为 pending
    vgic_set_irq_pending(vgicc, irq_id);

    logger_vgic_debug("Inject SPI %d to vCPU %d (task %d) on pCPU %d\n",
                      irq_id,
                      get_vcpuid(task),
                      task->task_id,
                      get_current_cpu_id());

    // 如果当前正在运行此 vCPU，尝试立即注入到 GICH_LR
    if (task == curr_task_el2()) {
        logger_vgic_debug("Try inject pending SPI for running vCPU %d (task %d)\n",
                          get_vcpuid(task),
                          task->task_id);
        vgic_try_inject_pending(task);
    }
}

// vm进入的时候把 pending 的中断注入到 GICH_LR
void
vgic_try_inject_pending(tcb_t *task)
{
    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);
    vgic_t            *vgic  = task->curr_vm->vgic;

    // 使用软件保存的 ELSR0 来判断空闲的 LR
    uint64_t elsr = vgicc->saved_elsr0;  // 目前你只有 ELSR0，够用（最多 32 个 LR）

    // 处理 SGI (0-15) 和 PPI (16-31)
    for (int i = 0; i < 32; ++i) {
        if (!vgic_is_irq_pending(vgicc, i))
            continue;

        int freelr = -1;
        for (int lr = 0; lr < GICH_LR_NUM; lr++) {
            if ((elsr >> lr) & 0x1) {
                freelr = lr;
                break;
            }

            // 防止重复注入：判断 saved_lr 中是否已经有相同中断
            uint32_t val = vgicc->saved_lr[lr];
            uint32_t vid = (val >> GICH_LR_PID_SHIFT) & 0x3ff;
            if (vid == i) {
                freelr = -1;
                break;
            }
        }

        if (freelr < 0) {
            // 太多了
            // logger_vgic_debug("No free LR for IRQ %d (in memory), delay inject\n", i);
            break;
        }

        int32_t  vcpu_id = get_vcpuid(task);
        uint32_t lr_val;

        // 根据中断类型创建不同的 LR 值
        if (i < 16) {
            // SGI: 使用软件中断格式
            lr_val = gic_make_virtual_software_sgi(i, /*cpu_id=*/vcpu_id, 0, 0);
        } else {
            // PPI: 使用硬件中断格式
            lr_val = gic_make_virtual_hardware_interrupt(i, i, 0, 0);
        }

        // 将虚拟中断写入到内存中的 LR
        vgicc->saved_lr[freelr] = lr_val;
        // 标记该 LR 不再空闲（ELSR 置位为 0 表示 occupied）
        vgicc->saved_elsr0 &= ~(1U << freelr);

        // 清除 pending 标志
        vgic_clear_irq_pending(vgicc, i);

        const char *irq_type = (i < 16) ? "SGI" : "PPI";
        logger_vgic_debug("Injected %s %d into LR%d for vCPU %d (task %d), LR value: 0x%x\n",
                          irq_type,
                          i,
                          freelr,
                          vcpu_id,
                          task->task_id,
                          lr_val);

        // dev use
        // gicc_restore_core_state();
        // vgicc_hw_dump();
        // vgic_sw_inject_test(i);
        // vgicc_hw_dump();
    }

    // 处理 SPI (32-SPI_ID_MAX)
    for (int i = 32; i < SPI_ID_MAX; ++i) {
        if (!vgic_is_irq_pending(vgicc, i))
            continue;

        // 检查 SPI 是否使能
        if (!(vgic->gicd_scenabler[(i / 32) - 1] & (1U << (i % 32)))) {
            // SPI 未使能，跳过注入
            continue;
        }

        int freelr = -1;
        for (int lr = 0; lr < GICH_LR_NUM; lr++) {
            if ((elsr >> lr) & 0x1) {
                freelr = lr;
                break;
            }

            // 防止重复注入：判断 saved_lr 中是否已经有相同中断
            uint32_t val = vgicc->saved_lr[lr];
            uint32_t vid = (val >> GICH_LR_PID_SHIFT) & 0x3ff;
            if (vid == i) {
                freelr = -1;
                break;
            }
        }

        if (freelr < 0) {
            logger_vgic_debug("No free LR for SPI %d (in memory), delay inject\n", i);
            break;
        }

        // SPI: 使用硬件中断格式
        uint32_t lr_val = gic_make_virtual_hardware_interrupt(i, i, 0, 0);

        // 将虚拟中断写入到内存中的 LR
        vgicc->saved_lr[freelr] = lr_val;
        // 标记该 LR 不再空闲（ELSR 置位为 0 表示 occupied）
        vgicc->saved_elsr0 &= ~(1U << freelr);

        // 清除 pending 标志
        vgic_clear_irq_pending(vgicc, i);

        logger_vgic_debug("Injected SPI %d into LR%d for vCPU %d (task %d), LR value: 0x%x\n",
                          i,
                          freelr,
                          get_vcpuid(task),
                          task->task_id,
                          lr_val);

        // dev use
        // gicc_restore_core_state();
        // vgicc_hw_dump();
        // vgic_sw_inject_test(i);
        // vgicc_hw_dump();
    }
}

// --------------------------------------------------------
// ==================    GICC 状态保存     ==================
// --------------------------------------------------------
void
gicc_save_core_state()
{
    tcb_t *curr = curr_task_el2();
    if (!curr->curr_vm)
        return;
    vgic_core_state_t *state = get_vgicc_by_vcpu(curr);

    state->vmcr        = mmio_read32((void *) GICH_VMCR);
    state->saved_elsr0 = mmio_read32((void *) GICH_ELSR0);
    state->saved_apr   = mmio_read32((void *) GICH_APR);
    state->saved_hcr   = mmio_read32((void *) GICH_HCR);

    for (int32_t i = 0; i < GICH_LR_NUM; i++)
        state->saved_lr[i] = gic_read_lr(i);
}

void
gicc_restore_core_state()
{
    tcb_t *curr = curr_task_el2();
    if (!curr->curr_vm)
        return;
    vgic_core_state_t *state = get_vgicc_by_vcpu(curr);

    mmio_write32(state->vmcr, (void *) GICH_VMCR);
    mmio_write32(state->saved_elsr0, (void *) GICH_ELSR0);
    mmio_write32(state->saved_apr, (void *) GICH_APR);
    mmio_write32(state->saved_hcr, (void *) GICH_HCR);

    for (int32_t i = 0; i < GICH_LR_NUM; i++)
        gic_write_lr(i, state->saved_lr[i]);

    // vgicc_dump_if_changed(state, get_vcpuid(curr), curr->task_id, curr->curr_vm->vm_id);
}

// --------------------------------------------------------
// ==================    直通中断操作     ===================
// --------------------------------------------------------
void
vgic_passthrough_irq(int32_t irq_id)
{
    // find vms and vcpus need to inject
    // todo
    tcb_t *task = curr_task_el2();
    vgic_inject_spi(task, irq_id);
}

// vgic inject test
void
vgic_hw_inject_test(uint32_t vector)
{
    logger_info("vgic inject vector: %d\n", vector);

    // 检查当前是否有运行的虚拟机
    tcb_t *curr = curr_task_el2();
    if (!curr || !curr->curr_vm) {
        logger_warn("No current VM for interrupt injection\n");
        return;
    }

    uint32_t mask = gic_make_virtual_hardware_interrupt(vector, vector, 0, 0);

    uint32_t elsr0 = gic_elsr0();
    uint32_t elsr1 = gic_elsr1();
    uint64_t elsr  = ((uint64_t) elsr1 << 32) | elsr0;

    uint32_t is_active = gic_apr();
    uint32_t pri       = gic_lr_read_pri(mask);
    uint32_t irq_no    = gic_lr_read_vid(mask);
    int32_t  freelr    = -1;

    logger_vgic_debug("ELSR: 0x%llx, APR: 0x%x, PRI: 0x%x, IRQ: %d\n",
                      elsr,
                      is_active,
                      pri,
                      irq_no);

    for (int32_t i = 0; i < GICH_LR_NUM; i++) {
        if ((elsr >> i) & 0x1) {
            if (freelr < 0)
                freelr = i;

            continue;
        }

        uint32_t lr_val          = gic_read_lr(i);
        uint32_t existing_vector = (lr_val >> GICH_LR_PID_SHIFT) & 0x3ff;
        if (existing_vector == vector) {
            logger_vgic_debug("vgic inject, vector %d already in lr%d (val=0x%x)\n",
                              vector,
                              i,
                              lr_val);
            return;  // busy
        }
    }

    if (freelr < 0) {
        logger_error("No free LR available for vector %d\n", vector);
        return;
    }

    logger_vgic_debug("Injecting vector %d into LR%d, mask=0x%x\n", vector, freelr, mask);
    gic_write_lr(freelr, mask);

    // 确保写入生效
    dsb(sy);
    isb();

    // 验证写入
    uint32_t written_val = gic_read_lr(freelr);
    logger_vgic_debug("LR%d written value: 0x%x\n", freelr, written_val);
}

void
vgic_sw_inject_test(uint32_t vector)
{
    logger_info("vgic inject vector: %d\n", vector);

    // 检查当前是否有运行的虚拟机
    tcb_t *curr = curr_task_el2();
    if (!curr || !curr->curr_vm) {
        logger_warn("No current VM for interrupt injection\n");
        return;
    }

    uint32_t mask = gic_make_virtual_software_sgi(vector, /*cpu_id=*/0, 0, 0);

    uint32_t elsr0 = gic_elsr0();
    uint32_t elsr1 = gic_elsr1();
    uint64_t elsr  = ((uint64_t) elsr1 << 32) | elsr0;

    uint32_t is_active = gic_apr();
    uint32_t pri       = gic_lr_read_pri(mask);
    uint32_t irq_no    = gic_lr_read_vid(mask);
    int32_t  freelr    = -1;

    logger_vgic_debug("ELSR: 0x%llx, APR: 0x%x, PRI: 0x%x, IRQ: %d\n",
                      elsr,
                      is_active,
                      pri,
                      irq_no);

    for (int32_t i = 0; i < GICH_LR_NUM; i++) {
        if ((elsr >> i) & 0x1) {
            if (freelr < 0)
                freelr = i;

            continue;
        }

        uint32_t lr_val          = gic_read_lr(i);
        uint32_t existing_vector = (lr_val >> GICH_LR_PID_SHIFT) & 0x3ff;
        if (existing_vector == vector) {
            logger_vgic_debug("vgic inject, vector %d already in lr%d (val=0x%x)\n",
                              vector,
                              i,
                              lr_val);
            return;  // busy
        }
    }

    if (freelr < 0) {
        logger_error("No free LR available for vector %d\n", vector);
        return;
    }

    logger_vgic_debug("Injecting vector %d into LR%d, mask=0x%x\n", vector, freelr, mask);
    gic_write_lr(freelr, mask);

    // 确保写入生效
    dsb(sy);
    isb();

    // 验证写入
    uint32_t written_val = gic_read_lr(freelr);
    logger_vgic_debug("LR%d written value: 0x%x\n", freelr, written_val);
}
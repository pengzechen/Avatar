#include "hyper/vtimer.h"
#include "hyper/vgic.h"
#include "hyper/vm.h"
#include "task/task.h"
#include "io.h"
#include "lib/aj_string.h"
#include "thread.h"

#define VIRTUAL_TIMER_IRQ 27  // PPI 27 用于虚拟定时器中断（注意：不使用真实的27号中断）

// CNTV_CTL 寄存器位定义
#define CNTV_CTL_ENABLE     (1U << 0)   // 使能位
#define CNTV_CTL_IMASK      (1U << 1)   // 中断屏蔽位
#define CNTV_CTL_ISTATUS    (1U << 2)   // 中断状态位

// 虚拟定时器管理结构
static vtimer_t _vtimers[VM_NUM_MAX];
static uint32_t _vtimer_num = 0;

// 虚拟定时器核心状态池
static vtimer_core_state_t _vtimer_states[VM_NUM_MAX * VCPU_NUM_MAX];
static uint32_t _vtimer_state_num = 0;

// --------------------------------------------------------
// ==================    初始化函数    ==================
// --------------------------------------------------------

void vtimer_global_init(void)
{
    memset(_vtimers, 0, sizeof(_vtimers));
    memset(_vtimer_states, 0, sizeof(_vtimer_states));
    _vtimer_num = 0;
    _vtimer_state_num = 0;
    logger_info("Virtual timer global initialized\n");
}

vtimer_t *alloc_vtimer(void)
{
    if (_vtimer_num >= VM_NUM_MAX) {
        logger_error("No more vtimer can be allocated!\n");
        return NULL;
    }

    vtimer_t *vtimer = &_vtimers[_vtimer_num++];
    memset(vtimer, 0, sizeof(vtimer_t));
    vtimer->vcpu_cnt = 0;
    vtimer->now_tick = 0;  // 初始化时钟 tick

    logger_info("Allocated vtimer %d\n", _vtimer_num - 1);
    return vtimer;
}

vtimer_core_state_t *alloc_vtimer_core_state(uint32_t vcpu_id)
{
    if (_vtimer_state_num >= VM_NUM_MAX * VCPU_NUM_MAX) {
        logger_error("No more vtimer core state can be allocated!\n");
        return NULL;
    }

    vtimer_core_state_t *vt = &_vtimer_states[_vtimer_state_num++];
    vtimer_core_init(vt, vcpu_id);
    return vt;
}

// 类似 VGIC 的 alloc_gicc 函数
vtimer_core_state_t *alloc_vtimer_core(void)
{
    if (_vtimer_state_num >= VM_NUM_MAX * VCPU_NUM_MAX) {
        logger_error("No more vtimer core state can be allocated!\n");
        return NULL;
    }

    return &_vtimer_states[_vtimer_state_num++];
}

vtimer_core_state_t *get_vtimer_by_vcpu(tcb_t *task)
{
    if (!task || !task->curr_vm || !task->curr_vm->vtimer) {
        return NULL;
    }
    extern int32_t get_vcpuid(tcb_t *task);
    uint32_t vcpu_id = get_vcpuid(task);
    vtimer_t *vtimer = task->curr_vm->vtimer;

    if (vcpu_id < vtimer->vcpu_cnt && vtimer->core_state[vcpu_id]) {
        return vtimer->core_state[vcpu_id];
    }

    return NULL;
}

void vtimer_core_init(vtimer_core_state_t *vt, uint32_t vcpu_id)
{
    memset(vt, 0, sizeof(vtimer_core_state_t));
    vt->id = vcpu_id;
    vt->enabled = false;
    vt->pending = false;
    vt->deadline = 0;
    vt->cntvct_offset = 0;
    vt->cntv_cval = 0;
    vt->cntv_ctl = 0;
    vt->fire_count = 0;
    vt->last_fire_time = 0;

    logger_info("Virtual timer core state initialized for vCPU %d\n", vcpu_id);
}

// --------------------------------------------------------
// ==================   定时器操作函数   ==================
// --------------------------------------------------------

// 通过 task->cpu_info->sys_reg 访问定时器寄存器
uint64_t vtimer_read_cntvct(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg || !task->curr_vm) {
        return 0;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return task->curr_vm->vtimer->now_tick; // 返回当前时钟 tick
    }

    // 返回虚拟计数器值：当前时钟 tick + 偏移
    return task->curr_vm->vtimer->now_tick + vt->cntvct_offset;
}

uint32_t vtimer_read_cntv_ctl(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return 0;
    }
    return (uint32_t)task->cpu_info->sys_reg->cntv_ctl_el0;
}

uint32_t vtimer_read_cntv_tval(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return 0;
    }
    return (uint32_t)task->cpu_info->sys_reg->cntv_tval_el0;
}

void vtimer_write_cntv_cval(tcb_t *task, uint64_t cval)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg || !task->curr_vm) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    // 更新寄存器值
    task->cpu_info->sys_reg->cntv_cval_el0 = cval;
    vt->cntv_cval = cval;

    // 如果定时器使能，重新计算 deadline
    uint32_t ctl = (uint32_t)task->cpu_info->sys_reg->cntv_ctl_el0;
    if (ctl & CNTV_CTL_ENABLE) {
        uint64_t virtual_count = vtimer_read_cntvct(task);
        if (cval > virtual_count) {
            // 计算 deadline 基于当前时钟 tick
            uint64_t delay = cval - virtual_count;
            vt->deadline = task->curr_vm->vtimer->now_tick + delay;
            vt->enabled = true;
        } else {
            // 立即触发
            vt->enabled = false;
            vt->pending = true;
            task->cpu_info->sys_reg->cntv_ctl_el0 |= CNTV_CTL_ISTATUS;
        }
    }

    logger_info("vCPU %d: Set CNTV_CVAL to 0x%llx\n", vt->id, cval);
}

void vtimer_write_cntv_ctl(tcb_t *task, uint32_t ctl)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg || !task->curr_vm) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    uint32_t old_ctl = (uint32_t)task->cpu_info->sys_reg->cntv_ctl_el0;
    ctl = ctl & 0x7;  // 只保留低3位

    // 更新寄存器值
    task->cpu_info->sys_reg->cntv_ctl_el0 = ctl;
    vt->cntv_ctl = ctl;

    // 检查使能位变化
    if ((ctl & CNTV_CTL_ENABLE) && !(old_ctl & CNTV_CTL_ENABLE)) {
        // 定时器被使能
        uint64_t virtual_count = vtimer_read_cntvct(task);
        if (vt->cntv_cval > virtual_count) {
            uint64_t delay = vt->cntv_cval - virtual_count;
            vt->deadline = task->curr_vm->vtimer->now_tick + delay;
            vt->enabled = true;
            vt->pending = false;
            task->cpu_info->sys_reg->cntv_ctl_el0 &= ~CNTV_CTL_ISTATUS;
        } else {
            vt->enabled = false;
            vt->pending = true;
            task->cpu_info->sys_reg->cntv_ctl_el0 |= CNTV_CTL_ISTATUS;
        }
    } else if (!(ctl & CNTV_CTL_ENABLE) && (old_ctl & CNTV_CTL_ENABLE)) {
        // 定时器被禁用
        vt->enabled = false;
        vt->pending = false;
        task->cpu_info->sys_reg->cntv_ctl_el0 &= ~CNTV_CTL_ISTATUS;
    }

    logger_info("vCPU %d: Set CNTV_CTL to 0x%x (enable=%d, imask=%d, istatus=%d)\n",
                vt->id, ctl,
                !!(ctl & CNTV_CTL_ENABLE),
                !!(ctl & CNTV_CTL_IMASK),
                !!(ctl & CNTV_CTL_ISTATUS));
}

void vtimer_write_cntv_tval(tcb_t *task, uint32_t tval)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return;
    }

    // 更新寄存器值
    task->cpu_info->sys_reg->cntv_tval_el0 = tval;

    // TVAL = CVAL - 当前计数器值，所以 CVAL = 当前计数器值 + TVAL
    uint64_t current_count = vtimer_read_cntvct(task);
    uint64_t cval = current_count + tval;
    vtimer_write_cntv_cval(task, cval);

    logger_info("vCPU %d: Set CNTV_TVAL to 0x%x (converted to CVAL: 0x%llx)\n",
                get_vtimer_by_vcpu(task) ? get_vtimer_by_vcpu(task)->id : 0, tval, cval);
}

void vtimer_set_timer(vtimer_core_state_t *vt, uint64_t cval, uint32_t ctl)
{
    if (!vt) {
        logger_error("vtimer_set_timer: invalid vtimer_core_state_t\n");
        return;
    }

    // 通过 vCPU ID 找到对应的 task
    tcb_t *task = get_task_by_vcpu_id(vt->id);
    if (!task) {
        logger_error("vtimer_set_timer: cannot find task for vCPU %d\n", vt->id);
        return;
    }

    // 使用基于 task 的函数来设置定时器
    vtimer_write_cntv_cval(task, cval);
    vtimer_write_cntv_ctl(task, ctl);

    logger_info("vCPU %d: Set timer CVAL=0x%llx, CTL=0x%x\n", vt->id, cval, ctl);
}

// 核心保存/恢复接口
void vtimer_core_save(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    // 从虚拟定时器状态同步到 sys_reg
    task->cpu_info->sys_reg->cntv_ctl_el0 = vt->cntv_ctl;
    task->cpu_info->sys_reg->cntv_cval_el0 = vt->cntv_cval;

    // 计算 TVAL = CVAL - 当前虚拟计数器值
    uint64_t virtual_count = vtimer_read_cntvct(task);
    int64_t tval = (int64_t)(vt->cntv_cval - virtual_count);
    task->cpu_info->sys_reg->cntv_tval_el0 = (uint64_t)tval;

    // logger_debug("vCPU %d: Saved timer state - CTL=0x%x, CVAL=0x%llx, TVAL=0x%llx\n",
    //             vt->id, vt->cntv_ctl, vt->cntv_cval, (uint64_t)tval);
}

void vtimer_core_restore(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg || !task->curr_vm) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    // 从 sys_reg 恢复到虚拟定时器状态
    uint32_t ctl = (uint32_t)task->cpu_info->sys_reg->cntv_ctl_el0;
    uint64_t cval = task->cpu_info->sys_reg->cntv_cval_el0;

    vt->cntv_ctl = ctl;
    vt->cntv_cval = cval;

    // 重新计算定时器状态
    if (ctl & CNTV_CTL_ENABLE) {
        uint64_t virtual_count = vtimer_read_cntvct(task);
        if (cval > virtual_count) {
            uint64_t delay = cval - virtual_count;
            vt->deadline = task->curr_vm->vtimer->now_tick + delay;
            vt->enabled = true;
            vt->pending = false;
        } else {
            vt->enabled = false;
            vt->pending = true;
            task->cpu_info->sys_reg->cntv_ctl_el0 |= CNTV_CTL_ISTATUS;
        }
    } else {
        vt->enabled = false;
        vt->pending = false;
    }

    // logger_debug("vCPU %d: Restored timer state - CTL=0x%x, CVAL=0x%llx, enabled=%d\n",
    //             vt->id, ctl, cval, vt->enabled);
}

bool vtimer_should_fire(vtimer_core_state_t *vt, uint64_t now)
{
    return vt->enabled && (now >= vt->deadline);
}

void vtimer_inject_to_vcpu(tcb_t *task)
{
    if (!task || !task->curr_vm) {
        logger_error("Invalid task for vtimer injection\n");
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        logger_error("Failed to get vtimer for task %d\n", task->task_id);
        return;
    }

    // 标记为 pending 并设置状态位
    vt->pending = true;
    vt->enabled = false;
    vt->cntv_ctl |= CNTV_CTL_ISTATUS;
    vt->fire_count++;
    vt->last_fire_time = task->curr_vm->vtimer->now_tick;

    // 如果中断没有被屏蔽，注入 PPI 27（注意：这是虚拟中断，不使用真实的27号中断）
    if (!(vt->cntv_ctl & CNTV_CTL_IMASK)) {
        vgic_inject_ppi(task, VIRTUAL_TIMER_IRQ);
        // logger_info("Injected virtual timer interrupt (PPI %d) to vCPU %d\n",
        //             VIRTUAL_TIMER_IRQ, vt->id);
    } else {
        logger_info("Virtual timer interrupt masked for vCPU %d\n", vt->id);
    }
}

// --------------------------------------------------------
// ==================   辅助函数        ==================
// --------------------------------------------------------

// 根据 VM 和 vCPU 索引查找对应的 task
tcb_t *get_task_by_vm_vcpu(struct _vm_t *vm, uint32_t vcpu_idx)
{
    if (!vm) return NULL;

    // 遍历该 VM 的所有 vCPU，找到第 vcpu_idx 个
    list_node_t *iter = list_first(&vm->vcpus);
    uint32_t current_idx = 0;
    while (iter) {
        tcb_t *vcpu_task = list_node_parent(iter, tcb_t, vm_node);

        if (current_idx == vcpu_idx) {
            return vcpu_task;  // 找到匹配的 task
        }

        current_idx++;
        iter = list_node_next(iter);
    }

    // 没有找到匹配的 task
    return NULL;
}

// 保持原函数接口兼容性，但使用新的查找逻辑
tcb_t *get_task_by_vcpu_id(uint32_t vcpu_id)
{
    // 这个函数现在已经不推荐使用，因为vcpu_id在不同VM中可能重复
    // 遍历所有 VM 的所有 vCPU 来查找匹配的 task
    for (uint32_t vm_idx = 0; vm_idx < _vtimer_num; vm_idx++) {
        vtimer_t *vtimer = &_vtimers[vm_idx];
        if (!vtimer->vm) continue;  // 跳过未初始化的 vtimer

        struct _vm_t *vm = vtimer->vm;

        // 遍历该 VM 的所有 vCPU
        list_node_t *iter = list_first(&vm->vcpus);
        while (iter) {
            tcb_t *vcpu_task = list_node_parent(iter, tcb_t, vm_node);

            // 检查该 task 的 vCPU ID 是否匹配
            uint32_t task_vcpu_id = (vcpu_task->cpu_info->sys_reg->mpidr_el1 & 0xff);
            if (task_vcpu_id == vcpu_id) {
                return vcpu_task;  // 找到匹配的 task
            }

            iter = list_node_next(iter);
        }
    }

    // 没有找到匹配的 task
    return NULL;
}

// --------------------------------------------------------
// ==================   主机中断处理    ==================
// --------------------------------------------------------


void v_timer_tick(uint64_t now)
{
    extern int32_t get_vcpuid(tcb_t *task);

    // 更新所有 VM 的时钟 tick
    for (uint32_t vm_idx = 0; vm_idx < _vtimer_num; vm_idx++) {
        vtimer_t *vtimer = &_vtimers[vm_idx];
        if (!vtimer->vm) continue;

        // 更新该 VM 的时钟 tick
        vtimer->now_tick = now;
    }

    // Iterate over all VMs
    for (uint32_t vm_idx = 0; vm_idx < _vtimer_num; vm_idx++) {
        vtimer_t *vtimer = &_vtimers[vm_idx];
        if (!vtimer->vm) continue;

        // Iterate over all vCPUs
        for (uint32_t vcpu_idx = 0; vcpu_idx < vtimer->vcpu_cnt; vcpu_idx++) {
            vtimer_core_state_t *vt = vtimer->core_state[vcpu_idx];
            if (!vt) continue;

            // Find the corresponding task using VM and vCPU index
            tcb_t *task = get_task_by_vm_vcpu(vtimer->vm, vcpu_idx);
            if (!task) continue;

            // Only process vCPUs bound to the current pCPU
            if (task->priority - 1 != get_current_cpu_id()) {
                continue;
            }

            // Check if timer should fire based on guest settings
            // if (vtimer_should_fire(vt, now)) {
                // logger_debug("[pcpu: %d]: Timer fired for VM%d vCPU %d(task: %d) - CVAL=0x%llx, now=0x%llx\n",
                //     get_current_cpu_id(), task->curr_vm->vm_id, get_vcpuid(task), task->task_id,
                //     vt->cntv_cval, now);

                // Update timer state
                vt->pending = true;
                vt->enabled = false;
                vt->cntv_ctl |= CNTV_CTL_ISTATUS;
                vt->fire_count++;
                vt->last_fire_time = now;

                // Update sys_reg to reflect the new state
                if (task->cpu_info && task->cpu_info->sys_reg) {
                    task->cpu_info->sys_reg->cntv_ctl_el0 = vt->cntv_ctl;
                }

                // Inject interrupt to the vCPU (虚拟中断，不使用真实的27号中断)
                vtimer_inject_to_vcpu(task);
            // }
        }
    }
}
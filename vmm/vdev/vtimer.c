#include "vmm/vtimer.h"
#include "vmm/vgic.h"
#include "vmm/vm.h"
#include "task/task.h"
#include "io.h"
#include "lib/avatar_string.h"
#include "thread.h"
#include "os_cfg.h"
#include "timer.h"

// CNTV_CTL 寄存器位定义
#define CNTV_CTL_ENABLE  (1U << 0)  // 使能位
#define CNTV_CTL_IMASK   (1U << 1)  // 中断屏蔽位
#define CNTV_CTL_ISTATUS (1U << 2)  // 中断状态位

// 虚拟定时器管理结构
static vtimer_t _vtimers[VM_NUM_MAX];
static uint32_t _vtimer_num = 0;

// 虚拟定时器核心状态池
static vtimer_core_state_t _vtimer_states[VM_NUM_MAX * VCPU_NUM_MAX];
static uint32_t            _vtimer_state_num = 0;

// --------------------------------------------------------
// ==================    初始化函数    ==================
// --------------------------------------------------------

void
vtimer_global_init(void)
{
    memset(_vtimers, 0, sizeof(_vtimers));
    memset(_vtimer_states, 0, sizeof(_vtimer_states));
    _vtimer_num       = 0;
    _vtimer_state_num = 0;
    logger_info("Virtual timer global initialized\n");
}

vtimer_t *
alloc_vtimer(void)
{
    if (_vtimer_num >= VM_NUM_MAX) {
        logger_error("No more vtimer can be allocated!\n");
        return NULL;
    }

    vtimer_t *vtimer = &_vtimers[_vtimer_num++];
    memset(vtimer, 0, sizeof(vtimer_t));
    vtimer->vcpu_cnt   = 0;
    vtimer->now_tick   = 0;                   // 虚拟时间从0开始
    vtimer->start_time = read_cntpct_el0();   // 记录VM启动时的物理时间
    vtimer->cntvoff    = vtimer->start_time;  // 设置偏移量，使虚拟时间从0开始

    logger_info("Allocated vtimer %d, start_time=0x%llx, cntvoff=0x%llx\n",
                _vtimer_num - 1,
                vtimer->start_time,
                vtimer->cntvoff);
    return vtimer;
}

vtimer_core_state_t *
alloc_vtimer_core_state(uint32_t vcpu_id)
{
    if (_vtimer_state_num >= VM_NUM_MAX * VCPU_NUM_MAX) {
        logger_error("No more vtimer core state can be allocated!\n");
        return NULL;
    }

    vtimer_core_state_t *vt = &_vtimer_states[_vtimer_state_num++];
    vtimer_core_init(vt, vcpu_id);
    return vt;
}

vtimer_core_state_t *
alloc_vtimer_core(void)
{
    if (_vtimer_state_num >= VM_NUM_MAX * VCPU_NUM_MAX) {
        logger_error("No more vtimer core state can be allocated!\n");
        return NULL;
    }

    return &_vtimer_states[_vtimer_state_num++];
}

void
vtimer_core_init(vtimer_core_state_t *vt, uint32_t vcpu_id)
{
    memset(vt, 0, sizeof(vtimer_core_state_t));
    vt->id             = vcpu_id;
    vt->enabled        = false;
    vt->pending        = false;
    vt->deadline       = 0;
    vt->cntvct_offset  = 0;
    vt->cntv_cval      = 0;
    vt->cntv_ctl       = 0;
    vt->cntv_tval      = 0;
    vt->fire_count     = 0;
    vt->last_fire_time = 0;

    logger_info("Virtual timer core state initialized for vCPU %d\n", vcpu_id);
}


vtimer_core_state_t *
get_vtimer_by_vcpu(tcb_t *task)
{
    if (!task || !task->curr_vm || !task->curr_vm->vtimer) {
        return NULL;
    }
    extern int32_t get_vcpuid(tcb_t * task);
    uint32_t       vcpu_id = get_vcpuid(task);
    vtimer_t      *vtimer  = task->curr_vm->vtimer;

    if (vcpu_id < vtimer->vcpu_cnt && vtimer->core_state[vcpu_id]) {
        return vtimer->core_state[vcpu_id];
    }

    return NULL;
}


// 为VM设置虚拟时间偏移
void
vtimer_set_vm_offset(vtimer_t *vtimer)
{
    if (!vtimer) {
        return;
    }

    // 设置CNTVOFF_EL2，使Guest看到的虚拟时间从0开始
    write_cntvoff_el2(vtimer->cntvoff);

    logger_info("Set CNTVOFF_EL2 to 0x%llx for VM, virtual time starts from 0\n", vtimer->cntvoff);
}

// --------------------------------------------------------
// ==================   定时器操作函数   ==================
// --------------------------------------------------------

uint32_t
vtimer_read_cntv_ctl(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return 0;
    }
    return (uint32_t) task->cpu_info->sys_reg->cntv_ctl_el0;
}

int32_t
vtimer_read_cntv_tval(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg || !task->curr_vm) {
        return 0;
    }

    // 根据 ARM 规范：TVAL = CVAL - CNTVCT
    // 这里 now_tick 相当于 CNTVCT（虚拟计数器值）
    uint64_t cval   = task->cpu_info->sys_reg->cntv_cval_el0;
    uint64_t cntvct = task->curr_vm->vtimer->now_tick;

    // 计算 TVAL（有符号的 32 位值）
    // TVAL = CVAL - CNTVCT，表示距离定时器触发还有多少个时钟周期
    int64_t tval_64 = (int64_t) cval - (int64_t) cntvct;

    return (int32_t) tval_64;
}

uint64_t
vtimer_read_cntv_cval(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return 0;
    }
    return task->cpu_info->sys_reg->cntv_cval_el0;
}

void
vtimer_write_cntv_ctl(tcb_t *task, uint32_t ctl)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    // 保存旧的控制寄存器值
    uint32_t old_ctl = vt->cntv_ctl;

    // 更新控制寄存器
    vt->cntv_ctl                          = ctl;
    task->cpu_info->sys_reg->cntv_ctl_el0 = ctl;

    // 如果 guest 清除了 ISTATUS 位，清除 pending 状态
    if ((old_ctl & CNTV_CTL_ISTATUS) && !(ctl & CNTV_CTL_ISTATUS)) {
        vt->pending = false;
        logger_vtimer_debug("vCPU %d: Guest cleared ISTATUS, clearing pending\n", vt->id);
    }

    // 更新 enabled 状态
    vt->enabled = (ctl & CNTV_CTL_ENABLE) ? true : false;

    // logger_vtimer_debug("vCPU %d: CTL write %u (enabled=%d, masked=%d, status=%d)\n",
    //             vt->id, ctl,
    //             !!(ctl & CNTV_CTL_ENABLE),
    //             !!(ctl & CNTV_CTL_IMASK),
    //             !!(ctl & CNTV_CTL_ISTATUS));
}

void
vtimer_write_cntv_cval(tcb_t *task, uint64_t cval)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    // 检测异常的 CVAL 值，防止 Guest OS 设置错误的定时器值
    uint64_t current_time = task->curr_vm->vtimer->now_tick;
    int64_t  tval_check   = (int64_t) cval - (int64_t) current_time;

    // 更新比较值寄存器
    vt->cntv_cval                          = cval;
    task->cpu_info->sys_reg->cntv_cval_el0 = cval;

    // 根据 ARM 规范，写入 CVAL 会清除 ISTATUS 位
    if (vt->cntv_ctl & CNTV_CTL_ISTATUS) {
        vt->cntv_ctl &= ~CNTV_CTL_ISTATUS;
        task->cpu_info->sys_reg->cntv_ctl_el0 = vt->cntv_ctl;
        vt->pending                           = false;
        // logger_vtimer_debug("vCPU %d: CVAL write cleared ISTATUS\n", vt->id);
    }

    // logger_vtimer_debug("vCPU %d: CVAL write %llu (current_time=%llu, tval=%lld)\n",
    //             vt->id, cval, current_time, tval_check);
}

void
vtimer_write_cntv_tval(tcb_t *task, int32_t tval)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg || !task->curr_vm) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    // 根据 ARM 规范：写入 TVAL 时，CVAL = TVAL + CNTVCT
    // 使用当前的虚拟时间作为基准
    uint64_t current_time = task->curr_vm->vtimer->now_tick;

    // TVAL 是有符号的 32 位值，需要正确处理符号扩展到 64 位
    int64_t  signed_tval_64 = (int64_t) tval;
    uint64_t new_cval       = current_time + signed_tval_64;

    // 更新寄存器
    vt->cntv_tval                          = tval;  // 直接使用 int32_t
    vt->cntv_cval                          = new_cval;
    task->cpu_info->sys_reg->cntv_tval_el0 = (uint32_t) tval;  // 转换为无符号存储
    task->cpu_info->sys_reg->cntv_cval_el0 = new_cval;

    // 根据 ARM 规范，写入 TVAL 会清除 ISTATUS 位
    if (vt->cntv_ctl & CNTV_CTL_ISTATUS) {
        vt->cntv_ctl &= ~CNTV_CTL_ISTATUS;
        task->cpu_info->sys_reg->cntv_ctl_el0 = vt->cntv_ctl;
        vt->pending                           = false;
        logger_vtimer_debug("vCPU %d: TVAL write cleared ISTATUS\n", vt->id);
    }

    logger_vtimer_debug("vCPU %d: TVAL write %d, CVAL=%llu, now=%llu\n",
                        vt->id,
                        tval,
                        new_cval,
                        current_time);
}

// 核心保存/恢复接口
void
vtimer_core_restore(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    task->cpu_info->sys_reg->cntv_ctl_el0  = vt->cntv_ctl;
    task->cpu_info->sys_reg->cntv_cval_el0 = vt->cntv_cval;

    // TVAL 会根据 CVAL 和当前时间动态计算，不需要直接设置
    // 但为了兼容性，我们还是设置一下
    task->cpu_info->sys_reg->cntv_tval_el0 = (uint32_t) vt->cntv_tval;
}

void
vtimer_core_save(tcb_t *task)
{
    if (!task || !task->cpu_info || !task->cpu_info->sys_reg || !task->curr_vm) {
        return;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(task);
    if (!vt) {
        return;
    }

    // 首先计算当前虚拟时间
    // 读取真正的虚拟计数器值：CNTVCT_EL0 = CNTPCT_EL0 - CNTVOFF_EL2
    uint64_t physical_time        = read_cntpct_el0();
    uint64_t cntvoff              = read_cntvoff_el2();
    uint64_t current_virtual_time = physical_time - cntvoff;

    // 从 Guest 的系统寄存器读取当前定时器状态
    uint32_t ctl  = (uint32_t) task->cpu_info->sys_reg->cntv_ctl_el0;
    uint64_t cval = task->cpu_info->sys_reg->cntv_cval_el0;
    int32_t tval_stored = (int32_t) task->cpu_info->sys_reg->cntv_tval_el0;  // Guest 存储的旧值

    // 检查 Guest 是否修改了寄存器值，如果修改了就调用相应的写入函数
    bool ctl_changed  = (ctl != vt->cntv_ctl);
    bool cval_changed = (cval != vt->cntv_cval);

    // 检查 TVAL 是否被 Guest 主动修改
    // 我们通过检查 Guest 存储的 TVAL 值与我们计算的实时 TVAL 值的差异来判断
    int64_t expected_tval = (int64_t) vt->cntv_cval - (int64_t) current_virtual_time;
    int64_t tval_diff     = (int64_t) tval_stored - expected_tval;
    bool    tval_changed  = (tval_diff > 1000 || tval_diff < -1000);  // 允许小的时间差异

    if (ctl_changed) {
        // logger_vtimer_debug("vCPU %d: Guest modified CTL: 0x%x -> 0x%x\n", vt->id, vt->cntv_ctl, ctl);
        vtimer_write_cntv_ctl(task, ctl);
    }

    if (cval_changed) {
        // logger_vtimer_debug("vCPU %d: Guest modified CVAL: %llu -> %llu\n", vt->id, vt->cntv_cval, cval);
        vtimer_write_cntv_cval(task, cval);
    }

    if (tval_changed && !cval_changed) {
        // Guest 修改了 TVAL 但没有修改 CVAL，说明是通过 TVAL 设置定时器
        // logger_vtimer_debug("vCPU %d: Guest modified TVAL: expected=%lld, stored=%d\n", vt->id, expected_tval, tval_stored);
        vtimer_write_cntv_tval(task, tval_stored);
    }

    // 如果寄存器被修改过，重新读取更新后的值
    if (ctl_changed || cval_changed || tval_changed) {
        ctl  = vt->cntv_ctl;
        cval = vt->cntv_cval;
    }

    // 更新 VM 的虚拟时间为真实值
    task->curr_vm->vtimer->now_tick = current_virtual_time;

    int64_t tval_calculated = (int64_t) cval - (int64_t) current_virtual_time;

    // 由于我们已经在前面调用了写入函数处理寄存器变化，这里只需要处理强制清除逻辑

    // 检查是否需要强制清除 pending 状态
    // 如果 Guest OS 长时间不清除 ISTATUS 位，我们强制清除 pending
    if (vt->pending && vt->last_fire_time > 0) {
        uint64_t time_since_fire = current_virtual_time - vt->last_fire_time;
        // 如果超过 100ms (6.25M ticks) 还没清除，强制清除
        if (time_since_fire > 6250000) {
            logger_warn("vCPU %d: Force clearing pending after %llu ticks (cval=%llu, now=%llu, "
                        "ctl=0x%x)\n",
                        vt->id,
                        time_since_fire,
                        cval,
                        current_virtual_time,
                        ctl);
            vt->pending = false;
            // 也清除我们设置的 ISTATUS 位
            vt->cntv_ctl &= ~CNTV_CTL_ISTATUS;
            if (task->cpu_info && task->cpu_info->sys_reg) {
                task->cpu_info->sys_reg->cntv_ctl_el0 = vt->cntv_ctl;
            }
        }
    }

    // 更新存储的状态（写入函数已经更新了 vt->cntv_ctl 和 vt->cntv_cval）
    vt->cntv_tval = (int32_t) tval_calculated;
}

bool
vtimer_should_fire(vtimer_core_state_t *vt, uint64_t now)
{
    // 检查定时器是否启用（CTL寄存器的ENABLE位）
    if (!(vt->cntv_ctl & CNTV_CTL_ENABLE)) {
        vt->enabled = false;
        return false;
    }

    vt->enabled = true;

    // 根据 ARM Generic Timer 规范：
    // 定时器触发条件是：(CVAL - CNTVCT) <= 0
    // 这里 now 相当于 CNTVCT（虚拟计数器值）
    // CVAL 是比较值寄存器

    // 如果当前时间 >= 比较值，则定时器应该触发
    bool should_fire = (now >= vt->cntv_cval);

    // 添加调试信息
    if (should_fire) {
        logger_vtimer_debug("vCPU %d: FIRE CHECK - now=%llu, cval=%llu, diff=%lld\n",
                            vt->id,
                            now,
                            vt->cntv_cval,
                            (int64_t) now - (int64_t) vt->cntv_cval);
    }

    return should_fire;
}

void
vtimer_inject_to_vcpu(tcb_t *task)
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
    vt->cntv_ctl |= CNTV_CTL_ISTATUS;  // 设置中断状态位
    vt->fire_count++;
    vt->last_fire_time = task->curr_vm->vtimer->now_tick;

    // 将状态写回到 guest 的寄存器中
    if (task->cpu_info && task->cpu_info->sys_reg) {
        task->cpu_info->sys_reg->cntv_ctl_el0 = vt->cntv_ctl;
    }

    // 如果中断没有被屏蔽，注入 PPI 27（注意：这是虚拟中断，不使用真实的27号中断）
    if (!(vt->cntv_ctl & CNTV_CTL_IMASK)) {
        vgic_inject_ppi(task, VIRTUAL_TIMER_IRQ);
        logger_vtimer_debug("Injected virtual timer interrupt (PPI %d) to vCPU %d\n",
                            VIRTUAL_TIMER_IRQ,
                            vt->id);
    } else {
        logger_vtimer_debug("Virtual timer interrupt masked for vCPU %d\n", vt->id);
    }
}

// --------------------------------------------------------
// ==================   辅助函数        ==================
// --------------------------------------------------------

// 根据 VM 和 vCPU 索引查找对应的 task
tcb_t *
get_task_by_vm_vcpu(struct _vm_t *vm, uint32_t vcpu_idx)
{
    if (!vm)
        return NULL;

    // 遍历该 VM 的所有 vCPU，找到第 vcpu_idx 个
    list_node_t *iter        = list_first(&vm->vcpus);
    uint32_t     current_idx = 0;
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
tcb_t *
get_task_by_vcpu_id(uint32_t vcpu_id)
{
    // 这个函数现在已经不推荐使用，因为vcpu_id在不同VM中可能重复
    // 遍历所有 VM 的所有 vCPU 来查找匹配的 task
    for (uint32_t vm_idx = 0; vm_idx < _vtimer_num; vm_idx++) {
        vtimer_t *vtimer = &_vtimers[vm_idx];
        if (!vtimer->vm)
            continue;  // 跳过未初始化的 vtimer

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


void
v_timer_tick(uint64_t now)
{
    extern int32_t get_vcpuid(tcb_t * task);

    // v_timer_tick 的职责：检查所有 VM 的虚拟定时器是否需要触发
    // 虚拟时间的更新由 vtimer_core_save 在上下文切换时完成

    // Iterate over all VMs
    for (uint32_t vm_idx = 0; vm_idx < _vtimer_num; vm_idx++) {
        vtimer_t *vtimer = &_vtimers[vm_idx];
        if (!vtimer->vm)
            continue;

        // 获取当前时间
        uint64_t current_time = vtimer->now_tick;

        // Iterate over all vCPUs
        for (uint32_t vcpu_idx = 0; vcpu_idx < vtimer->vcpu_cnt; vcpu_idx++) {
            vtimer_core_state_t *vt = vtimer->core_state[vcpu_idx];
            if (!vt)
                continue;

            // Find the corresponding task using VM and vCPU index
            tcb_t *task = get_task_by_vm_vcpu(vtimer->vm, vcpu_idx);
            if (!task)
                continue;

            // Only process vCPUs bound to the current pCPU
            if (task->affinity - 1 != get_current_cpu_id()) {
                continue;
            }

            // 使用存储在 vtimer_core_state_t 中的状态，而不是每次从 Guest 寄存器读取
            // 这些状态在 vtimer_core_save/restore 时会被正确更新
            uint32_t guest_ctl  = vt->cntv_ctl;
            uint64_t guest_cval = vt->cntv_cval;

            // 检查定时器是否应该触发
            // 条件：定时器启用 && 当前时间 >= 目标时间 && 没有待处理的中断
            bool timer_enabled = (guest_ctl & CNTV_CTL_ENABLE) != 0;
            bool should_fire   = timer_enabled && (current_time >= guest_cval);

            if (should_fire && !vt->pending) {
                logger_vtimer_debug("[pcpu: %d]: Timer fired for VM%d vCPU %d(task: %d) - "
                                    "guest_cval=%llu, current_time=%llu, diff=%lld\n",
                                    get_current_cpu_id(),
                                    task->curr_vm->vm_id,
                                    get_vcpuid(task),
                                    task->task_id,
                                    guest_cval,
                                    current_time,
                                    (int64_t) current_time - (int64_t) guest_cval);

                vtimer_inject_to_vcpu(task);
            } else if (should_fire && vt->pending) {
                // 定时器应该触发但已经有待处理的中断，跳过
                // logger_vtimer_debug("[pcpu: %d]: Timer should fire but already pending for VM%d vCPU %d\n",
                //     get_current_cpu_id(), task->curr_vm->vm_id, get_vcpuid(task));
            }
        }
    }
}
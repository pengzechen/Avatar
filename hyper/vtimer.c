#include "hyper/vtimer.h"
#include "hyper/vgic.h"
#include "hyper/vm.h"
#include "task/task.h"
#include "io.h"
#include "lib/aj_string.h"
#include "thread.h"

#define VIRTUAL_TIMER_IRQ 27  // PPI 27 用于虚io拟定时器中断

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

uint64_t vtimer_read_cntvct(vtimer_core_state_t *vt)
{
    // 返回虚拟计数器值：物理计数器值 + 偏移
    uint64_t physical_count = read_cntvct_el0();
    return physical_count + vt->cntvct_offset;
}

uint32_t vtimer_read_cntv_ctl(vtimer_core_state_t *vt)
{
    return vt->cntv_ctl;
}

void vtimer_write_cntv_cval(vtimer_core_state_t *vt, uint64_t cval)
{
    vt->cntv_cval = cval;

    // 如果定时器使能，重新计算 deadline
    if (vt->cntv_ctl & CNTV_CTL_ENABLE) {
        uint64_t virtual_count = vtimer_read_cntvct(vt);
        if (cval > virtual_count) {
            // 计算物理时间的 deadline
            uint64_t delay = cval - virtual_count;
            vt->deadline = read_cntvct_el0() + delay;
            vt->enabled = true;
        } else {
            // 立即触发
            vt->enabled = false;
            vt->pending = true;
            vt->cntv_ctl |= CNTV_CTL_ISTATUS;
        }
    }

    logger_info("vCPU %d: Set CNTV_CVAL to 0x%llx\n", vt->id, cval);
}

void vtimer_write_cntv_ctl(vtimer_core_state_t *vt, uint32_t ctl)
{
    uint32_t old_ctl = vt->cntv_ctl;
    vt->cntv_ctl = ctl & 0x7;  // 只保留低3位

    // 检查使能位变化
    if ((ctl & CNTV_CTL_ENABLE) && !(old_ctl & CNTV_CTL_ENABLE)) {
        // 定时器被使能
        uint64_t virtual_count = vtimer_read_cntvct(vt);
        if (vt->cntv_cval > virtual_count) {
            uint64_t delay = vt->cntv_cval - virtual_count;
            vt->deadline = read_cntvct_el0() + delay;
            vt->enabled = true;
            vt->pending = false;
            vt->cntv_ctl &= ~CNTV_CTL_ISTATUS;
        } else {
            vt->enabled = false;
            vt->pending = true;
            vt->cntv_ctl |= CNTV_CTL_ISTATUS;
        }
    } else if (!(ctl & CNTV_CTL_ENABLE) && (old_ctl & CNTV_CTL_ENABLE)) {
        // 定时器被禁用
        vt->enabled = false;
        vt->pending = false;
        vt->cntv_ctl &= ~CNTV_CTL_ISTATUS;
    }

    logger_info("vCPU %d: Set CNTV_CTL to 0x%x (enable=%d, imask=%d, istatus=%d)\n",
                vt->id, ctl,
                !!(ctl & CNTV_CTL_ENABLE),
                !!(ctl & CNTV_CTL_IMASK),
                !!(ctl & CNTV_CTL_ISTATUS));
}

void vtimer_set_timer(vtimer_core_state_t *vt, uint64_t cval, uint32_t ctl)
{
    // 设置比较值
    vtimer_write_cntv_cval(vt, cval);
    // 设置控制寄存器
    vtimer_write_cntv_ctl(vt, ctl);

    logger_info("vCPU %d: Set timer CVAL=0x%llx, CTL=0x%x\n", vt->id, cval, ctl);
}

bool vtimer_should_fire(vtimer_core_state_t *vt, uint64_t now)
{
    return vt->enabled && (now >= vt->deadline);
}

void vtimer_inject_to_vcpu(tcb_t *task)
{
    if (!task) {
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
    vt->last_fire_time = read_cntvct_el0();

    // 如果中断没有被屏蔽，注入 PPI 27
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

// 根据 vCPU ID 查找对应的 task
tcb_t *get_task_by_vcpu_id(uint32_t vcpu_id)
{
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

// 处理 Guest 对虚拟定时器系统寄存器的访问
bool handle_vtimer_sysreg_access(stage2_fault_info_t *info, trap_frame_t *ctx)
{
    union hsr hsr = info->hsr;

    // 检查是否是 MSR/MRS 指令
    if (hsr.sysreg.ec != 0x18) {
        return false;
    }

    // 获取当前 vCPU 的定时器状态
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    if (!curr || !curr->curr_vm) {
        return false;
    }

    vtimer_core_state_t *vt = get_vtimer_by_vcpu(curr);
    if (!vt) {
        return false;
    }

    uint32_t op0 = hsr.sysreg.op0;
    uint32_t op1 = hsr.sysreg.op1;
    uint32_t crn = hsr.sysreg.crn;
    uint32_t crm = hsr.sysreg.crm;
    uint32_t op2 = hsr.sysreg.op2;
    uint32_t rt = hsr.sysreg.rt;
    bool is_write = hsr.sysreg.direction;  // 1=MSR(写入), 0=MRS(读取)

    // 检查是否是定时器相关寄存器
    // CNTVCT_EL0: op0=3, op1=3, crn=14, crm=0, op2=2
    if (op0 == 3 && op1 == 3 && crn == 14 && crm == 0 && op2 == 2) {
        // 读取虚拟计数器（只读寄存器）
        if (!is_write) {
            uint64_t virtual_count = vtimer_read_cntvct(vt);
            ctx->r[rt] = virtual_count;
            logger_info("Guest read CNTVCT_EL0: 0x%llx\n", virtual_count);
            return true;
        } else {
            logger_warn("Guest attempted to write to read-only CNTVCT_EL0\n");
            return true;  // 忽略写入操作
        }
    }
    // CNTV_CVAL_EL0: op0=3, op1=3, crn=14, crm=3, op2=2
    else if (op0 == 3 && op1 == 3 && crn == 14 && crm == 3 && op2 == 2) {
        if (is_write) {
            // 写入比较值
            uint64_t cval = ctx->r[rt];
            vtimer_write_cntv_cval(vt, cval);
            logger_info("Guest write CNTV_CVAL_EL0: 0x%llx\n", cval);
            return true;
        } else {
            // 读取比较值
            ctx->r[rt] = vt->cntv_cval;
            logger_info("Guest read CNTV_CVAL_EL0: 0x%llx\n", vt->cntv_cval);
            return true;
        }
    }
    // CNTV_CTL_EL0: op0=3, op1=3, crn=14, crm=3, op2=1
    else if (op0 == 3 && op1 == 3 && crn == 14 && crm == 3 && op2 == 1) {
        if (is_write) {
            // 写入控制寄存器
            uint32_t ctl = (uint32_t)ctx->r[rt];
            vtimer_write_cntv_ctl(vt, ctl);
            logger_info("Guest write CNTV_CTL_EL0: 0x%x\n", ctl);
            return true;
        } else {
            // 读取控制寄存器
            ctx->r[rt] = vtimer_read_cntv_ctl(vt);
            logger_info("Guest read CNTV_CTL_EL0: 0x%x\n", vt->cntv_ctl);
            return true;
        }
    }
    // CNTV_TVAL_EL0: op0=3, op1=3, crn=14, crm=3, op2=0 (可选支持)
    else if (op0 == 3 && op1 == 3 && crn == 14 && crm == 3 && op2 == 0) {
        if (is_write) {
            // 写入定时器值 (TVAL = CVAL - 当前计数器值)
            uint32_t tval = (uint32_t)ctx->r[rt];
            uint64_t current_count = vtimer_read_cntvct(vt);
            uint64_t cval = current_count + tval;
            vtimer_write_cntv_cval(vt, cval);
            logger_info("Guest write CNTV_TVAL_EL0: 0x%x (converted to CVAL: 0x%llx)\n", tval, cval);
            return true;
        } else {
            // 读取定时器值 (TVAL = CVAL - 当前计数器值)
            uint64_t current_count = vtimer_read_cntvct(vt);
            int64_t tval = (int64_t)(vt->cntv_cval - current_count);
            ctx->r[rt] = (uint64_t)tval;
            logger_info("Guest read CNTV_TVAL_EL0: 0x%llx\n", (uint64_t)tval);
            return true;
        }
    }

    return false;  // 不是定时器寄存器访问
}

static spinlock_t vtimer_handler_lock;

void v_timer_handler(uint64_t * nouse)
{
    uint64_t now = read_cntvct_el0();
    
    // 只让一个 pCPU 执行，其他直接返回
    if (!spin_trylock(&vtimer_handler_lock)) {
        return;
    }

    // 检查所有 VM 的所有 vCPU 的虚拟定时器
    for (uint32_t vm_idx = 0; vm_idx < _vtimer_num; vm_idx++) {
        vtimer_t *vtimer = &_vtimers[vm_idx];
        if (!vtimer->vm) continue;

        for (uint32_t vcpu_idx = 0; vcpu_idx < vtimer->vcpu_cnt; vcpu_idx++) {
            vtimer_core_state_t *vt = vtimer->core_state[vcpu_idx];
            if (!vt) continue;

            // 找到对应的 task 并注入中断
            tcb_t *task = get_task_by_vcpu_id(vt->id);
            if (task) {
                vtimer_inject_to_vcpu(task);
            } else {
                // 即使找不到 task，也要更新定时器状态
                vt->pending = true;
                vt->enabled = false;
                vt->cntv_ctl |= CNTV_CTL_ISTATUS;
                vt->fire_count++;
                vt->last_fire_time = now;
            }
        }
    }
    
    spin_unlock(&vtimer_handler_lock);
}

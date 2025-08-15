#include "avatar_types.h"
#include "vmm/vpsci.h"
#include "psci.h"
#include "task/task.h"
#include "thread.h"
#include "io.h"

// vpsci_cpu_on: 启动 guest vcpu
// ctx_el2: 当前 trap_frame_t
// 参数：
//   r[1]: target_cpu - 目标 CPU ID (MPIDR_EL1 的低8位)
//   r[2]: entry_point_address - CPU 启动后的入口地址
//   r[3]: context_id - 传递给目标 CPU 的上下文参数
// 返回值：PSCI 标准返回码
int32_t vpsci_cpu_on(trap_frame_t *ctx_el2)
{
    uint64_t cpu_id = ctx_el2->r[1];
    uint64_t entry = ctx_el2->r[2];
    uint64_t context = ctx_el2->r[3];

    // 参数验证
    if (entry == 0) {
        logger_warn("           invalid entry point address: 0x%lx\n", entry);
        return PSCI_RET_INVALID_PARAMS;
    }

    // 检查入口地址对齐（ARM64 要求4字节对齐）
    if (entry & 0x3) {
        logger_warn("           entry point not aligned: 0x%lx\n", entry);
        return PSCI_RET_INVALID_ADDRESS;
    }

    tcb_t *curr = curr_task_el2();
    struct _vm_t *vm = curr->curr_vm;

    if (!vm) {
        logger_error("           current task has no VM context\n");
        return PSCI_RET_INTERNAL_FAILURE;
    }

    list_node_t *iter = list_first(&vm->vcpus);
    tcb_t *target_task = NULL;
    int32_t found = 0;

    // 查找目标 vCPU
    while (iter)
    {
        tcb_t *task = list_node_parent(iter, tcb_t, vm_node);
        if ((task->cpu_info->sys_reg->mpidr_el1 & 0xff) == cpu_id)
        {
            target_task = task;
            found = 1;
            logger_info("           found vcpu for cpu_id: %d, task_id: %d\n",
                       cpu_id, task->task_id);
            break;
        }
        iter = list_node_next(iter);
    }

    if (!found)
    {
        logger_warn("           vcpu not found for cpu_id: %d\n", cpu_id);
        return PSCI_RET_NOT_PRESENT;
    }

    // 检查目标 CPU 当前状态
    switch (target_task->state) {
        case TASK_STATE_RUNNING:
            logger_info("           cpu_id: %d is already running (task_id: %d)\n",
                       cpu_id, target_task->task_id);
            return PSCI_RET_ALREADY_ON;

        case TASK_STATE_READY:
            logger_info("           cpu_id: %d is already ready (task_id: %d)\n",
                       cpu_id, target_task->task_id);
            return PSCI_RET_ALREADY_ON;

        case TASK_STATE_WAIT_IRQ:
            // CPU 在等待中断，可以重新启动
            logger_info("           cpu_id: %d was waiting for IRQ, restarting\n", cpu_id);
            break;

        case TASK_STATE_WAITING:
            // CPU 在睡眠状态，可以重新启动
            logger_info("           cpu_id: %d was sleeping, restarting\n", cpu_id);
            break;

        case TASK_STATE_CREATE:
            // CPU 刚创建，可以启动
            logger_info("           cpu_id: %d is in CREATE state, starting\n", cpu_id);
            break;

        default:
            logger_warn("           cpu_id: %d in unknown state: %d\n",
                       cpu_id, target_task->state);
            return PSCI_RET_INTERNAL_FAILURE;
    }

    // 设置 CPU 启动参数
    trap_frame_t *frame = (trap_frame_t *)target_task->ctx.sp_elx;
    if (!frame) {
        logger_error("           invalid trap frame for task_id: %d\n", target_task->task_id);
        return PSCI_RET_INTERNAL_FAILURE;
    }

    // 设置入口地址和上下文参数
    frame->elr = entry;      // 设置程序计数器
    frame->r[0] = context;   // 将 context_id 传递给目标 CPU 的 X0 寄存器

    // 重置时间片
    target_task->counter = SYS_TASK_TICK;

    // 将 vCPU 添加到目标物理 CPU 的就绪队列
    uint32_t target_core = target_task->affinity - 1;
    if (target_core >= SMP_NUM) {
        logger_error("           invalid affinity for task_id: %d, affinity: %d\n",
                    target_task->task_id, target_task->affinity);
        return PSCI_RET_INTERNAL_FAILURE;
    }

    task_add_to_readylist_tail_remote(target_task, target_core);

    logger_info("           cpu_id: %d successfully started, entry: 0x%lx, context: 0x%lx\n",
               cpu_id, entry, context);

    return PSCI_RET_SUCCESS;
}

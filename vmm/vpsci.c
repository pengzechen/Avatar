#include "aj_types.h"
#include "vmm/vpsci.h"
#include "psci.h"
#include "task/task.h"
#include "thread.h"
#include "io.h"

// vpsci_cpu_on: 启动 guest vcpu
// ctx_el2: 当前 trap_frame_t
// 返回值：0 成功，其他失败
int32_t vpsci_cpu_on(trap_frame_t *ctx_el2)
{
    uint64_t cpu_id = ctx_el2->r[1];
    uint64_t entry = ctx_el2->r[2];
    uint64_t context = ctx_el2->r[3];

    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    struct _vm_t *vm = curr->curr_vm;

    list_node_t *iter = list_first(&vm->vcpus);
    tcb_t *task = NULL;
    int32_t found = 0;
    while (iter)
    {
        task = list_node_parent(iter, tcb_t, vm_node);
        if ((task->cpu_info->sys_reg->mpidr_el1 & 0xff) == cpu_id)
        {
            logger_info("           found a vcpu, task id: %d\n", task->task_id);
            trap_frame_t *frame = (trap_frame_t *)task->ctx.sp_elx;
            frame->elr = entry;   // 设置 elr
            // task_add_to_readylist_tail(task); // 设置为就绪状态
            task_add_to_readylist_tail_remote(task, task->priority - 1);
            found = 1;
            break;
        }
        iter = list_node_next(iter);
    }
    if (!found)
    {
        logger_warn("           vcpu not found for cpu_id: %d\n", cpu_id);
        return -1;
    }
    logger("\n");
    return 0;
}

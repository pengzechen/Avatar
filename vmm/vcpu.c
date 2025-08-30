/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file vcpu.c
 * @brief Implementation of vcpu.c
 * @author Avatar Project Team
 * @date 2024
 */


#include "io.h"
#include "vmm/vcpu.h"
#include "lib/avatar_string.h"
#include "os_cfg.h"
#include "task/task.h"

cpu_sysregs_t cpu_sysregs[MAX_TASKS];

// void print_vcpu(int32_t id)
// {
// 	int32_t reg;
// 	for (reg = 0; reg < 31; reg++)
// 	{
// 		logger("R%d : %llx\n", vcpu[id].ctx.r[reg]);
// 	}
// 	logger("SPSR : %llx\n", vcpu[id].ctx.spsr);
// 	logger("LR : %llx\n", vcpu[id].ctx.elr);
// }


int32_t
get_vcpuid(tcb_t *task)
{
    if (!task) {
        task = curr_task_el2();
    }
    return (task->cpu_info->sys_reg->mpidr_el1 & 0xff);
}

list_t *
get_vcpus(tcb_t *task)
{
    struct _vm_t *vm = NULL;
    if (!task) {
        task = curr_task_el2();
    }
    vm = task->curr_vm;
    return &vm->vcpus;
}

// 这时候的 curr 已经是下一个任务了
// 这里是先进行内存操作，再恢复到真实的寄存器和硬件
void
vcpu_in()
{
    tcb_t      *curr = curr_task_el2();
    extern void restore_sysregs(cpu_sysregs_t *);
    extern void gicc_restore_core_state();
    extern void vgic_try_inject_pending(tcb_t * task);
    extern void vtimer_core_restore(tcb_t * task);

    // 先修改内存中的值
    if (!curr->curr_vm)
        return;

    // 中断操作记录在内存
    vgic_try_inject_pending(curr);

    // 恢复虚拟定时器状态到内存
    vtimer_core_restore(curr);

    // 内存恢复到寄存器
    restore_sysregs(curr->cpu_info->sys_reg);

    // 内存恢复到硬件
    gicc_restore_core_state();
}

// 这里要先进行硬件和寄存器保存，再进行内存恢复
void
vcpu_out()
{
    tcb_t      *curr = curr_task_el2();
    extern void save_sysregs(cpu_sysregs_t *);
    extern void gicc_save_core_state();
    extern void vtimer_core_save(tcb_t * task);
    if (!curr->curr_vm)
        return;

    save_sysregs(curr->cpu_info->sys_reg);

    gicc_save_core_state();

    vtimer_core_save(curr);
}
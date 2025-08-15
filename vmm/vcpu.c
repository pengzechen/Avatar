
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


int32_t get_vcpuid(tcb_t *task)
{
    if (!task)
    {
        task = curr_task_el2();
    }
    return (task->cpu_info->sys_reg->mpidr_el1 & 0xff);
}

list_t *get_vcpus(tcb_t *task)
{
    struct _vm_t *vm = NULL;
    if (!task)
    {
        task = curr_task_el2();
    }
    vm = task->curr_vm;
    return &vm->vcpus;
}
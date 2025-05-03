
#include <io.h>
#include <hyper/vcpu.h>
#include <mem/aj_string.h>
#include "os_cfg.h"

cpu_sysregs_t cpu_sysregs[MAX_TASKS];

// void print_vcpu(int id)
// {
// 	int reg;
// 	spin_lock(&vcpu[id].lock);
// 	for (reg = 0; reg < 31; reg++)
// 	{
// 		printf("R%d : %x\n", vcpu[id].ctx.r[reg]);
// 	}
// 	printf("SPSR : %x\n", vcpu[id].ctx.spsr);
// 	printf("LR : %x\n", vcpu[id].ctx.elr);
// 	spin_unlock(&vcpu[id].lock);
// }

#include "io.h"
#include "vmm/vcpu.h"
#include "lib/avatar_string.h"
#include "os_cfg.h"

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
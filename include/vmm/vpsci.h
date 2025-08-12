#ifndef HYPER_VPSCI_H
#define HYPER_VPSCI_H

#include "aj_types.h"
#include "task/task.h"

// 启动 guest vcpu
int32_t vpsci_cpu_on(trap_frame_t *ctx_el2);

#endif // HYPER_VPSCI_H

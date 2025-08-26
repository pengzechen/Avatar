/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file vpsci.h
 * @brief Implementation of vpsci.h
 * @author Avatar Project Team
 * @date 2024
 */

#ifndef HYPER_VPSCI_H
#define HYPER_VPSCI_H

#include "avatar_types.h"
#include "task/task.h"

// 启动 guest vcpu
int32_t
vpsci_cpu_on(trap_frame_t *ctx_el2);

int32_t
vpsci_cpu_reset(trap_frame_t *ctx_el2);

#endif  // HYPER_VPSCI_H

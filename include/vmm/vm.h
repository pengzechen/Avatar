/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file vm.h
 * @brief Implementation of vm.h
 * @author Avatar Project Team
 * @date 2024
 */


#ifndef __VM_H__
#define __VM_H__

#include "../avatar_types.h"

#include "vcpu.h"
#include "vgic.h"
#include "task/task.h"
#include "../guest/guest_manifest.h"

#define VM_NAME_MAX 64

typedef struct _tcb_t    tcb_t;
typedef struct _vgic_t   vgic_t;
typedef struct _vtimer_t vtimer_t;
typedef struct _vpl011_t vpl011_t;

struct _vm_t
{
    uint32_t vm_id;
    char     vm_name[VM_NAME_MAX];  // 虚拟机名称

    void *stage2pg_base;  // stage2 页表基地址

    uint64_t entry;  // 虚拟机入口地址
    uint32_t vcpu_cnt;
    list_t   vcpus;         // vcpu 列表
    tcb_t   *primary_vcpu;  // 主 vcpu

    vgic_t   *vgic;
    vtimer_t *vtimer;  // 虚拟定时器
    vpl011_t *vpl011;  // 虚拟串口

    // 新增：Guest类型和配置信息
    guest_type_t            guest_type;   // Guest类型
    const guest_manifest_t *manifest;     // Guest配置清单指针
    guest_load_result_t     load_result;  // 加载结果
};

static inline uint64_t
read_sctlr_el2()
{
    uint64_t value;
    asm volatile("mrs %0, sctlr_el2" : "=r"(value));
    return value;
}

static inline uint64_t
read_hcr_el2()
{
    uint64_t value;
    asm volatile("mrs %0, hcr_el2" : "=r"(value));
    return value;
}

static inline uint64_t
read_vttbr_el2()
{
    uint64_t value;
    asm volatile("mrs %0, vttbr_el2" : "=r"(value));
    return value;
}

struct _vm_t *
alloc_vm();
void
vm_init_with_manifest(struct _vm_t *vm, const guest_manifest_t *manifest);
void
run_vm(struct _vm_t *vm);

#endif  // __VM_H__
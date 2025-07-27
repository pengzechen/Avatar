
#ifndef __VM_H__
#define __VM_H__

#include "../aj_types.h"

#include "vcpu.h"
#include "vgic.h"
#include "task/task.h"

typedef struct _tcb_t tcb_t;

struct vm_t
{
    uint32_t id;

    list_t vpus;         // vcpu 列表
    tcb_t *primary_vcpu; // 主 vcpu

    struct vgic_t *vgic;
};

static inline uint64_t read_sctlr_el2()
{
    uint64_t value;
    asm volatile(
        "mrs %0, sctlr_el2"
        : "=r"(value));
    return value;
}

static inline uint64_t read_hcr_el2()
{
    uint64_t value;
    asm volatile(
        "mrs %0, hcr_el2"
        : "=r"(value));
    return value;
}

static inline uint64_t read_vttbr_el2()
{
    uint64_t value;
    asm volatile(
        "mrs %0, vttbr_el2"
        : "=r"(value));
    return value;
}

struct vm_t *alloc_vm();
void vm_init(struct vm_t *vm, int vcpu_num);
void run_vm(struct vm_t *vm);

#endif // __VM_H__
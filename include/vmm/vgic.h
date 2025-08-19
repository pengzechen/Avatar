/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file vgic.h
 * @brief Implementation of vgic.h
 * @author Avatar Project Team
 * @date 2024
 */



#ifndef __VGIC_H__
#define __VGIC_H__

#include "../gic.h"

#include "avatar_types.h"
#include "vm.h"
#include "vmm_cfg.h"
#include "gic.h"
#include "task/task.h"

typedef struct _tcb_t tcb_t;

typedef struct _vgic_core_state_t
{
    uint32_t id;

    uint32_t vmcr;  // GIC state;
    uint32_t saved_elsr0;
    uint32_t saved_apr;
    uint32_t saved_hcr;
    uint32_t saved_lr[GICH_LR_NUM];

    uint32_t irq_pending_mask[SPI_ID_MAX / 32];  // 记录处于挂起状态的中断（IRQ）
    uint32_t pending_lr[SPI_ID_MAX];

    uint32_t sgi_ppi_isenabler;  // SGI+PPI enable register (GICD_ISENABLER(0))
    uint8_t  sgi_ppi_ipriorityr[GIC_FIRST_SPI];
} vgic_core_state_t;

typedef struct _vgic_t
{
    struct _vm_t *vm;

    vgic_core_state_t *core_state[VCPU_NUM_MAX];

    // ctrlr
    uint32_t gicd_ctlr;
    // typer
    uint32_t gicd_typer;
    // iidr
    uint32_t gicd_iidr;

    // SPI interrupt enable registers (GICD_ISENABLER(1) and above)
    uint32_t gicd_scenabler[SPI_ID_MAX / 32];

    uint8_t  gicd_ipriorityr[SPI_ID_MAX];
    uint8_t  gicd_itargetsr[SPI_ID_MAX];
    uint32_t gicd_icfgr[SPI_ID_MAX / 16];

} vgic_t;


void
intc_handler(stage2_fault_info_t *info, trap_frame_t *el2_ctx);

void
vgic_hw_inject_test(uint32_t vector);
void
vgic_sw_inject_test(uint32_t vector);

vgic_t *
alloc_vgic();

vgic_core_state_t *
alloc_gicc();
vgic_core_state_t *
get_vgicc_by_vcpu(tcb_t *task);
void
vgicc_dump(vgic_core_state_t *vgicc);

extern void
gicc_save_core_state();
extern void
gicc_restore_core_state();

void
vgic_inject_ppi(tcb_t *task, int32_t irq_id);
void
vgic_inject_spi(tcb_t *task, int32_t irq_id);
void
vgic_inject_sgi(tcb_t *task, int32_t int_id);

void
vgic_try_inject_pending(tcb_t *task);

void
vgic_passthrough_irq(int32_t irq_id);


// 辅助函数：操作 irq_pending_mask 位图
static inline bool
vgic_is_irq_pending(vgic_core_state_t *vgicc, uint32_t irq_id)
{
    uint32_t word_idx = irq_id / 32;
    uint32_t bit_idx  = irq_id % 32;
    return (vgicc->irq_pending_mask[word_idx] & (1U << bit_idx)) != 0;
}

static inline void
vgic_set_irq_pending(vgic_core_state_t *vgicc, uint32_t irq_id)
{
    uint32_t word_idx = irq_id / 32;
    uint32_t bit_idx  = irq_id % 32;
    vgicc->irq_pending_mask[word_idx] |= (1U << bit_idx);
}

static inline void
vgic_clear_irq_pending(vgic_core_state_t *vgicc, uint32_t irq_id)
{
    uint32_t word_idx = irq_id / 32;
    uint32_t bit_idx  = irq_id % 32;
    vgicc->irq_pending_mask[word_idx] &= ~(1U << bit_idx);
}

// 获取 SGI/PPI 的完整 pending 状态（软件 + 硬件）
static uint32_t
vgic_get_sgi_ppi_pending_status(vgic_core_state_t *vgicc)
{
    uint32_t pending_status = 0;

    // 1. 从软件设置的 pending 状态开始
    pending_status = vgicc->irq_pending_mask[0];

    // 2. 检查 GICH_LR 中的硬件注入 pending 状态
    for (int32_t lr = 0; lr < GICH_LR_NUM; lr++) {
        uint32_t lr_val = vgicc->saved_lr[lr];
        if (lr_val != 0)  // LR 不为空
        {
            uint32_t vid   = lr_val & 0x3ff;        // Virtual ID
            uint32_t state = (lr_val >> 28) & 0x3;  // State field

            // 如果是 SGI/PPI 且处于 pending 状态
            if (vid < 32 && (state == 1))  // state=1 表示 pending
            {
                pending_status |= (1U << vid);
            }
        }
    }

    return pending_status;
}


#endif  // __VGIC_H__
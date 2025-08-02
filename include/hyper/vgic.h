

#ifndef __VGIC_H__
#define __VGIC_H__

#include "../gic.h"

#include "aj_types.h"
#include "vm.h"
#include "hyper_cfg.h"
#include "gic.h"

typedef struct
{
    uint32_t id;

    uint32_t vmcr; // GIC state;
    uint32_t saved_elsr0;
    uint32_t saved_apr;
    uint32_t saved_hcr;
    uint32_t saved_lr[GICH_LR_NUM];
    
    uint32_t irq_pending_mask[SPI_ID_MAX / 32]; // 记录处于挂起状态的中断（IRQ）
    uint32_t pending_lr[SPI_ID_MAX];

    uint32_t ppi_isenabler;
    uint8_t ppi_ipriorityr[GIC_FIRST_SPI];
} vgic_core_state_t;

struct vgic_t
{
    struct _vm_t *vm;

    uint32_t ptov[SPI_ID_MAX]; // 实际中断 ID 到虚拟中断 ID 的映射
    uint32_t vtop[SPI_ID_MAX];
    uint32_t use_irq[SPI_ID_MAX / 32]; // 一个位掩码数组，标记哪些实际中断 ID 被 VGIC 使用
    uint32_t real_pri;                 // 设置和管理实际中断的优先级

    vgic_core_state_t *core_state[VCPU_NUM_MAX];

    // ctrlr
    uint32_t gicd_ctlr;
    // typer
    uint32_t gicd_typer;
    // iidr
    uint32_t gicd_iidr;
    
    uint32_t gicd_igroupr[SPI_ID_MAX / 32];
    uint32_t gicd_isenabler[SPI_ID_MAX / 32];

    uint8_t gicd_ipriorityr[SPI_ID_MAX];
    uint8_t gicd_itargetsr[SPI_ID_MAX];

    uint32_t gicd_icfgr[SPI_ID_MAX / 16];
    uint32_t gicd_nsacr[SPI_ID_MAX / 16];
};

void virtual_gic_register_int(struct vgic_t *vgic, uint32_t pintvec, uint32_t vintvec);

void v_timer_handler();

void intc_handler(stage2_fault_info_t *info, trap_frame_t *el2_ctx);

void vgic_hw_inject_test(uint32_t vector);
void vgic_sw_inject_test(uint32_t vector);

struct vgic_t *alloc_vgic();

vgic_core_state_t *alloc_gicc() ;

void vgicc_dump(vgic_core_state_t *vgicc);

extern void gicc_save_core_state();

extern void gicc_restore_core_state();


#endif // __VGIC_H__
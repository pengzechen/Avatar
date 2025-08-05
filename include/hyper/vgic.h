

#ifndef __VGIC_H__
#define __VGIC_H__

#include "../gic.h"

#include "aj_types.h"
#include "vm.h"
#include "hyper_cfg.h"
#include "gic.h"
#include "task/task.h"

typedef struct _tcb_t tcb_t;

typedef struct _vgic_core_state_t
{
    uint32_t id;

    uint32_t vmcr; // GIC state;
    uint32_t saved_elsr0;
    uint32_t saved_apr;
    uint32_t saved_hcr;
    uint32_t saved_lr[GICH_LR_NUM];

    uint32_t irq_pending_mask[SPI_ID_MAX / 32]; // 记录处于挂起状态的中断（IRQ）
    uint32_t pending_lr[SPI_ID_MAX];

    uint32_t sgi_ppi_isenabler; // SGI+PPI enable register (GICD_ISENABLER(0))
    uint8_t sgi_ppi_ipriorityr[GIC_FIRST_SPI];
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
    
    uint8_t gicd_ipriorityr[SPI_ID_MAX];
    uint8_t gicd_itargetsr[SPI_ID_MAX];
    uint32_t gicd_icfgr[SPI_ID_MAX / 16];

} vgic_t;

void v_timer_handler();

void intc_handler(stage2_fault_info_t *info, trap_frame_t *el2_ctx);

void vgic_hw_inject_test(uint32_t vector);
void vgic_sw_inject_test(uint32_t vector);

vgic_t *alloc_vgic();

vgic_core_state_t *alloc_gicc();

void vgicc_dump(vgic_core_state_t *vgicc);

extern void gicc_save_core_state();

extern void gicc_restore_core_state();

void vgic_inject_ppi(tcb_t *task, int32_t irq_id);
void vgic_passthrough_irq(int32_t irq_id);

#endif // __VGIC_H__
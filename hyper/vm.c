
#include <hyper/vgic.h>
#include <hyper/hyper_cfg.h>
#include <aj_types.h>
#include "gic.h"

// qemu 准备启动4个核
// 每个核跑两个 vcpu， 共8个vcpu
// 跑4个vm每个vm使用2个vcpu 或者 跑2个vm每个vm使用4个vcpu

static uint32_t _vgic_num = 0;
vgic_core_state_t _state[VCPU_NUM_MAX];
static uint32_t _state_num = 0;

#define HV_TIMER_VECTOR 27
#define PL011_INT 33

void v_timer_handler()
{
    // logger("v_timer_handler\n");
    vgic_inject(HV_TIMER_VECTOR);
}

void fake_console()
{
    // logger("fake console\n");
    vgic_inject(PL011_INT);
}

void vm_init()
{
    virtual_gic_register_int(get_vgic(0), HV_TIMER_VECTOR, HV_TIMER_VECTOR);

    virtual_gic_register_int(get_vgic(0), PL011_INT, PL011_INT);

    irq_install(HV_TIMER_VECTOR, v_timer_handler);

    irq_install(PL011_INT, fake_console);
}

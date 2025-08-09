
#include <timer.h>
#include <gic.h>
#include <aj_types.h>
#include <io.h>
#include <exception.h>
#include <thread.h>
#include <task/task.h>
#include <hyper/vgic.h>
#include <os_cfg.h>

static uint64_t test_num = 0;

extern void v_timer_tick(uint64_t now);


void handle_timer_interrupt(uint64_t *sp)
{
#if HV
    // Hypervisor Timer - 设置定时值
    write_cnthp_tval_el2(TIMER_TVAL_VALUE);
#else
    // Physical Timer - 设置定时值
    write_cntp_tval_el0(TIMER_TVAL_VALUE);
#endif
    timer_tick_schedule(sp);
    v_timer_tick(read_cntpct_el0());
}

void timer_init_second()
{
    uint64_t frq = read_cntfrq_el0();

    logger("timer frq: %d\n", frq);

#if HV
    // Hypervisor Timer 初始化
    logger("Initializing Hypervisor Timer (Vector %d)\n", TIMER_VECTOR);
    write_cnthp_tval_el2(TIMER_TVAL_VALUE);
    write_cnthp_ctl_el2(0b1);  // 启用 Hypervisor Timer
#else
    // Physical Timer 初始化
    logger("Initializing Physical Timer (Vector %d)\n", TIMER_VECTOR);
    write_cntp_tval_el0(TIMER_TVAL_VALUE);
    write_cntp_ctl_el0(0b1);   // 启用 Physical Timer
#endif

    gic_enable_int(TIMER_VECTOR, 1);

    if (gic_get_enable(TIMER_VECTOR))
    {
        logger("timer enabled successfully ...\n");
    }
    // gic_set_target(TIMER_VECTOR, 0b00000010);
    gic_set_ipriority(TIMER_VECTOR, 1);
}

// 每个pe都要配置
void timer_init()
{
    uint64_t frq = read_cntfrq_el0();

    logger("timer frq: %d\n", frq);

#if HV
    // Hypervisor Timer 初始化
    logger("Initializing Hypervisor Timer (Vector %d)\n", TIMER_VECTOR);
    write_cnthp_tval_el2(TIMER_TVAL_VALUE);
    write_cnthp_ctl_el2(0b1);  // 启用 Hypervisor Timer
#else
    // Physical Timer 初始化
    logger("Initializing Physical Timer (Vector %d)\n", TIMER_VECTOR);
    write_cntp_tval_el0(TIMER_TVAL_VALUE);
    write_cntp_ctl_el0(0b1);   // 启用 Physical Timer
#endif

    // 统一使用 handle_timer_interrupt，它内部会根据HV宏选择正确的寄存器
    irq_install(TIMER_VECTOR, handle_timer_interrupt);

    gic_enable_int(TIMER_VECTOR, 1);

    if (gic_get_enable(TIMER_VECTOR))
    {
        logger("timer enabled successfully ...\n");
    }
    // gic_set_target(TIMER_VECTOR, 0b00000001);
    gic_set_ipriority(TIMER_VECTOR, 1);
}
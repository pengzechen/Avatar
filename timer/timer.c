
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
    // 设置定时值 - 使用配置常量
    write_cntp_tval_el0(TIMER_TVAL_VALUE);
    timer_tick_schedule(sp);
    v_timer_tick(read_cntpct_el0());
}

void timer_init_second()
{
    uint64_t frq = read_cntfrq_el0();

    logger("timer frq: %d\n", frq);

    // 设置定时值 - 使用配置常量
    write_cntp_tval_el0(TIMER_TVAL_VALUE);
    // 启用定时器
    write_cntp_ctl_el0(0b1);

    gic_enable_int(TIMER_VECTOR, 1);

    if (gic_get_enable(TIMER_VECTOR))
    {
        logger("timer enabled successfully ...\n");
    }
}

// 每个pe都要配置
void timer_init()
{

    uint64_t frq = read_cntfrq_el0();

    logger("timer frq: %d\n", frq);

    // 设置定时值 - 使用配置常量
    write_cntp_tval_el0(TIMER_TVAL_VALUE);
    // 启用定时器
    write_cntp_ctl_el0(0b1);

    irq_install(TIMER_VECTOR, handle_timer_interrupt);

    gic_enable_int(TIMER_VECTOR, 1);

    if (gic_get_enable(TIMER_VECTOR))
    {
        logger("timer enabled successfully ...\n");
    }
}


#include <timer.h>
#include <gic.h>
#include <aj_types.h>
#include <io.h>
#include <exception.h>
#include <thread.h>
#include <task.h>
#include <hyper/vgic.h>

static uint64_t test_num = 0;

#define TIMER_VECTOR 30


void handle_timer_interrupt(uint64_t *sp)
{
    // 设置定时值
    write_cntp_tval_el0(625000);
    // if (test_num++ % 1000 == 0) {
    //     printf("timer: 10s\n");
    // }
    // printf("core: %d, handle irq exception\n", get_current_cpu_id());
    // printf("get daif: %x\n", get_daif());
    timer_tick_schedule(sp);
    // schedule();

    // 这里给其它的irq提高优先级
    gic_set_ipriority(6, 0xf0000000);
    gic_set_ipriority(8, 0x0000f000);
}

void timer_init_second()
{
    uint64_t frq = read_cntfrq_el0();

    printf("timer frq: %d\n", frq);

    // 设置定时值
    write_cntp_tval_el0(625000);
    // 启用定时器
    write_cntp_ctl_el0(0b1);

    gic_enable_int(TIMER_VECTOR, 0);

    if (gic_get_enable(TIMER_VECTOR))
    {
        printf("timer enabled successfully ...\n");
    }
}


// 每个pe都要配置
void timer_init()
{

    uint64_t frq = read_cntfrq_el0();

    printf("timer frq: %d\n", frq);

    // 设置定时值
    write_cntp_tval_el0(625000);
    // 启用定时器
    write_cntp_ctl_el0(0b1);

    irq_install(TIMER_VECTOR, handle_timer_interrupt);

    gic_enable_int(TIMER_VECTOR, 0);

    if (gic_get_enable(TIMER_VECTOR))
    {
        printf("timer enabled successfully ...\n");
    }

}

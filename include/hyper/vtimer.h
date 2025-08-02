
#ifndef __VTIMER_H__
#define __VTIMER_H__

#include <aj_types.h>
#include "task/task.h"
#include "exception.h"

typedef struct _tcb_t tcb_t;

// 虚拟定时器核心状态（每个 vCPU 独有）
typedef struct _vtimer_core_state_t
{
    uint32_t id;  // vCPU ID

    // 虚拟定时器寄存器状态
    uint64_t cntvct_offset;     // 虚拟计数器偏移
    uint64_t cntv_cval;         // 虚拟定时器比较值寄存器
    uint32_t cntv_ctl;          // 虚拟定时器控制寄存器

    // 定时器状态
    uint64_t deadline;          // 下一次触发的时间戳（物理时间）
    bool enabled;               // 是否启用
    bool pending;               // 是否有待处理的中断

    // 统计信息
    uint64_t fire_count;        // 触发次数
    uint64_t last_fire_time;    // 上次触发时间
} vtimer_core_state_t;

// 前向声明
struct _vm_t;

// 虚拟定时器管理结构（每个 VM 一个）
typedef struct _vtimer_t
{
    struct _vm_t *vm;  // 关联的虚拟机
    vtimer_core_state_t *core_state[VCPU_NUM_MAX];  // 每个 vCPU 的定时器状态指针
    uint32_t vcpu_cnt;  // vCPU 数量
} vtimer_t;

static inline uint64_t read_cntvct_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

// 写 CNTV_CTL_EL0 （虚拟定时器控制寄存器）
static inline void write_cntv_ctl_el0(uint64_t val)
{
    asm volatile("msr cntv_ctl_el0, %0" : : "r"(val));
}

// 读 CNTV_CTL_EL0
static inline uint64_t read_cntv_ctl_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntv_ctl_el0" : "=r"(val));
    return val;
}

// 写 CNTV_TVAL_EL0 （虚拟定时器定时值寄存器）
static inline void write_cntv_tval_el0(uint64_t val)
{
    asm volatile("msr cntv_tval_el0, %0" : : "r"(val));
}

// 读 CNTV_TVAL_EL0
static inline uint64_t read_cntv_tval_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntv_tval_el0" : "=r"(val));
    return val;
}

// 写 CNTV_CVAL_EL0 （虚拟定时器比较值寄存器）
static inline void write_cntv_cval_el0(uint64_t val)
{
    asm volatile("msr cntv_cval_el0, %0" : : "r"(val));
}

// 读 CNTV_CVAL_EL0
static inline uint64_t read_cntv_cval_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntv_cval_el0" : "=r"(val));
    return val;
}


// 函数声明
void vtimer_global_init(void);
vtimer_t *alloc_vtimer(void);
vtimer_core_state_t *alloc_vtimer_core(void);  // 类似 alloc_gicc
vtimer_core_state_t *alloc_vtimer_core_state(uint32_t vcpu_id);
vtimer_core_state_t *get_vtimer_by_vcpu(tcb_t *task);
tcb_t *get_task_by_vcpu_id(uint32_t vcpu_id);

void vtimer_core_init(vtimer_core_state_t *vt, uint32_t vcpu_id);
void vtimer_set_timer(vtimer_core_state_t *vt, uint64_t cval, uint32_t ctl);
bool vtimer_should_fire(vtimer_core_state_t *vt, uint64_t now);
void vtimer_inject_to_vcpu(tcb_t *task);

// 定时器寄存器访问
uint64_t vtimer_read_cntvct(vtimer_core_state_t *vt);                // Guest 读取虚拟计数器时调用
void vtimer_write_cntv_cval(vtimer_core_state_t *vt, uint64_t cval); // Guest 设置定时器比较值时调用
void vtimer_write_cntv_ctl(vtimer_core_state_t *vt, uint32_t ctl);   // Guest 启用/禁用定时器时调用
uint32_t vtimer_read_cntv_ctl(vtimer_core_state_t *vt);

// 主机定时器中断处理（保持原有签名）
void v_timer_handler(uint64_t * nouse);

// 系统寄存器访问处理
bool handle_vtimer_sysreg_access(stage2_fault_info_t *info, trap_frame_t *ctx);

#endif // __VTIMER_H__

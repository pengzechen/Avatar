
#ifndef __TASK_H__
#define __TASK_H__

#include "aj_types.h"
#include "hyper/vcpu.h"
#include "list.h"


#pragma pack(1)
typedef struct _contex_t {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;       // Stack Frame Pointer
    uint64_t x30;       // Link register (the address to return)
    uint64_t tpidr_el0; // "Thread ID" Register
    uint64_t sp_el1;
} contex_t ;
#pragma pack()


typedef struct _cpu_t
{
	cpu_ctx_t     ctx;
	cpu_ctx_t     *pctx;   // 指向trap的栈，可以修改restore的数据
	cpu_sysregs_t *sys_reg;
	spinlock_t    lock;
} cpu_t ;

extern cpu_t vcpu[];

typedef enum _task_state_t {
    TASK_STATE_CREATE = 1,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_WAITING,
} task_state_t;

#pragma pack(1)
typedef struct
{   
    contex_t         ctx;
    cpu_t            *cpu_info;
    uint64_t         *sp;
    
    task_state_t state; 
    uint32_t counter;
    uint32_t priority;
    uint32_t id;         // 任务ID
    
    list_node_t process; // 属于哪个进程
} tcb_t;
#pragma pack()

#define wfi()       __asm__ volatile("wfi" : : : "memory")


void timer_tick_schedule(uint64_t *);
void print_current_task_list();

// @param: task_func: el0 任务真正的入口, sp: el0 任务的内核栈
tcb_t *create_task(
    void (*task_func)(),  // el0 任务真正的入口
    uint64_t               // el0 任务的内核栈
    );

tcb_t *craete_vm_task(void (*task_func)(), uint64_t stack_top);
void schedule_init();
void schedule_init_local(tcb_t *task, void * new_sp);


#endif // __TASK_H__
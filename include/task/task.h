
#ifndef __TASK_H__
#define __TASK_H__

#include "aj_types.h"
#include "hyper/vcpu.h"
#include "list.h"
#include "os_cfg.h"
#include "pro.h"

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
    uint64_t tpidr_elx; // "Thread ID" Register
    uint64_t sp_elx;
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
    uint64_t         sp;
    
    task_state_t state; 
    uint32_t counter;
    uint32_t sleep_ticks;
    uint32_t priority;
    uint32_t id;         // 任务ID
    uint64_t pgdir;
    
    list_node_t process; // 属于哪个进程

    list_node_t run_node;		// 运行相关结点
	list_node_t wait_node;		// 等待队列
	list_node_t all_node;		// 所有队列结点

    struct _process_t * curr_pro; // 当前进程
} tcb_t;
#pragma pack()

#define wfi()       __asm__ volatile("wfi" : : : "memory")

typedef struct _task_manager_t {

	list_t ready_list;			// 就绪队列
	list_t task_list;			// 所有已创建任务的队列
	list_t sleep_list;          // 延时队列
	
    tcb_t idle_task[SMP_NUM];			// 空闲任务
    cpu_t idle_cpu[SMP_NUM];
    spinlock_t lock;

}task_manager_t;


void timer_tick_schedule(uint64_t *);
void print_current_task_list();

// @param: task_func: el0 任务真正的入口, sp: el0 任务的内核栈
tcb_t *create_task(
    void (*task_func)(),  // el0 任务真正的入口
    uint64_t,               // el0 任务的内核栈
    uint32_t
    );

tcb_t *craete_vm_task(
    void (*task_func)(), 
    uint64_t stack_top,
    uint32_t
    );

void schedule_init();
void task_manager_init(void);
task_manager_t * get_task_manager() ;
void schedule_init_local(tcb_t *task, void * new_sp);
void task_set_ready(tcb_t *task) ;
void task_set_block (tcb_t *task);
void schedule();

tcb_t * get_idle() ;
void el1_idle_init(); // 初始化空闲任务
uint64_t get_idle_sp_top() ;

// 将来进程初始化一个任务，需要为这个任务分配准备页表基址
void set_tcb_pgdir(tcb_t * task, uint64_t pgdir);

// 系统调用
void sys_sleep_tick(uint64_t ms);





#endif // __TASK_H__
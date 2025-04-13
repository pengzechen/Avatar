
#include <aj_types.h>
#include <io.h>
#include <exception.h>
#include <gic.h>
#include <timer.h>
#include <hyper/vcpu.h>
#include <thread.h>

extern void *syscall_table[];

// 示例使用方式：处理同步异常
void handle_sync_exception(uint64_t *stack_pointer)
{
    trap_frame_t *el1_ctx = (trap_frame_t *)stack_pointer;

    int el1_esr = read_esr_el1();

    int ec = ((el1_esr >> 26) & 0b111111);

    save_cpu_ctx(el1_ctx);   // 保存 发生异常的那个任务的 处理器状态
    
    if (ec == 0x15)
    { // svc
        // enable_interrupts();
        // printf("This is svc call handler\n");
        // printf("svc number: %x\n", el1_ctx->r[8]);
        uint64_t (*func)(void*) = (uint64_t (*)(void*)) syscall_table[el1_ctx->r[8]];
        // 传进去的应该是一个指针，指针偏移 0-7 是参数。现在只支持最多 8 个参数
        uint64_t ret = func((void*)el1_ctx);
        // 返回值放在 x0
        el1_ctx->r[0] = ret;
        // disable_interrupts();
        return;
    }

    if (ec == 0x17)
    { // smc
        printf("This is smc call handler\n");
        return;
    }

    /* =============== UnDefined ================== */
    if (get_current_cpu_id() == 0) {
        printf("el1 esr: %x\n", el1_esr);
        printf("ec: %x\n", ec);
        printf("This is handle_sync_exception: \n");
        for (int i = 0; i < 31; i++)
        {
            uint64_t value = el1_ctx->r[i];
            printf("General-purpose register: %d, value: %x\n", i, value);
        }
        uint64_t elr_el1_value = el1_ctx->elr;
        uint64_t usp_value = el1_ctx->usp;
        uint64_t spsr_value = el1_ctx->spsr;
        printf("usp: %x, elr: %x, spsr: %x\n", usp_value, elr_el1_value, spsr_value);
    }
    while (1)
        ;
}

irq_handler_t g_handler_vec[512] = {0};

irq_handler_t *get_g_handler_vec()
{
    return g_handler_vec;
}

void irq_install(int vector, void (*h)(uint64_t *))
{
    g_handler_vec[vector] = h;
}

// 示例使用方式：处理 IRQ 异常
void handle_irq_exception(uint64_t *stack_pointer)
{
    trap_frame_t *el1_ctx = (trap_frame_t *)stack_pointer;

    uint64_t x1_value = el1_ctx->r[1];
    uint64_t sp_el0_value = el1_ctx->usp;
    save_cpu_ctx(el1_ctx);   // 保存 发生异常的那个任务的 处理器状态

    int iar = gic_read_iar();
    int vector = gic_iar_irqnr(iar);
    gic_write_eoir(iar);

    g_handler_vec[vector]((uint64_t *)el1_ctx); // arg not use
}

// 示例使用方式：处理无效异常
void invalid_exception(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{
    trap_frame_t *el1_ctx = (trap_frame_t *)stack_pointer;

    uint64_t x2_value = el1_ctx->r[2];

    // 在这里实现处理无效异常的代码
}
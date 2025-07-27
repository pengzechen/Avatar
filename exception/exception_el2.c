
#include <aj_types.h>
#include <io.h>
#include <exception.h>
#include <gic.h>
#include <mem/ept.h>
#include <hyper/vcpu.h>
#include <psci.h>
#include "task/task.h"
#include "thread.h"

void advance_pc(ept_violation_info_t *info, trap_frame_t *context)
{
    context->elr += info->hsr.len ? 4 : 2;
}

// 示例使用方式：处理同步异常
void handle_sync_exception_el2(uint64_t *stack_pointer)
{
    trap_frame_t *ctx_el2 = (trap_frame_t *)stack_pointer;

    int el2_esr = read_esr_el2();

    int ec = ((el2_esr >> 26) & 0b111111);

    // logger("        el2 esr: %llx, ec: %llx\n", el2_esr, ec);

    union hsr hsr = {.bits = el2_esr};
    save_cpu_ctx(ctx_el2);
    if (ec == 0x1)
    {
        // wfi
        ept_violation_info_t info;
        // logger("Prefetch abort : %llx\n", hsr.bits);
        info.hsr.bits = hsr.bits;
        logger_info("            This is wfi trap handler\n");
        advance_pc(&info, ctx_el2);
        return;
    }
    else if (ec == 0x16)
    { // hvc
        ept_violation_info_t info;
        info.hsr.bits = hsr.bits;
        logger_info("            This is hvc call handler\n");
        logger_warn("           function id: %lx\n", ctx_el2->r[0]);
        if (ctx_el2->r[0] == PSCI_0_2_FN64_CPU_ON)
        {   
            logger_info("           This is cpu on handler\n");
            uint64_t cpu_id = ctx_el2->r[1];
            uint64_t entry = ctx_el2->r[2];
            uint64_t context = ctx_el2->r[3];
            logger_info("           cpu_id: %d, entry: 0x%llx, sp: 0x%llx\n", cpu_id, entry, context);
            

            tcb_t *curr = (tcb_t *)read_tpidr_el2();
            struct vm_t *vm = curr->vm;
            
            list_node_t *iter = list_first(&vm->vpus);
            tcb_t *task = NULL;
            while (iter)
            {
                task = list_node_parent(iter, tcb_t, vm_node);
                if (task->cpu_info->sys_reg->mpidr_el1 == cpu_id) {
                    logger_info("           found a vcpu, task id: %d\n", task->id);
                    trap_frame_t * frame = (trap_frame_t *)task->ctx.sp_elx;
                    frame->elr = entry; // 设置 elr
                    task_set_ready(task); // 设置为就绪状态
                }
                iter = list_node_next(iter);
            }
            logger("\n");
        }
        advance_pc(&info, ctx_el2);
        ctx_el2->r[0] = 0; // SMC 返回值
        return;
    }
    else if (ec == 0x17)
    { // smc
        ept_violation_info_t info;
        // logger("Prefetch abort : %llx\n", hsr.bits);
        info.hsr.bits = hsr.bits;
        logger_info("            This is smc call handler\n");
        advance_pc(&info, ctx_el2);
        ctx_el2->r[0] = 0; // SMC 返回值
        return;
    }
    else if (ec == 0x18)
    {
        logger_info("            This is sys instr handler\n");
    }
    else if (ec == 0x24)
    { // data abort
        // logger_info("            This is data abort handler\n");
        ept_violation_info_t info;
        // logger("Prefetch abort : %llx\n", hsr.bits);
        info.hsr.bits = hsr.bits;
        info.reason = PREFETCH;
        uint64_t hpfar = read_hpfar_el2(); // 目前 hpfar 和 far 读到的内容不同，少了8位
        uint64_t far = read_far_el2();
        // logger("far: 0x%llx, hpfar: 0x%llx\n", far, hpfar);
        info.gpa = (far & 0xfff) | (hpfar << 8);
        info.gva = far;

        // gva_to_ipa(info.gva, &info.gpa);
        data_abort_handler(&info, ctx_el2);

        advance_pc(&info, ctx_el2);
        return;
    }

    for (int i = 0; i < 31; i++)
    {
        uint64_t value = ctx_el2->r[i];
        logger("General-purpose register: %d, value: %llx\n", i, value);
    }

    uint64_t elr_el1_value = ctx_el2->elr;
    uint64_t usp_value = ctx_el2->usp;
    uint64_t spsr_value = ctx_el2->spsr;

    logger("usp: %llx, elr: %llx, spsr: %llx\n", usp_value, elr_el1_value, spsr_value);

    while (1)
        ;
}

// 示例使用方式：处理 IRQ 异常
void handle_irq_exception_el2(uint64_t *stack_pointer)
{
    trap_frame_t *context = (trap_frame_t *)stack_pointer;

    uint64_t x1_value = context->r[1];
    uint64_t sp_el0_value = context->usp;

    int iar = gic_read_iar();
    int vector = gic_iar_irqnr(iar);
    gic_write_eoir(iar);

    if (vector != 27 && vector != 33)
    {
        /*
         * 你手动注入给 guest 的 virtual interrupt（虚拟硬件中断），不能在 EL2 写 DIR！
         * 否则会直接把这个中断标记为 inactive，guest 就根本收不到。
         */
        gic_write_dir(iar);
    }

    save_cpu_ctx(context);

    get_g_handler_vec()[vector]((uint64_t *)context); // arg not use
}

// 示例使用方式：处理无效异常
void invalid_exception_el2(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{

    trap_frame_t *context = (trap_frame_t *)stack_pointer;

    uint64_t x2_value = context->r[2];

    logger("invalid_exception_el2, kind: %d, source: %d\n", kind, source);

    int el2_esr = read_esr_el2();

    int ec = ((el2_esr >> 26) & 0b111111);

    logger("        el2 esr: %llx\n", el2_esr);
    logger("        ec: %llx\n", ec);

    for (int i = 0; i < 31; i++)
    {
        uint64_t value = context->r[i];
        logger("General-purpose register: %d, value: %llx\n", i, value);
    }

    uint64_t elr_el1_value = context->elr;
    uint64_t usp_value = context->usp;
    uint64_t spsr_value = context->spsr;

    logger("usp: %llx, elr: %llx, spsr: %llx\n", usp_value, elr_el1_value, spsr_value);

    while (1)
        ;
}

// 调用 handle_irq_exception_el2
void current_spxel_irq(uint64_t *stack_pointer)
{
    logger("irq stay in same el\n");
    handle_irq_exception_el2(stack_pointer);
}
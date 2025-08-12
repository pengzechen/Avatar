
#include "aj_types.h"
#include "io.h"
#include "exception.h"
#include "gic.h"
#include "mem/stage2page.h"
#include "vmm/vcpu.h"
#include "vmm/vtimer.h"
#include "psci.h"
#include "vmm/vpsci.h"
#include "task/task.h"
#include "thread.h"

void advance_pc(stage2_fault_info_t *info, trap_frame_t *context)
{
    context->elr += info->hsr.len ? 4 : 2;
}

void decode_spsr(uint64_t spsr) {
    // M[3:0] - Exception level and SP selection (bits 3:0)
    uint64_t m_field = spsr & 0xF;
    const char *el_str;

    switch (m_field) {
        case 0x0: el_str = "EL0t"; break;      // EL0 using SP_EL0
        case 0x4: el_str = "EL1t"; break;      // EL1 using SP_EL0
        case 0x5: el_str = "EL1h"; break;      // EL1 using SP_EL1
        case 0x8: el_str = "EL2t"; break;      // EL2 using SP_EL0
        case 0x9: el_str = "EL2h"; break;      // EL2 using SP_EL2
        case 0xC: el_str = "EL3t"; break;      // EL3 using SP_EL0
        case 0xD: el_str = "EL3h"; break;      // EL3 using SP_EL3
        default:  el_str = "Reserved"; break;
    }

    logger("SPSR_EL2 decode (0x%llx):\n", spsr);
    logger("  M[3:0]: 0x%x -> %s\n", (uint32_t)m_field, el_str);
    logger("  M[4] (Execution state): %s\n", ((spsr >> 4) & 1) ? "AArch32" : "AArch64");
    logger("  F (FIQ masked): %s\n", ((spsr >> 6) & 1) ? "Yes" : "No");
    logger("  I (IRQ masked): %s\n", ((spsr >> 7) & 1) ? "Yes" : "No");
    logger("  A (SError masked): %s\n", ((spsr >> 8) & 1) ? "Yes" : "No");
    logger("  D (Debug masked): %s\n", ((spsr >> 9) & 1) ? "Yes" : "No");

    // 额外的状态位
    if ((spsr >> 10) & 0x3) {
        logger("  BTYPE: 0x%x\n", (uint32_t)((spsr >> 10) & 0x3));
    }
    if ((spsr >> 20) & 1) {
        logger("  SS (Software Step): Set\n");
    }
    if ((spsr >> 21) & 1) {
        logger("  PAN (Privileged Access Never): Set\n");
    }
    if ((spsr >> 22) & 1) {
        logger("  UAO (User Access Override): Set\n");
    }
    if ((spsr >> 23) & 1) {
        logger("  DIT (Data Independent Timing): Set\n");
    }
    if ((spsr >> 24) & 1) {
        logger("  TCO (Tag Check Override): Set\n");
    }

    // 条件标志位
    logger("  NZCV: N=%d Z=%d C=%d V=%d\n",
           (int)((spsr >> 31) & 1),  // N
           (int)((spsr >> 30) & 1),  // Z
           (int)((spsr >> 29) & 1),  // C
           (int)((spsr >> 28) & 1)); // V
}


// 示例使用方式：处理同步异常
void handle_sync_exception_el2(uint64_t *stack_pointer)
{
    trap_frame_t *ctx_el2 = (trap_frame_t *)stack_pointer;

    int32_t el2_esr = read_esr_el2();

    int32_t ec = ((el2_esr >> 26) & 0b111111);

    // logger("        el2 esr: %llx, ec: %llx\n", el2_esr, ec);

    union hsr hsr = {.bits = el2_esr};
    save_cpu_ctx(ctx_el2);
    if (ec == 0x1)
    {
        // wfi
        stage2_fault_info_t info;
        // logger("Prefetch abort : %llx\n", hsr.bits);
        info.hsr.bits = hsr.bits;
        logger_info("            This is wfi trap handler\n");
        advance_pc(&info, ctx_el2);
        return;
    }
    else if (ec == 0x16)
    { // hvc
        stage2_fault_info_t info;
        info.hsr.bits = hsr.bits;
        logger_info("           This is hvc call handler\n");
        logger_warn("           function id: %lx\n", ctx_el2->r[0]);
        if (ctx_el2->r[0] == PSCI_0_2_FN64_CPU_ON)
        {
            logger_info("           This is cpu on handler\n");
            int32_t ret = vpsci_cpu_on(ctx_el2);
            ctx_el2->r[0] = ret; // SMC 返回值
        }
        advance_pc(&info, ctx_el2);
        return;
    }
    else if (ec == 0x17)
    { // smc
        stage2_fault_info_t info;
        // logger("Prefetch abort : %llx\n", hsr.bits);
        info.hsr.bits = hsr.bits;
        logger_info("            This is smc call handler\n");
        advance_pc(&info, ctx_el2);
        ctx_el2->r[0] = 0; // SMC 返回值
        return;
    }
    else if (ec == 0x18)
    {
        // 系统指令异常 - 处理 Guest 对定时器寄存器的访问
        logger_info("            This is sys instr handler\n");
        stage2_fault_info_t info;
        info.hsr.bits = hsr.bits;

        // 调用虚拟定时器处理函数
        // if (handle_vtimer_sysreg_access(&info, ctx_el2)) {
        //     advance_pc(&info, ctx_el2);
        //     return;
        // }

        // 如果不是定时器寄存器访问，继续原有处理
        logger_warn("Unhandled system register access: 0x%x\n", hsr.bits);
    }
    else if (ec == 0x20) 
    {
        logger_info("            This is illegal exec state handler\n");
        stage2_fault_info_t info;
        info.hsr.bits = hsr.bits;
        logger("Guest exception: FAR_EL2=0x%lx\n", read_far_el2());
        logger("        el2 esr: %llx, ec: %llx\n", el2_esr, ec);
        decode_spsr(ctx_el2->spsr);

        uint64_t sp_el1;   // el1 h
        asm volatile("mrs %0, sp_el1" : "=r"(sp_el1));
        logger("sp_el1: 0x%lx\n", sp_el1);

        uint64_t sp_el0;  // el1 t
        asm volatile("mrs %0, sp_el0" : "=r"(sp_el0));
        logger("sp_el0: 0x%lx\n", sp_el0);

        
        advance_pc(&info, ctx_el2);
        // while(1);
        // return;
    }
    else if (ec == 0x24)
    { // data abort
        // logger_info("            This is data abort handler\n");
        stage2_fault_info_t info;
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

    for (int32_t i = 0; i < 31; i++)
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

    int32_t iar = gic_read_iar();
    int32_t vector = gic_iar_irqnr(iar);
    gic_write_eoir(iar);

    /*
        * 你手动注入给 guest 的 virtual interrupt（虚拟硬件中断），不能在 EL2 写 DIR！
        * 否则会直接把这个中断标记为 inactive，guest 就根本收不到。
        * 需要透穿的不写 dir
        */
    gic_write_dir(iar);
    

    save_cpu_ctx(context);

    get_g_handler_vec()[vector]((uint64_t *)context); // arg not use
}

// 示例使用方式：处理无效异常
void invalid_exception_el2(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{

    trap_frame_t *context = (trap_frame_t *)stack_pointer;

    uint64_t x2_value = context->r[2];

    logger("invalid_exception_el2, kind: %d, source: %d\n", kind, source);

    int32_t el2_esr = read_esr_el2();

    int32_t ec = ((el2_esr >> 26) & 0b111111);

    logger("        el2 esr: %llx\n", el2_esr);
    logger("        ec: %llx\n", ec);

    for (int32_t i = 0; i < 31; i++)
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
    // logger("irq stay in same el\n");
    handle_irq_exception_el2(stack_pointer);
}
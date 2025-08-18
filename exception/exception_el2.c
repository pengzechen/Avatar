
#include "avatar_types.h"
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

// Static function declarations
static void
handle_wfi_wfe_trap(union esr_el2 *esr, trap_frame_t *ctx);
static void
handle_hvc_call(union esr_el2 *esr, trap_frame_t *ctx);
static void
handle_smc_call(union esr_el2 *esr, trap_frame_t *ctx);
static void
handle_sysreg_access(union esr_el2 *esr, trap_frame_t *ctx);
static void
handle_illegal_execution_state(union esr_el2 *esr, trap_frame_t *ctx);
static void
handle_data_abort(union esr_el2 *esr, trap_frame_t *ctx);
static void
handle_unknown_exception(uint32_t ec, union esr_el2 *esr, trap_frame_t *ctx);
static const char *
get_exception_kind_name(uint64_t kind);
static const char *
get_exception_source_name(uint64_t source);

void
advance_pc(union esr_el2 *esr, trap_frame_t *context)
{
    context->elr += esr->len ? 4 : 2;
}

// Legacy function for backward compatibility
void
advance_pc_legacy(stage2_fault_info_t *info, trap_frame_t *context)
{
    advance_pc(&info->esr, context);
}

void
decode_spsr(uint64_t spsr)
{
    // M[3:0] - Exception level and SP selection (bits 3:0)
    uint64_t    m_field = spsr & 0xF;
    const char *el_str;

    switch (m_field) {
        case 0x0:
            el_str = "EL0t";
            break;  // EL0 using SP_EL0
        case 0x4:
            el_str = "EL1t";
            break;  // EL1 using SP_EL0
        case 0x5:
            el_str = "EL1h";
            break;  // EL1 using SP_EL1
        case 0x8:
            el_str = "EL2t";
            break;  // EL2 using SP_EL0
        case 0x9:
            el_str = "EL2h";
            break;  // EL2 using SP_EL2
        case 0xC:
            el_str = "EL3t";
            break;  // EL3 using SP_EL0
        case 0xD:
            el_str = "EL3h";
            break;  // EL3 using SP_EL3
        default:
            el_str = "Reserved";
            break;
    }

    logger("SPSR_EL2 decode (0x%llx):\n", spsr);
    logger("  M[3:0]: 0x%x -> %s\n", (uint32_t) m_field, el_str);
    logger("  M[4] (Execution state): %s\n", ((spsr >> 4) & 1) ? "AArch32" : "AArch64");
    logger("  F (FIQ masked): %s\n", ((spsr >> 6) & 1) ? "Yes" : "No");
    logger("  I (IRQ masked): %s\n", ((spsr >> 7) & 1) ? "Yes" : "No");
    logger("  A (SError masked): %s\n", ((spsr >> 8) & 1) ? "Yes" : "No");
    logger("  D (Debug masked): %s\n", ((spsr >> 9) & 1) ? "Yes" : "No");

    // 额外的状态位
    if ((spsr >> 10) & 0x3) {
        logger("  BTYPE: 0x%x\n", (uint32_t) ((spsr >> 10) & 0x3));
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
           (int) ((spsr >> 31) & 1),   // N
           (int) ((spsr >> 30) & 1),   // Z
           (int) ((spsr >> 29) & 1),   // C
           (int) ((spsr >> 28) & 1));  // V
}

/**
 * Handle synchronous exceptions trapped to EL2
 * @param stack_pointer: Pointer to saved context on stack
 */
void
handle_sync_exception_el2(uint64_t *stack_pointer)
{
    trap_frame_t *ctx_el2 = (trap_frame_t *) stack_pointer;
    uint32_t      el2_esr = read_esr_el2();
    uint32_t      ec      = (el2_esr >> 26) & 0x3F;  // Extract Exception Class

    union esr_el2 esr = {.bits = el2_esr};
    save_cpu_ctx(ctx_el2);
    switch (ec) {
        case ESR_EC_WFI_WFE:
            handle_wfi_wfe_trap(&esr, ctx_el2);
            break;

        case ESR_EC_HVC:
            handle_hvc_call(&esr, ctx_el2);
            break;

        case ESR_EC_SMC:
            handle_smc_call(&esr, ctx_el2);
            break;
        case ESR_EC_SYSREG:
            handle_sysreg_access(&esr, ctx_el2);
            break;

        case ESR_EC_ILLEGAL_STATE:
            handle_illegal_execution_state(&esr, ctx_el2);
            break;

        case ESR_EC_DATA_ABORT:
            handle_data_abort(&esr, ctx_el2);
            break;

        default:
            handle_unknown_exception(ec, &esr, ctx_el2);
            break;
    }
}


/**
 * Handle WFI/WFE instruction traps
 */
static void
handle_wfi_wfe_trap(union esr_el2 *esr, trap_frame_t *ctx)
{
    logger_info("WFI/WFE instruction trapped\n");

    // Check if it's WFI or WFE based on the TI bit
    if (esr->wfi_wfe.ti) {
        logger_debug("WFE (Wait For Event) instruction\n");
    } else {
        logger_debug("WFI (Wait For Interrupt) instruction\n");
    }

    advance_pc(esr, ctx);
}

/**
 * Handle HVC (Hypervisor Call) instructions
 */
static void
handle_hvc_call(union esr_el2 *esr, trap_frame_t *ctx)
{
    uint64_t function_id = ctx->r[0];
    logger_info("HVC call: function_id=0x%lx\n", function_id);

    // Handle PSCI calls
    switch (function_id) {
        case PSCI_0_2_FN64_CPU_ON:
            logger_info("PSCI CPU_ON call\n");
            ctx->r[0] = vpsci_cpu_on(ctx);
            break;

        case PSCI_0_2_FN_CPU_ON:
            logger_info("PSCI CPU_ON (32-bit) call\n");
            ctx->r[0] = vpsci_cpu_on(ctx);
            break;

        default:
            logger_warn("Unhandled HVC call: 0x%lx\n", function_id);
            ctx->r[0] = PSCI_RET_NOT_SUPPORTED;
            break;
    }

    advance_pc(esr, ctx);
}

/**
 * Handle SMC (Secure Monitor Call) instructions
 */
static void
handle_smc_call(union esr_el2 *esr, trap_frame_t *ctx)
{
    uint64_t function_id = ctx->r[0];
    logger_info("SMC call: function_id=0x%lx\n", function_id);

    // Handle PSCI calls - SMC uses 32-bit function IDs
    switch (function_id) {
        case PSCI_0_2_FN_PSCI_VERSION:
            logger_info("PSCI VERSION call\n");
            // Return PSCI version 0.2
            ctx->r[0] = PSCI_VERSION(0, 2);
            break;

        case PSCI_0_2_FN_CPU_ON:
            logger_info("PSCI CPU_ON call\n");
            ctx->r[0] = vpsci_cpu_on(ctx);
            break;

        case PSCI_0_2_FN64_CPU_ON:
            logger_info("PSCI CPU_ON call\n");
            ctx->r[0] = vpsci_cpu_on(ctx);
            break;

        case PSCI_0_2_FN_MIGRATE_INFO_TYPE:
            logger_info("PSCI MIGRATE_INFO_TYPE call\n");
            // 在虚拟化环境中，我们模拟多核 Trusted OS
            // 返回 PSCI_0_2_TOS_MP (2) 表示：
            // - Trusted OS 是多核的
            // - 不需要在 CPU 操作时进行迁移
            // 这是虚拟化环境的标准做法
            ctx->r[0] = PSCI_0_2_TOS_MP;
            break;

        default:
            logger_warn("Unhandled SMC call: 0x%lx\n", function_id);
            ctx->r[0] = PSCI_RET_NOT_SUPPORTED;
            break;
    }

    advance_pc(esr, ctx);
}

/**
 * Handle system register access traps
 */
static void
handle_sysreg_access(union esr_el2 *esr, trap_frame_t *ctx)
{
    logger_info("System register access trapped\n");

    // Extract system register information
    uint32_t op0      = esr->sysreg.op0;
    uint32_t op1      = esr->sysreg.op1;
    uint32_t crn      = esr->sysreg.crn;
    uint32_t crm      = esr->sysreg.crm;
    uint32_t op2      = esr->sysreg.op2;
    uint32_t rt       = esr->sysreg.rt;
    bool     is_write = esr->sysreg.direction;

    logger_debug("SysReg access: op0=%d, op1=%d, CRn=%d, CRm=%d, op2=%d, Rt=%d, %s\n",
                 op0,
                 op1,
                 crn,
                 crm,
                 op2,
                 rt,
                 is_write ? "write" : "read");

    // TODO: Implement virtual timer register handling
    // if (handle_vtimer_sysreg_access(op0, op1, crn, crm, op2, rt, is_write, ctx)) {
    //     advance_pc(esr, ctx);
    //     return;
    // }

    logger_warn("Unhandled system register access: ESR=0x%x\n", esr->bits);
    advance_pc(esr, ctx);
}

/**
 * Handle illegal execution state exceptions
 */
static void
handle_illegal_execution_state(union esr_el2 *esr, trap_frame_t *ctx)
{
    logger_error("Illegal execution state exception\n");
    logger_error("FAR_EL2=0x%lx, ESR_EL2=0x%x\n", read_far_el2(), esr->bits);

    decode_spsr(ctx->spsr);

    uint64_t sp_el1, sp_el0;
    asm volatile("mrs %0, sp_el1" : "=r"(sp_el1));
    asm volatile("mrs %0, sp_el0" : "=r"(sp_el0));
    logger_error("SP_EL1=0x%lx, SP_EL0=0x%lx\n", sp_el1, sp_el0);

    // For illegal execution state, we might not want to advance PC
    // as it could lead to further issues
    logger_error("System halted due to illegal execution state\n");
    while (1) {
        asm volatile("wfi");
    }
}

/**
 * Handle data abort exceptions from lower EL
 */
static void
handle_data_abort(union esr_el2 *esr, trap_frame_t *ctx)
{
    // Create stage2 fault info structure for memory access faults
    stage2_fault_info_t fault_info = {.esr = *esr, .reason = STAGE2_FAULT_DATA};

    // Read fault address information
    uint64_t hpfar = read_hpfar_el2();
    uint64_t far   = read_far_el2();

    // Combine FAR and HPFAR to get the full guest physical address
    fault_info.gpa = (far & 0xFFF) | (hpfar << 8);
    fault_info.gva = far;

    // Extract access information from ESR
    fault_info.is_write    = esr->dabt.write;
    fault_info.access_size = 1U << (esr->dabt.size & 0x3);

    // logger_debug("Data abort: GVA=0x%lx, GPA=0x%lx, %s, size=%d bytes\n",
    //              fault_info.gva, fault_info.gpa,
    //              fault_info.is_write ? "write" : "read",
    //              fault_info.access_size);

    // Handle the stage2 page fault
    data_abort_handler(&fault_info, ctx);

    // Advance PC using the legacy function for stage2 faults
    advance_pc_legacy(&fault_info, ctx);
}

/**
 * Handle unknown/unimplemented exception classes
 */
static void
handle_unknown_exception(uint32_t ec, union esr_el2 *esr, trap_frame_t *ctx)
{
    logger_error("Unknown exception class: EC=0x%x, ESR=0x%x\n", ec, esr->bits);
    logger_error("ELR_EL2=0x%lx, SPSR_EL2=0x%lx\n", ctx->elr, ctx->spsr);

    // Dump registers for debugging
    for (int i = 0; i < 31; i++) {
        logger_error("X%d=0x%lx\n", i, ctx->r[i]);
    }

    // Halt the system for debugging
    while (1) {
        asm volatile("wfi");
    }
}

/**
 * Handle IRQ exceptions trapped to EL2
 * @param stack_pointer: Pointer to saved context on stack
 */
void
handle_irq_exception_el2(uint64_t *stack_pointer)
{
    trap_frame_t *context = (trap_frame_t *) stack_pointer;

    // Read interrupt acknowledge register to get the interrupt ID
    uint32_t iar    = gic_read_iar();
    uint32_t irq_id = gic_iar_irqnr(iar);

    // logger_debug("IRQ exception: IRQ_ID=%d\n", irq_id);

    // Acknowledge the interrupt (End of Interrupt)
    gic_write_eoir(iar);

    /*
     * IMPORTANT: For virtual interrupts injected to guests, do NOT write DIR in EL2!
     * Writing DIR would mark the interrupt as inactive immediately, preventing the
     * guest from receiving it. Only write DIR for interrupts that should be handled
     * entirely in the hypervisor.
     */
    gic_write_dir(iar);

    // Save CPU context for interrupt handling
    save_cpu_ctx(context);

    // Dispatch to the registered interrupt handler
    irq_handler_t *handler_vec = get_g_handler_vec();
    if (handler_vec && handler_vec[irq_id]) {
        handler_vec[irq_id]((uint64_t *) context);
    } else {
        logger_warn("No handler registered for IRQ %d\n", irq_id);
    }
}

/**
 * Handle invalid/unimplemented exception vectors
 * @param stack_pointer: Pointer to saved context on stack
 * @param kind: Exception kind (sync/irq/fiq/serror)
 * @param source: Exception source (current_el_sp0/current_el_spx/lower_el_aarch64/lower_el_aarch32)
 */
void
invalid_exception_el2(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{
    trap_frame_t *context = (trap_frame_t *) stack_pointer;
    uint32_t      esr_el2 = read_esr_el2();
    uint32_t      ec      = (esr_el2 >> 26) & 0x3F;

    // Log the invalid exception details
    logger_error("=== INVALID EXCEPTION ===\n");
    logger_error("Kind: %s (%lu)\n", get_exception_kind_name(kind), kind);
    logger_error("Source: %s (%lu)\n", get_exception_source_name(source), source);
    logger_error("ESR_EL2: 0x%x\n", esr_el2);
    logger_error("Exception Class: 0x%x\n", ec);

    // Log processor state
    logger_error("ELR_EL2: 0x%lx\n", context->elr);
    logger_error("SPSR_EL2: 0x%lx\n", context->spsr);
    logger_error("SP_EL0: 0x%lx\n", context->usp);
    logger_error("FAR_EL2: 0x%lx\n", read_far_el2());

    // Decode SPSR for additional context
    decode_spsr(context->spsr);

    // Dump general purpose registers
    logger_error("=== REGISTER DUMP ===\n");
    for (int i = 0; i < 31; i++) {
        logger_error("X%d: 0x%016lx\n", i, context->r[i]);
    }

    logger_error("=== SYSTEM HALTED ===\n");
    logger_error("Invalid exception cannot be handled. System will halt.\n");

    // Halt the system - this is a fatal error
    while (1) {
        asm volatile("wfi");
    }
}

/**
 * Get human-readable exception kind name
 */
static const char *
get_exception_kind_name(uint64_t kind)
{
    switch (kind) {
        case 0:
            return "Synchronous";
        case 1:
            return "IRQ";
        case 2:
            return "FIQ";
        case 3:
            return "SError";
        default:
            return "Unknown";
    }
}

/**
 * Get human-readable exception source name
 */
static const char *
get_exception_source_name(uint64_t source)
{
    switch (source) {
        case 0:
            return "Current EL with SP_EL0";
        case 1:
            return "Current EL with SP_ELx";
        case 2:
            return "Lower EL (AArch64)";
        case 3:
            return "Lower EL (AArch32)";
        default:
            return "Unknown";
    }
}

/**
 * Handle IRQ exceptions that occur at the current EL (EL2) with SP_ELx
 * This is typically for hypervisor-internal interrupts
 * @param stack_pointer: Pointer to saved context on stack
 */
void
current_spxel_irq(uint64_t *stack_pointer)
{
    // logger_debug("IRQ at current EL (EL2) with SP_ELx\n");

    // For now, delegate to the same handler as lower EL IRQs
    // In the future, this might need different handling for hypervisor-specific interrupts
    handle_irq_exception_el2(stack_pointer);
}
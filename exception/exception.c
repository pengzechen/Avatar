
#include "avatar_types.h"
#include "io.h"
#include "exception.h"
#include "gic.h"
#include "timer.h"
#include "vmm/vcpu.h"
#include "thread.h"

// ESR_EL1 Exception Class definitions for EL1 exceptions
#define ESR_EL1_EC_SVC 0x15  // SVC instruction execution
#define ESR_EL1_EC_SMC 0x17  // SMC instruction execution

// Maximum number of interrupt vectors
#define MAX_IRQ_VECTORS 512

// External symbol declarations
extern void *syscall_table[];

// Static function declarations
static void
handle_svc_call(trap_frame_t *context, uint32_t esr);
static void
handle_smc_call_el1(trap_frame_t *context, uint32_t esr);
static void
handle_unknown_sync_exception(trap_frame_t *context, uint32_t esr, uint32_t ec);
static void
dump_exception_context(trap_frame_t *context, uint32_t esr, uint32_t ec);
static const char *
get_exception_kind_name(uint64_t kind);
static const char *
get_exception_source_name(uint64_t source);

/**
 * Handle synchronous exceptions from EL1
 * @param stack_pointer: Pointer to saved context on stack
 */
void
handle_sync_exception(uint64_t *stack_pointer)
{
    if (!stack_pointer) {
        logger_error("Invalid stack pointer in sync exception handler\n");
        return;
    }

    trap_frame_t *context = (trap_frame_t *) stack_pointer;
    uint32_t      esr_el1 = read_esr_el1();
    uint32_t      ec      = (esr_el1 >> 26) & 0x3F;  // Extract Exception Class

    // Save CPU context for the interrupted task
    save_cpu_ctx(context);

    switch (ec) {
        case ESR_EL1_EC_SVC:
            handle_svc_call(context, esr_el1);
            break;

        case ESR_EL1_EC_SMC:
            handle_smc_call_el1(context, esr_el1);
            break;

        default:
            handle_unknown_sync_exception(context, esr_el1, ec);
            break;
    }
}

// Global interrupt handler vector table
static irq_handler_t g_handler_vec[MAX_IRQ_VECTORS] = {0};

/**
 * Get the global interrupt handler vector table
 * @return: Pointer to the handler vector table
 */
irq_handler_t *
get_g_handler_vec(void)
{
    return g_handler_vec;
}

/**
 * Install an interrupt handler for a specific vector
 * @param vector: Interrupt vector number
 * @param handler: Handler function pointer
 */
void
irq_install(int32_t vector, void (*handler)(uint64_t *))
{
    if (vector < 0 || vector >= MAX_IRQ_VECTORS) {
        logger_error("Invalid IRQ vector: %d (max: %d)\n", vector, MAX_IRQ_VECTORS - 1);
        return;
    }

    if (!handler) {
        logger_error("Invalid handler for IRQ vector %d\n", vector);
        return;
    }

    g_handler_vec[vector] = handler;
    // logger_debug("Installed IRQ handler for vector %d\n", vector);
}

/**
 * Handle IRQ exceptions from EL1
 * @param stack_pointer: Pointer to saved context on stack
 */
void
handle_irq_exception(uint64_t *stack_pointer)
{
    if (!stack_pointer) {
        logger_error("Invalid stack pointer in IRQ exception handler\n");
        return;
    }

    trap_frame_t *context = (trap_frame_t *) stack_pointer;

    // Save CPU context for the interrupted task
    save_cpu_ctx(context);

    // Read interrupt acknowledge register to get the interrupt ID
    uint32_t iar    = gic_read_iar();
    uint32_t irq_id = gic_iar_irqnr(iar);

    // logger_debug("IRQ exception at EL1: IRQ_ID=%d\n", irq_id);

    // Acknowledge the interrupt (End of Interrupt)
    gic_write_eoir(iar);

    // Validate IRQ vector
    if (irq_id >= MAX_IRQ_VECTORS) {
        logger_error("Invalid IRQ vector: %d\n", irq_id);
        return;
    }

    // Dispatch to the registered interrupt handler
    if (g_handler_vec[irq_id]) {
        g_handler_vec[irq_id]((uint64_t *) context);
    } else {
        logger_warn("No handler registered for IRQ %d\n", irq_id);
    }
}

/**
 * Handle invalid/unimplemented exception vectors at EL1
 * @param stack_pointer: Pointer to saved context on stack
 * @param kind: Exception kind (sync/irq/fiq/serror)
 * @param source: Exception source (current_el_sp0/current_el_spx/lower_el_aarch64/lower_el_aarch32)
 */
void
invalid_exception(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{
    if (!stack_pointer) {
        logger_error("Invalid stack pointer in invalid exception handler\n");
        while (1) {
            asm volatile("wfi");
        }
    }

    trap_frame_t *context = (trap_frame_t *) stack_pointer;
    uint32_t      esr_el1 = read_esr_el1();
    uint32_t      ec      = (esr_el1 >> 26) & 0x3F;

    // Log the invalid exception details
    logger_error("=== INVALID EXCEPTION AT EL1 ===\n");
    logger_error("Kind: %s (%lu)\n", get_exception_kind_name(kind), kind);
    logger_error("Source: %s (%lu)\n", get_exception_source_name(source), source);
    logger_error("ESR_EL1: 0x%x\n", esr_el1);
    logger_error("Exception Class: 0x%x\n", ec);

    // Log processor state
    logger_error("ELR_EL1: 0x%lx\n", context->elr);
    logger_error("SPSR_EL1: 0x%lx\n", context->spsr);
    logger_error("SP_EL0: 0x%lx\n", context->usp);

    // Dump context for debugging
    dump_exception_context(context, esr_el1, ec);

    logger_error("=== SYSTEM HALTED ===\n");
    logger_error("Invalid exception cannot be handled. System will halt.\n");

    // Halt the system - this is a fatal error
    while (1) {
        asm volatile("wfi");
    }
}

/**
 * Handle SVC (Supervisor Call) instructions from EL0
 */
static void
handle_svc_call(trap_frame_t *context, uint32_t esr)
{
    // Extract SVC number from X8 register (ARM64 calling convention)
    uint64_t svc_number = context->r[8];

    // logger_debug("SVC call: number=%lu\n", svc_number);

    // Validate SVC number (assuming syscall_table has reasonable bounds)
    if (!syscall_table || !syscall_table[svc_number]) {
        logger_error("Invalid SVC number: %lu\n", svc_number);
        context->r[0] = -1;  // Return error
        return;
    }

    // Call the system call handler
    // The context pointer contains all arguments in r[0]-r[7]
    uint64_t (*syscall_func)(void *) = (uint64_t(*)(void *)) syscall_table[svc_number];
    uint64_t result                  = syscall_func((void *) context);

    // Store return value in X0
    context->r[0] = result;
}

/**
 * Handle SMC (Secure Monitor Call) instructions from EL1
 */
static void
handle_smc_call_el1(trap_frame_t *context, uint32_t esr)
{
    uint64_t function_id = context->r[0];

    logger_info("SMC call from EL1: function_id=0x%lx\n", function_id);

    // For now, just log and return success
    // In a real implementation, this might forward to EL3 or handle specific SMC calls
    context->r[0] = 0;  // Return success
}

/**
 * Handle unknown/unimplemented synchronous exceptions
 */
static void
handle_unknown_sync_exception(trap_frame_t *context, uint32_t esr, uint32_t ec)
{
    logger_error("Unknown synchronous exception at EL1\n");
    logger_error("ESR_EL1: 0x%x, Exception Class: 0x%x\n", esr, ec);

    // Only dump context on CPU 0 to avoid spam in multi-core systems
    // if (get_current_cpu_id() == 0) {
    dump_exception_context(context, esr, ec);
    // }

    logger_error("System halted due to unknown exception\n");
    while (1) {
        asm volatile("wfi");
    }
}

/**
 * Dump exception context for debugging
 */
static void
dump_exception_context(trap_frame_t *context, uint32_t esr, uint32_t ec)
{
    logger_error("=== EXCEPTION CONTEXT DUMP ===\n");
    logger_error("ESR: 0x%x, EC: 0x%x\n", esr, ec);
    logger_error("ELR: 0x%lx, SPSR: 0x%lx, SP_EL0: 0x%lx\n",
                 context->elr,
                 context->spsr,
                 context->usp);

    // Dump general purpose registers
    logger_error("=== REGISTER DUMP ===\n");
    for (int i = 0; i < 31; i++) {
        logger_error("X%d: 0x%016lx\n", i, context->r[i]);
    }
}

/**
 * Get human-readable exception kind name (shared with EL2 handler)
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
 * Get human-readable exception source name (shared with EL2 handler)
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
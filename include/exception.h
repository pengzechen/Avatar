#ifndef __ECCEPTION_H__
#define __ECCEPTION_H__

#define NUM_REGS 31

#include "avatar_types.h"

// ESR_EL2 Exception Class definitions
#define ESR_EC_WFI_WFE       0x01  // WFI or WFE instruction execution
#define ESR_EC_HVC           0x16  // HVC instruction execution
#define ESR_EC_SMC           0x17  // SMC instruction execution
#define ESR_EC_SYSREG        0x18  // System register access
#define ESR_EC_ILLEGAL_STATE 0x20  // Illegal Execution State
#define ESR_EC_DATA_ABORT    0x24  // Data Abort from lower EL

typedef struct
{
    uint64_t r[NUM_REGS];  // General-purpose registers x0..x30
    uint64_t usp;   // 如果是内核就是 el0 的栈，       如果是 hyper 就是 el1 的栈
    uint64_t elr;   // Exception Link Register       可以是 el1， 也可以是el2
    uint64_t spsr;  // Saved Process Status Register 可以是 el1， 也可以是el2
} trap_frame_t;

typedef trap_frame_t cpu_ctx_t;

union esr_el2 {
    uint32_t bits;

    struct
    {
        uint32_t iss : 25; /* Instruction Specific Syndrome */
        uint32_t len : 1;  /* Instruction length */
        uint32_t ec : 6;   /* Exception Class */
    };

    /* Common to all conditional exception classes (0x0N, except 0x00). */
    struct esr_cond
    {
        uint32_t iss : 20;    /* Instruction Specific Syndrome */
        uint32_t cc : 4;      /* Condition Code */
        uint32_t ccvalid : 1; /* CC Valid */
        uint32_t len : 1;     /* Instruction length */
        uint32_t ec : 6;      /* Exception Class */
    } cond;

    struct esr_wfi_wfe
    {
        uint32_t ti : 1; /* Trapped instruction */
        uint32_t sbzp : 19;
        uint32_t cc : 4;      /* Condition Code */
        uint32_t ccvalid : 1; /* CC Valid */
        uint32_t len : 1;     /* Instruction length */
        uint32_t ec : 6;      /* Exception Class */
    } wfi_wfe;

    /* reg, reg0, reg1 are 4 bits on AArch32, the fifth bit is sbzp. */
    struct esr_cp32
    {
        uint32_t read : 1;    /* Direction */
        uint32_t crm : 4;     /* CRm */
        uint32_t reg : 5;     /* Rt */
        uint32_t crn : 4;     /* CRn */
        uint32_t op1 : 3;     /* Op1 */
        uint32_t op2 : 3;     /* Op2 */
        uint32_t cc : 4;      /* Condition Code */
        uint32_t ccvalid : 1; /* CC Valid */
        uint32_t len : 1;     /* Instruction length */
        uint32_t ec : 6;      /* Exception Class */
    } cp32;                   /* ESR_EC_CP15_32, CP14_32, CP10 */

    struct esr_cp64
    {
        uint32_t read : 1; /* Direction */
        uint32_t crm : 4;  /* CRm */
        uint32_t reg1 : 5; /* Rt1 */
        uint32_t reg2 : 5; /* Rt2 */
        uint32_t sbzp : 1;
        uint32_t op1 : 4;     /* Op1 */
        uint32_t cc : 4;      /* Condition Code */
        uint32_t ccvalid : 1; /* CC Valid */
        uint32_t len : 1;     /* Instruction length */
        uint32_t ec : 6;      /* Exception Class */
    } cp64;                   /* ESR_EC_CP15_64, ESR_EC_CP14_64 */

    struct esr_cp
    {
        uint32_t coproc : 4; /* Number of coproc accessed */
        uint32_t sbz0p : 1;
        uint32_t tas : 1; /* Trapped Advanced SIMD */
        uint32_t res0 : 14;
        uint32_t cc : 4;      /* Condition Code */
        uint32_t ccvalid : 1; /* CC Valid */
        uint32_t len : 1;     /* Instruction length */
        uint32_t ec : 6;      /* Exception Class */
    } cp;                     /* ESR_EC_CP */

    struct esr_dabt
    {
        uint32_t dfsc : 6;  /* Data Fault Status Code */
        uint32_t write : 1; /* Write / not Read */
        uint32_t s1ptw : 1; /* Stage 1 Page Table Walk */
        uint32_t cache : 1; /* Cache Maintenance */
        uint32_t eat : 1;   /* External Abort Type */
        uint32_t sbzp0 : 6;
        uint32_t reg : 5;   /* Register */
        uint32_t sign : 1;  /* Sign extend */
        uint32_t size : 2;  /* Access Size */
        uint32_t valid : 1; /* Syndrome Valid */
        uint32_t len : 1;   /* Instruction length */
        uint32_t ec : 6;    /* Exception Class */
    } dabt;                 /* ESR_EC_DATA_ABORT_* */

    struct esr_sysreg
    {
        uint32_t op2 : 3;       /* Op2 */
        uint32_t op1 : 3;       /* Op1 */
        uint32_t crn : 4;       /* CRn */
        uint32_t rt : 5;        /* Rt */
        uint32_t crm : 4;       /* CRm */
        uint32_t direction : 1; /* Direction (0=MRS, 1=MSR) */
        uint32_t op0 : 2;       /* Op0 */
        uint32_t res0 : 3;      /* Reserved */
        uint32_t cc : 4;        /* Condition Code */
        uint32_t ccvalid : 1;   /* CC Valid */
        uint32_t len : 1;       /* Instruction length */
        uint32_t ec : 6;        /* Exception Class */
    } sysreg;                   /* ESR_EC_SYSREG */
};

// Exception information for general exception handling
typedef struct _exception_info_t
{
    union esr_el2 esr;
    uint32_t      exception_class;
    trap_frame_t *context;
} exception_info_t;

// Memory fault types for stage2 page faults
enum stage2_fault_reason_t
{
    STAGE2_FAULT_PREFETCH = 0,  // Instruction fetch abort
    STAGE2_FAULT_DATA           // Data access abort
};

// Specific structure for stage2 memory faults (data/instruction aborts)
typedef struct _stage2_fault_info_t
{
    union esr_el2              esr;
    enum stage2_fault_reason_t reason;
    vaddr_t                    gva;          // Guest Virtual Address
    paddr_t                    gpa;          // Guest Physical Address
    bool                       is_write;     // Write access (for data aborts)
    uint32_t                   access_size;  // Access size in bytes
} stage2_fault_info_t;

static inline uint32_t
read_esr_el1(void)
{
    uint32_t esr;

    // 使用内联汇编读取 ESR_EL1 寄存器
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));

    return esr;
}

static inline uint32_t
read_esr_el2(void)
{
    uint32_t esr;

    // 使用内联汇编读取 ESR_EL1 寄存器
    __asm__ volatile("mrs %0, esr_el2" : "=r"(esr));

    return esr;
}

static inline uint32_t
read_esr_el3(void)
{
    uint32_t esr;

    // 使用内联汇编读取 ESR_EL1 寄存器
    __asm__ volatile("mrs %0, esr_el3" : "=r"(esr));

    return esr;
}

static inline uint64_t
read_hpfar_el2(void)
{
    uint64_t value;
    __asm__ __volatile__("mrs %0, hpfar_el2 \n" : "=r"(value));
    return value;
}

static inline uint64_t
read_far_el2(void)
{
    uint64_t value;
    __asm__ __volatile__("mrs %0, far_el2 \n" : "=r"(value));
    return value;
}

static inline uint64_t
read_hyfar_el2(void)
{
    uint64_t value;
    __asm__ __volatile__("mrs %0, hpfar_el2 \n" : "=r"(value));
    return value;
}

typedef void (*irq_handler_t)(uint64_t *);

// Exception handling functions
void
advance_pc(union esr_el2 *esr, trap_frame_t *context);
void
advance_pc_legacy(stage2_fault_info_t *info, trap_frame_t *context);

void
irq_install(int32_t vector, void (*h)(uint64_t *));
irq_handler_t *
get_g_handler_vec();

#endif  // __ECCEPTION_FRAME_H__
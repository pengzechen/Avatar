
#include <aj_types.h>
#include <io.h>
#include <exception.h>
#include <gic.h>
#include <mem/ept.h>
#include <hyper/vcpu.h>

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

    // printf("        el2 esr: %x, ec: %x\n", el2_esr, ec);

    union hsr hsr = { .bits = el2_esr };
    save_cpu_ctx(ctx_el2);
    if (ec == 0x1) {
        // wfi
        ept_violation_info_t info;
        //printf("Prefetch abort : %x\n", hsr.bits);
        info.hsr.bits = hsr.bits;
        print_info("            This is wfi trap handler\n");
        advance_pc(&info, ctx_el2);
        return;
    }
    else if (ec == 0x16)
    { // hvc
        print_info("            This is hvc call handler\n");
        return;
    }
    else if (ec == 0x17)
    { // smc
        ept_violation_info_t info;
        //printf("Prefetch abort : %x\n", hsr.bits);
        info.hsr.bits = hsr.bits;
        print_info("            This is smc call handler\n");
        advance_pc(&info, ctx_el2);
        return;
    }
    else if (ec == 0x18)
    {
        print_info("            This is sys instr handler\n");
    }
    else if (ec == 0x24)
    { // data abort
        //print_info("            This is data abort handler\n");
        ept_violation_info_t info;
        //printf("Prefetch abort : %x\n", hsr.bits);
        info.hsr.bits = hsr.bits;
        info.reason = PREFETCH;
        uint64_t hpfar = read_hpfar_el2();  // 目前 hpfar 和 far 读到的内容不同，少了8位
        uint64_t far = read_far_el2();
        // printf("far: 0x%x, hpfar: 0x%x\n", far, hpfar);
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
        printf("General-purpose register: %d, value: %x\n", i, value);
    }

    uint64_t elr_el1_value = ctx_el2->elr;
    uint64_t usp_value = ctx_el2->usp;
    uint64_t spsr_value = ctx_el2->spsr;

    printf("usp: %x, elr: %x, spsr: %x\n", usp_value, elr_el1_value, spsr_value);

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
    gic_write_dir(iar);

    // printf("\nthis is handle irq el2\n");

    save_cpu_ctx(context);

    get_g_handler_vec()[vector]((uint64_t*)context); // arg not use
}

// 示例使用方式：处理无效异常
void invalid_exception_el2(uint64_t *stack_pointer, uint64_t kind, uint64_t source)
{

    trap_frame_t *context = (trap_frame_t *)stack_pointer;

    uint64_t x2_value = context->r[2];

    printf("invalid_exception_el2\n");

    int el2_esr = read_esr_el2();

    int ec = ((el2_esr >> 26) & 0b111111);

    printf("        el2 esr: %x\n", el2_esr);
    printf("        ec: %x\n", ec);

    for (int i = 0; i < 31; i++)
    {
        uint64_t value = context->r[i];
        printf("General-purpose register: %d, value: %x\n", i, value);
    }

    uint64_t elr_el1_value = context->elr;
    uint64_t usp_value = context->usp;
    uint64_t spsr_value = context->spsr;

    printf("usp: %x, elr: %x, spsr: %x\n", usp_value, elr_el1_value, spsr_value);


    while(1)
        ;
}

// 调用 handle_irq_exception_el2
void current_spxel_irq(uint64_t *stack_pointer)
{
    printf("irq stay in same el\n");
    handle_irq_exception_el2(stack_pointer);
}
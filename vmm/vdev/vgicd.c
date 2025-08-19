
#include "vmm/vgic.h"
#include "avatar_types.h"
#include "io.h"
#include "mmio.h"
#include "lib/bit_utils.h"


// 通用寄存器访问参数结构
typedef struct
{
    int32_t  reg_num;
    uint32_t value;
    int32_t  len;
} reg_access_params_t;

/**
 * @brief 处理虚拟GIC分发器(VGICD)的写操作
 *
 * 当Guest尝试写入GIC分发器寄存器时，会触发Stage-2页表异常，
 * 该函数负责模拟这些写操作，将Guest寄存器中的数据写入到虚拟GIC状态中
 *
 * @param info Stage-2异常信息，包含异常原因、地址等
 * @param el2_ctx EL2异常上下文，包含Guest的寄存器状态
 * @param paddr 目标物理地址（虚拟GIC寄存器地址）
 */
void
vgicd_write(stage2_fault_info_t *info, trap_frame_t *el2_ctx, void *paddr)
{
    uint32_t           reg_num;  // Guest寄存器编号 (X0-X30)
    volatile uint64_t *r;        // 指向Guest寄存器的指针
    volatile void     *buf;      // 寄存器数据缓冲区
    uint32_t           len;      // MMIO操作的数据长度
    volatile uint32_t *dst;      // 目标地址指针

    // 从HSR寄存器中提取寄存器编号和操作大小
    reg_num = info->esr.dabt.reg;                  // 获取源寄存器编号
    len     = 1U << (info->esr.dabt.size & 0x3U);  // 计算数据长度: 1,2,4,8字节

    // VGICD寄存器通常是32位的，检查操作大小
    if (len != 4U) {
        logger_warn("VGICD write size is not 4, but %u\n", len);
    }

    // 获取Guest寄存器的地址和数据
    r   = &el2_ctx->r[reg_num];  // 指向Guest寄存器
    buf = (void *) r;            // 寄存器数据缓冲区

    // 设置目标地址（虚拟GIC寄存器）
    dst = (uint32_t *) paddr;
    logger_vgic_debug("VGICD write: (%u bytes) 0x%x R%u\n", len, *dst, reg_num);

    logger_vgic_debug("old data: 0x%x\n", *dst);

    // 执行写操作：将Guest寄存器数据写入虚拟GIC寄存器
    // 跳过X30寄存器（链接寄存器），避免破坏返回地址
    if (reg_num != 30U) {
        *dst = *(uint32_t *) buf;  // 只写入32位数据
    }

    // 内存屏障：确保写操作对所有CPU核心可见
    dsb(sy);  // 数据同步屏障
    isb();    // 指令同步屏障

    logger_vgic_debug("new data: 0x%x\n", *dst);
}

/**
 * @brief 处理虚拟GIC分发器(VGICD)的读操作
 *
 * 当Guest尝试读取GIC分发器寄存器时，会触发Stage-2页表异常，
 * 该函数负责模拟这些读操作，从虚拟GIC状态中读取数据并写入Guest寄存器
 *
 * @param info Stage-2异常信息，包含异常原因、地址等
 * @param el2_ctx EL2异常上下文，包含Guest的寄存器状态
 * @param paddr 源物理地址（虚拟GIC寄存器地址）
 */
void
vgicd_read(stage2_fault_info_t *info, trap_frame_t *el2_ctx, void *paddr)
{
    uint32_t           reg_num;  // Guest目标寄存器编号 (X0-X30)
    volatile uint64_t *r;        // 指向Guest寄存器的指针
    volatile void     *buf;      // 寄存器数据缓冲区
    volatile uint32_t *src;      // 源地址指针（虚拟GIC寄存器）
    uint32_t           len;      // MMIO操作的数据长度
    uint32_t           dat;      // 从虚拟GIC寄存器读取的数据

    // 从HSR寄存器中提取寄存器编号和操作大小
    reg_num = info->esr.dabt.reg;                  // 获取目标寄存器编号
    len     = 1U << (info->esr.dabt.size & 0x3U);  // 计算数据长度: 1,2,4,8字节

    // VGICD寄存器通常是32位的，检查操作大小
    if (len != 4U) {
        logger_warn("VGICD read size is not 4, but %u\n", len);
    }

    // 获取Guest寄存器的地址
    r   = &el2_ctx->r[reg_num];  // 指向Guest目标寄存器
    buf = (void *) r;            // 寄存器数据缓冲区

    // 从虚拟GIC寄存器读取数据
    src = (uint32_t *) paddr;  // 源地址（虚拟GIC寄存器）
    dat = *src;                // 读取32位数据

    logger_vgic_debug("VGICD read: 0x%llx -> R%u (data: 0x%x)\n", (uint64_t) paddr, reg_num, dat);

    // 将读取的数据写入Guest寄存器
    // 跳过X30寄存器（链接寄存器），避免破坏返回地址
    if (reg_num != 30U) {
        *(uint32_t *) buf = dat;  // 写入寄存器低32位，高32位自动清零
    }

    // 内存屏障：确保读操作和寄存器更新的顺序
    dsb(sy);  // 数据同步屏障
    isb();    // 指令同步屏障
}

// 解析寄存器访问参数
static inline reg_access_params_t
parse_reg_access(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    reg_access_params_t params;
    params.reg_num = info->esr.dabt.reg;
    params.value   = el2_ctx->r[params.reg_num];
    params.len     = 1 << (info->esr.dabt.size & 0x3);
    return params;
}

// GICD_CTLR 寄存器写操作处理
static void
handle_gicd_ctlr_write(vgic_t *vgic, stage2_fault_info_t *info, trap_frame_t *el2_ctx, paddr_t gpa)
{
    reg_access_params_t params = parse_reg_access(info, el2_ctx);
    vgic->gicd_ctlr            = params.value;
    logger_vgic_debug("GICD_CTLR write: gpa=0x%llx, value=0x%llx, len=%d\n",
                      gpa,
                      params.value,
                      params.len);
}

// GICD_CTLR 寄存器读操作处理
static void
handle_gicd_ctlr_read(vgic_t *vgic, stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    vgicd_read(info, el2_ctx, &vgic->gicd_ctlr);
    logger_vgic_debug("GICD_CTLR read: ctlr=0x%x\n", vgic->gicd_ctlr);
}

// GICD_ISENABLER(0) SGI+PPI 使能寄存器写操作处理
static void
handle_gicd_isenabler0_write(tcb_t               *curr,
                             stage2_fault_info_t *info,
                             trap_frame_t        *el2_ctx,
                             paddr_t              gpa)
{
    reg_access_params_t params = parse_reg_access(info, el2_ctx);
    vgic_core_state_t  *vgicc  = get_vgicc_by_vcpu(curr);

    // W1S: 写1置位，写0无效果
    vgicc->sgi_ppi_isenabler |= params.value;

    // SGI 不需要硬件使能，PPI 在启动时已经打开
    // 这里只是记录虚拟机的使能状态

    logger_vgic_debug(
        "GICD_ISENABLER(0) write: gpa=0x%llx, value=0x%llx, len=%d, enabled_mask=0x%x\n",
        gpa,
        params.value,
        params.len,
        vgicc->sgi_ppi_isenabler);
}

// GICD_ISENABLER(1+) SPI 使能寄存器写操作处理
static void
handle_gicd_isenabler_spi_write(vgic_t              *vgic,
                                stage2_fault_info_t *info,
                                trap_frame_t        *el2_ctx,
                                paddr_t              gpa)
{
    reg_access_params_t params  = parse_reg_access(info, el2_ctx);
    int32_t             reg_idx = (gpa - GICD_ISENABLER(0)) / 0x4;
    int32_t             base_id = reg_idx * 32;

    // W1S: 写1置位
    vgic->gicd_scenabler[reg_idx - 1] |= params.value;

    // 对每个置位的中断调用硬件使能
    for (int bit = 0; bit < 32; bit++) {
        if (params.value & (1U << bit)) {
            gic_enable_int(base_id + bit, 1);
        }
    }

    logger_vgic_debug(
        "GICD_ISENABLER(%d) write: gpa=0x%llx, value=0x%llx, len=%d, enabled_mask=0x%x\n",
        reg_idx,
        gpa,
        params.value,
        params.len,
        vgic->gicd_scenabler[reg_idx]);
}

// GICD_ISENABLER(0) SGI+PPI 使能寄存器读操作处理
static void
handle_gicd_isenabler0_read(tcb_t *curr, stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    vgic_core_state_t *vgicc        = get_vgicc_by_vcpu(curr);
    uint32_t           enabled_mask = vgicc->sgi_ppi_isenabler;
    vgicd_read(info, el2_ctx, &enabled_mask);

    logger_vgic_debug("GICD_ISENABLER(0) read: enabled_mask=0x%x\n", enabled_mask);
}

// GICD_ISENABLER(1+) SPI 使能寄存器读操作处理
static void
handle_gicd_isenabler_spi_read(vgic_t              *vgic,
                               stage2_fault_info_t *info,
                               trap_frame_t        *el2_ctx,
                               paddr_t              gpa)
{
    int32_t  reg_idx      = (gpa - GICD_ISENABLER(0)) / 0x4;
    uint32_t enabled_mask = vgic->gicd_scenabler[reg_idx];
    vgicd_read(info, el2_ctx, &enabled_mask);

    logger_vgic_debug("GICD_ISENABLER(%d) read: enabled_mask=0x%x\n", reg_idx, enabled_mask);
}

// GICD_ICENABLER(0) SGI+PPI 禁用寄存器写操作处理
static void
handle_gicd_icenabler0_write(tcb_t               *curr,
                             stage2_fault_info_t *info,
                             trap_frame_t        *el2_ctx,
                             paddr_t              gpa)
{
    reg_access_params_t params = parse_reg_access(info, el2_ctx);
    vgic_core_state_t  *vgicc  = get_vgicc_by_vcpu(curr);

    // W1C: 写1清零，写0无效果
    vgicc->sgi_ppi_isenabler &= ~params.value;

    // SGI 不需要硬件禁用，PPI 保持硬件使能状态
    // 这里只是记录虚拟机的禁用状态

    logger_vgic_debug(
        "GICD_ICENABLER(0) write: gpa=0x%llx, value=0x%llx, len=%d, enabled_mask=0x%x\n",
        gpa,
        params.value,
        params.len,
        vgicc->sgi_ppi_isenabler);
}

// GICD_ICENABLER(1+) SPI 禁用寄存器写操作处理
static void
handle_gicd_icenabler_spi_write(vgic_t              *vgic,
                                stage2_fault_info_t *info,
                                trap_frame_t        *el2_ctx,
                                paddr_t              gpa)
{
    reg_access_params_t params  = parse_reg_access(info, el2_ctx);
    int32_t             reg_idx = (gpa - GICD_ICENABLER(0)) / 0x4;
    int32_t             base_id = reg_idx * 32;

    // W1C: 写1清零
    vgic->gicd_scenabler[reg_idx - 1] &= ~params.value;

    // 对每个要清零的中断调用硬件禁用
    for (int bit = 0; bit < 32; bit++) {
        if (params.value & (1U << bit)) {
            if (base_id + bit != 33)
                gic_enable_int(base_id + bit, 0);
        }
    }

    logger_vgic_debug(
        "GICD_ICENABLER(%d) write: gpa=0x%llx, value=0x%llx, len=%d, enabled_mask=0x%x\n",
        reg_idx,
        gpa,
        params.value,
        params.len,
        vgic->gicd_scenabler[reg_idx]);
}

// GICD_ICENABLER(0) SGI+PPI 禁用寄存器读操作处理
static void
handle_gicd_icenabler0_read(tcb_t *curr, stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    vgic_core_state_t *vgicc        = get_vgicc_by_vcpu(curr);
    uint32_t           enabled_mask = vgicc->sgi_ppi_isenabler;
    vgicd_read(info, el2_ctx, &enabled_mask);

    logger_vgic_debug("GICD_ICENABLER(0) read: enabled_mask=0x%x\n", enabled_mask);
}

// GICD_ICENABLER(1+) SPI 禁用寄存器读操作处理
static void
handle_gicd_icenabler_spi_read(vgic_t              *vgic,
                               stage2_fault_info_t *info,
                               trap_frame_t        *el2_ctx,
                               paddr_t              gpa)
{
    int32_t  reg_idx      = (gpa - GICD_ICENABLER(0)) / 0x4;
    uint32_t enabled_mask = vgic->gicd_scenabler[reg_idx];
    vgicd_read(info, el2_ctx, &enabled_mask);

    logger_vgic_debug("GICD_ICENABLER(%d) read: enabled_mask=0x%x\n", reg_idx, enabled_mask);
}

// GICD_ISPENDER(0) SGI+PPI 挂起寄存器写操作处理
static void
handle_gicd_ispender0_write(tcb_t               *curr,
                            stage2_fault_info_t *info,
                            trap_frame_t        *el2_ctx,
                            paddr_t              gpa)
{
    reg_access_params_t params = parse_reg_access(info, el2_ctx);
    vgic_core_state_t  *vgicc  = get_vgicc_by_vcpu(curr);

    // W1S: 写1置位，写0无效果
    // 对每个要设置的位进行处理
    for (int32_t bit = 0; bit < 32; bit++) {
        if (params.value & (1U << bit)) {
            vgic_set_irq_pending(vgicc, bit);
            logger_vgic_debug("Set SGI/PPI %d pending\n", bit);
        }
    }

    logger_vgic_debug("GICD_ISPENDER(0) write: gpa=0x%llx, value=0x%llx, len=%d\n",
                      gpa,
                      params.value,
                      params.len);
}

// GICD_ICPENDER(0) SGI+PPI 挂起清除寄存器写操作处理
static void
handle_gicd_icpender0_write(tcb_t               *curr,
                            stage2_fault_info_t *info,
                            trap_frame_t        *el2_ctx,
                            paddr_t              gpa)
{
    reg_access_params_t params = parse_reg_access(info, el2_ctx);
    vgic_core_state_t  *vgicc  = get_vgicc_by_vcpu(curr);

    // W1C: 写1清零，写0无效果
    // 对每个要清除的位进行处理
    for (int32_t bit = 0; bit < 32; bit++) {
        if (params.value & (1U << bit)) {
            vgic_clear_irq_pending(vgicc, bit);

            // 同时需要清除 GICH_LR 中对应的 pending 状态
            for (int32_t lr = 0; lr < GICH_LR_NUM; lr++) {
                uint32_t lr_val = vgicc->saved_lr[lr];
                uint32_t vid    = lr_val & 0x3ff;         // Virtual ID
                if (vid == bit && (lr_val & (1U << 28)))  // 检查是否是 pending 状态
                {
                    // 清除 LR 中的 pending 位 (bit 28)
                    vgicc->saved_lr[lr] &= ~(1U << 28);
                    // 标记该 LR 为空闲
                    vgicc->saved_elsr0 |= (1U << lr);
                    logger_vgic_debug("Cleared LR%d for SGI/PPI %d\n", lr, bit);
                    break;
                }
            }

            logger_vgic_debug("Clear SGI/PPI %d pending\n", bit);
        }
    }

    logger_vgic_debug("GICD_ICPENDER(0) write: gpa=0x%llx, value=0x%llx, len=%d\n",
                      gpa,
                      params.value,
                      params.len);
}

// GICD_ISPENDER(0) SGI+PPI 挂起寄存器读操作处理
static void
handle_gicd_ispender0_read(tcb_t *curr, stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    vgic_core_state_t *vgicc          = get_vgicc_by_vcpu(curr);
    uint32_t           pending_status = vgic_get_sgi_ppi_pending_status(vgicc);
    vgicd_read(info, el2_ctx, &pending_status);

    logger_vgic_debug("GICD_ISPENDER(0) read: pending_status=0x%x\n", pending_status);
}

// GICD_ICPENDER(0) SGI+PPI 挂起清除寄存器读操作处理
static void
handle_gicd_icpender0_read(tcb_t *curr, stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    vgic_core_state_t *vgicc          = get_vgicc_by_vcpu(curr);
    uint32_t           pending_status = vgic_get_sgi_ppi_pending_status(vgicc);
    vgicd_read(info, el2_ctx, &pending_status);

    logger_vgic_debug("GICD_ICPENDER(0) read: pending_status=0x%x\n", pending_status);
}

// GICD_IPRIORITYR SGI+PPI 优先级寄存器写操作处理
static void
handle_gicd_ipriorityr_sgi_ppi_write(tcb_t               *curr,
                                     stage2_fault_info_t *info,
                                     trap_frame_t        *el2_ctx,
                                     paddr_t              gpa)
{
    reg_access_params_t params = parse_reg_access(info, el2_ctx);
    vgic_core_state_t  *vgicc  = get_vgicc_by_vcpu(curr);
    int32_t             offset = gpa - GICD_IPRIORITYR(0);

    // 直接写入 per-vCPU 的优先级数组
    for (int32_t i = 0; i < params.len; ++i) {
        uint32_t int_id  = offset + i;
        uint8_t  pri_raw = (params.value >> (8 * i)) & 0xFF;

        if (int_id < GIC_FIRST_SPI) {
            vgicc->sgi_ppi_ipriorityr[int_id] = pri_raw;

            // 同时设置硬件优先级（用于实际的中断处理）
            uint32_t pri = pri_raw >> 3;  // 还原 priority 值（只保留高 5 位）
            gic_set_ipriority(int_id, pri);
        }
    }

    logger_vgic_debug("GICD_IPRIORITYR(sgi-ppi) write: offset=%d, len=%d, val=0x%llx\n",
                      offset,
                      params.len,
                      params.value);
}

// GICD_IPRIORITYR SPI 优先级寄存器写操作处理
static void
handle_gicd_ipriorityr_spi_write(vgic_t              *vgic,
                                 stage2_fault_info_t *info,
                                 trap_frame_t        *el2_ctx,
                                 paddr_t              gpa)
{
    reg_access_params_t params = parse_reg_access(info, el2_ctx);
    int32_t             offset = gpa - GICD_IPRIORITYR(0);

    // 直接写入 VM 级别的优先级数组
    for (int32_t i = 0; i < params.len; ++i) {
        uint32_t int_id  = offset + i;
        uint8_t  pri_raw = (params.value >> (8 * i)) & 0xFF;

        if (int_id < SPI_ID_MAX) {
            vgic->gicd_ipriorityr[int_id] = pri_raw;

            // 同时设置硬件优先级（用于实际的中断处理）
            uint32_t pri = pri_raw >> 3;
            gic_set_ipriority(int_id, pri);
        }
    }

    logger_vgic_debug("GICD_IPRIORITYR(spi) write: offset=%d, len=%d, val=0x%llx\n",
                      offset,
                      params.len,
                      params.value);
}

// GICD_IPRIORITYR SGI+PPI 优先级寄存器读操作处理
static void
handle_gicd_ipriorityr_sgi_ppi_read(tcb_t               *curr,
                                    stage2_fault_info_t *info,
                                    trap_frame_t        *el2_ctx,
                                    paddr_t              gpa)
{
    vgic_core_state_t *vgicc         = get_vgicc_by_vcpu(curr);
    uint32_t           word_offset   = (gpa - GICD_IPRIORITYR(0)) / 4;
    uint32_t           priority_word = ((uint32_t *) vgicc->sgi_ppi_ipriorityr)[word_offset];
    vgicd_read(info, el2_ctx, &priority_word);

    logger_vgic_debug("GICD_IPRIORITYR(sgi-ppi) read: word_offset=%d, priority_word=0x%x\n",
                      word_offset,
                      priority_word);
}

// GICD_IPRIORITYR SPI 优先级寄存器读操作处理
static void
handle_gicd_ipriorityr_spi_read(vgic_t              *vgic,
                                stage2_fault_info_t *info,
                                trap_frame_t        *el2_ctx,
                                paddr_t              gpa)
{
    uint32_t word_offset   = (gpa - GICD_IPRIORITYR(0)) / 4;
    uint32_t priority_word = ((uint32_t *) vgic->gicd_ipriorityr)[word_offset];
    vgicd_read(info, el2_ctx, &priority_word);

    logger_vgic_debug("GICD_IPRIORITYR(spi) read: word_offset=%d, priority_word=0x%x\n",
                      word_offset,
                      priority_word);
}

// GICD_SGIR 软件生成中断寄存器写操作处理
static void
handle_gicd_sgir_write(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    reg_access_params_t params   = parse_reg_access(info, el2_ctx);
    uint32_t            sgir_val = params.value;

    uint8_t sgi_int_id         = sgir_val & 0xF;
    uint8_t target_list_filter = (sgir_val >> 24) & 0x3;
    uint8_t cpu_target_list    = (sgir_val >> 16) & 0xFF;

    tcb_t        *curr = curr_task_el2();
    struct _vm_t *vm   = curr->curr_vm;

    uint32_t curr_id = get_vcpuid(curr);

    list_node_t *iter = list_first(&vm->vcpus);
    while (iter) {
        tcb_t   *task   = list_node_parent(iter, tcb_t, vm_node);
        uint32_t vcpuid = get_vcpuid(task);
        switch (target_list_filter) {
            case 0:  // 指定目标 CPU
                if ((cpu_target_list >> vcpuid) & 1) {
                    vgic_inject_sgi(task, sgi_int_id);
                }
                break;
            case 1:  // 其他 CPU
                if (vcpuid != curr_id) {
                    vgic_inject_sgi(task, sgi_int_id);
                }
                break;
            case 2:  // 当前 CPU
                if (vcpuid == curr_id) {
                    vgic_inject_sgi(task, sgi_int_id);
                }
                break;
            default:
                logger_error("SGIR: invalid target_list_filter = %d\n", target_list_filter);
                break;
        }
        iter = list_node_next(iter);
    }
    logger_vgic_debug("GICD_SGIR write completed\n");
}

// 处理未实现的寄存器访问
static void
handle_unimplemented_register(const char *reg_name, bool is_write)
{
    logger_warn("%s %s not implemented\n", reg_name, is_write ? "write" : "read");
}

// 处理 GICD_TYPER 寄存器读操作
static void
handle_gicd_typer_read(vgic_t *vgic, stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    uint32_t typer = gic_get_typer();
    // 清除原有的 CPU 数量字段 (bits [7:5])
    typer &= ~(0x7 << 5);
    // 设置虚拟机的 vCPU 数量 (CPU number = value + 1)
    uint32_t vcpu_count = vgic->vm->vcpu_cnt;
    if (vcpu_count > 0) {
        typer |= ((vcpu_count - 1) & 0x7) << 5;
    }
    vgicd_read(info, el2_ctx, &typer);
    logger_vgic_debug("GICD_TYPER read: typer=0x%x, vcpu_cnt=%d\n", typer, vcpu_count);
}

// 处理 GICD_IIDR 寄存器读操作
static void
handle_gicd_iidr_read(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    uint32_t iidr = gic_get_iidr();
    vgicd_read(info, el2_ctx, &iidr);
    logger_vgic_debug("GICD_IIDR read: iidr=0x%x\n", iidr);
}

// 处理 GICD_ICFGR SPI 配置寄存器写操作
static void
handle_gicd_icfgr_spi_write(vgic_t              *vgic,
                            stage2_fault_info_t *info,
                            trap_frame_t        *el2_ctx,
                            paddr_t              gpa)
{
    reg_access_params_t params     = parse_reg_access(info, el2_ctx);
    uint32_t            reg_offset = (gpa - GICD_ICFGR(0)) / 4;

    vgic->gicd_icfgr[reg_offset] = params.value;
    uint32_t masked_value        = params.value & 0xAAAAAAAA;
    write32(masked_value, (void *) gpa);

    logger_vgic_debug("GICD_ICFGR(spi) write: reg_offset=%d, len=%d, reg_value=0x%x, masked=0x%x\n",
                      reg_offset,
                      params.len,
                      params.value,
                      masked_value);
}

// 处理 GICD_ITARGETSR SPI 目标寄存器写操作
static void
handle_gicd_itargetsr_spi_write(vgic_t              *vgic,
                                stage2_fault_info_t *info,
                                trap_frame_t        *el2_ctx,
                                paddr_t              gpa)
{
    reg_access_params_t params                       = parse_reg_access(info, el2_ctx);
    uint32_t            word_offset                  = (gpa - GICD_ITARGETSR(0)) / 4;
    ((uint32_t *) vgic->gicd_itargetsr)[word_offset] = params.value;

    logger_vgic_debug("GICD_ITARGETSR(spi) write: word_offset=%d, len=%d, reg_value=0x%x\n",
                      word_offset,
                      params.len,
                      params.value);
}

// 处理 GICD_ICFGR SGI+PPI 配置寄存器读操作
static void
handle_gicd_icfgr_sgi_ppi_read(stage2_fault_info_t *info, trap_frame_t *el2_ctx, paddr_t gpa)
{
    uint32_t cfg_word;
    uint32_t reg_offset = (gpa - GICD_ICFGR(0)) / 4;

    if (reg_offset == 0) {
        cfg_word = 0x0000AAAA;
    } else {
        cfg_word = 0x00000000;
    }

    vgicd_read(info, el2_ctx, &cfg_word);
    logger_vgic_debug("GICD_ICFGR(sgi-ppi) read: reg_offset=%d, cfg_word=0x%x\n",
                      reg_offset,
                      cfg_word);
}

// 处理 GICD_ICFGR SPI 配置寄存器读操作
static void
handle_gicd_icfgr_spi_read(vgic_t              *vgic,
                           stage2_fault_info_t *info,
                           trap_frame_t        *el2_ctx,
                           paddr_t              gpa)
{
    uint32_t reg_offset = (gpa - GICD_ICFGR(0)) / 4;
    uint32_t cfg_word   = vgic->gicd_icfgr[reg_offset];
    vgicd_read(info, el2_ctx, &cfg_word);

    logger_vgic_debug("GICD_ICFGR(spi) read: reg_offset=%d, cfg_word=0x%x\n", reg_offset, cfg_word);
}

// 处理 GICD_ITARGETSR SGI+PPI 目标寄存器读操作
static void
handle_gicd_itargetsr_sgi_ppi_read(tcb_t *curr, stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    uint32_t vcpu_id     = get_vcpuid(curr);
    uint8_t  value       = 1 << vcpu_id;
    uint32_t target_word = (value << 24) | (value << 16) | (value << 8) | value;
    vgicd_read(info, el2_ctx, &target_word);

    logger_vgic_debug("GICD_ITARGETSR(sgi-ppi) read: vcpu_id=%d, target_word=0x%x\n",
                      vcpu_id,
                      target_word);
}

// 处理 GICD_ITARGETSR SPI 目标寄存器读操作
static void
handle_gicd_itargetsr_spi_read(vgic_t              *vgic,
                               stage2_fault_info_t *info,
                               trap_frame_t        *el2_ctx,
                               paddr_t              gpa)
{
    uint32_t word_offset = (gpa - GICD_ITARGETSR(0)) / 4;
    uint32_t target_word = ((uint32_t *) vgic->gicd_itargetsr)[word_offset];
    vgicd_read(info, el2_ctx, &target_word);

    logger_vgic_debug("GICD_ITARGETSR(spi) read: word_offset=%d, target_word=0x%x\n",
                      word_offset,
                      target_word);
}

// 处理 GICD 写操作
static void
handle_gicd_write_operations(tcb_t               *curr,
                             struct _vm_t        *vm,
                             vgic_t              *vgic,
                             stage2_fault_info_t *info,
                             trap_frame_t        *el2_ctx,
                             paddr_t              gpa)
{
    if (gpa == GICD_CTLR) {
        handle_gicd_ctlr_write(vgic, info, el2_ctx, gpa);
    }
    /* is enable reg - W1S */
    // SGI and PPI enable (GICD_ISENABLER(0))
    else if (gpa == GICD_ISENABLER(0)) {
        handle_gicd_isenabler0_write(curr, info, el2_ctx, gpa);
    } else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0)) {
        handle_gicd_isenabler_spi_write(vgic, info, el2_ctx, gpa);
    }
    // SGI and PPI disable (GICD_ICENABLER(0)) - W1C
    else if (gpa == GICD_ICENABLER(0)) {
        handle_gicd_icenabler0_write(curr, info, el2_ctx, gpa);
    } else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0)) {
        handle_gicd_icenabler_spi_write(vgic, info, el2_ctx, gpa);
    }
    /* is pend reg*/
    // SGI+PPI set pending (GICD_ISPENDER(0)) - W1S
    else if (gpa == GICD_ISPENDER(0)) {
        handle_gicd_ispender0_write(curr, info, el2_ctx, gpa);
    }
    /* ic pend reg*/
    // SGI+PPI clear pending (GICD_ICPENDER(0)) - W1C
    else if (gpa == GICD_ICPENDER(0)) {
        handle_gicd_icpender0_write(curr, info, el2_ctx, gpa);
    }
    // SPI set pending (GICD_ISPENDER(1) and above)
    else if (GICD_ISPENDER(1) <= gpa && gpa < GICD_ICPENDER(0)) {
        handle_unimplemented_register("GICD_ISPENDER(spi)", true);
    }
    // SPI clear pending (GICD_ICPENDER(1) and above)
    else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_ISACTIVER(0)) {
        handle_unimplemented_register("GICD_ICPENDER(spi)", true);
    }
    /* I priority reg*/
    // SGI + PPI priority write (per-vCPU)
    else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4)) {
        handle_gicd_ipriorityr_sgi_ppi_write(curr, info, el2_ctx, gpa);
    }
    // SPI priority write (VM-wide)
    else if (GICD_IPRIORITYR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_IPRIORITYR(SPI_ID_MAX / 4)) {
        handle_gicd_ipriorityr_spi_write(vgic, info, el2_ctx, gpa);
    }
    /* I cfg reg*/
    // SPI configuration register write
    else if (GICD_ICFGR(GIC_FIRST_SPI / 16) <= gpa && gpa < GICD_ICFGR(SPI_ID_MAX / 16)) {
        handle_gicd_icfgr_spi_write(vgic, info, el2_ctx, gpa);
    }
    /* I target reg*/
    // SPI target register write
    else if (GICD_ITARGETSR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_ITARGETSR(SPI_ID_MAX / 4)) {
        handle_gicd_itargetsr_spi_write(vgic, info, el2_ctx, gpa);
    }
    /* sgi reg*/
    else if (gpa == GICD_SGIR)  // wo
    {
        handle_gicd_sgir_write(info, el2_ctx);
    }
    /* pend sgi reg*/
    else if (GICD_SPENDSGIR(0) <= gpa && gpa < GICD_SPENDSGIR(MAX_SGI_ID / 4)) {
        handle_unimplemented_register("GICD_SPENDSGIR", true);
    }
    /* clear pend sgi reg*/
    else if (GICD_CPENDSGIR(0) <= gpa && gpa < GICD_CPENDSGIR(MAX_SGI_ID / 4)) {
        handle_unimplemented_register("GICD_CPENDSGIR", true);
    }
}

// 处理 GICD 读操作
static void
handle_gicd_read_operations(tcb_t               *curr,
                            struct _vm_t        *vm,
                            vgic_t              *vgic,
                            stage2_fault_info_t *info,
                            trap_frame_t        *el2_ctx,
                            paddr_t              gpa)
{
    if (gpa == GICD_CTLR) {
        handle_gicd_ctlr_read(vgic, info, el2_ctx);
    } else if (gpa == GICD_TYPER)  // ro
    {
        handle_gicd_typer_read(vgic, info, el2_ctx);
    } else if (gpa == GICD_IIDR)  // ro
    {
        handle_gicd_iidr_read(info, el2_ctx);
    }
    /*  is enable reg*/
    // SGI and PPI enable read (GICD_ISENABLER(0))
    else if (gpa == GICD_ISENABLER(0)) {
        handle_gicd_isenabler0_read(curr, info, el2_ctx);
    }
    // SPI enable read (GICD_ISENABLER(1) and above)
    else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0)) {
        handle_gicd_isenabler_spi_read(vgic, info, el2_ctx, gpa);
    }
    // SGI and PPI disable read (GICD_ICENABLER(0))
    else if (gpa == GICD_ICENABLER(0)) {
        handle_gicd_icenabler0_read(curr, info, el2_ctx);
    }
    // SPI disable read (GICD_ICENABLER(1) and above)
    else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0)) {
        handle_gicd_icenabler_spi_read(vgic, info, el2_ctx, gpa);
    }
    /* is pend reg*/
    // SGI+PPI pending read (GICD_ISPENDER(0))
    else if (gpa == GICD_ISPENDER(0)) {
        handle_gicd_ispender0_read(curr, info, el2_ctx);
    }
    /* ic pend reg*/
    // SGI+PPI pending read (GICD_ICPENDER(0)) - 返回相同的值
    else if (gpa == GICD_ICPENDER(0)) {
        handle_gicd_icpender0_read(curr, info, el2_ctx);
    }
    // SPI pending read (GICD_ISPENDER(1) and above)
    else if (GICD_ISPENDER(1) <= gpa && gpa < GICD_ICPENDER(0)) {
        handle_unimplemented_register("GICD_ISPENDER(spi)", false);
    }
    // SPI pending read (GICD_ICPENDER(1) and above)
    else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_IPRIORITYR(0)) {
        handle_unimplemented_register("GICD_ICPENDER(spi)", false);
    }
    /* I priority reg*/
    // SGI+PPI priority read (per-vCPU)
    else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4)) {
        handle_gicd_ipriorityr_sgi_ppi_read(curr, info, el2_ctx, gpa);
    }
    // SPI priority read (VM-wide)
    else if (GICD_IPRIORITYR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_IPRIORITYR(SPI_ID_MAX / 4)) {
        handle_gicd_ipriorityr_spi_read(vgic, info, el2_ctx, gpa);
    }
    /* I cfg reg*/
    // SGI+PPI configuration register read (固定配置)
    else if (GICD_ICFGR(0) <= gpa && gpa < GICD_ICFGR(GIC_FIRST_SPI / 16)) {
        handle_gicd_icfgr_sgi_ppi_read(info, el2_ctx, gpa);
    }
    // SPI configuration register read
    else if (GICD_ICFGR(GIC_FIRST_SPI / 16) <= gpa && gpa < GICD_ICFGR(SPI_ID_MAX / 16)) {
        handle_gicd_icfgr_spi_read(vgic, info, el2_ctx, gpa);
    }
    /* I target reg*/
    // SGI+PPI target register read (returns current vCPU mask)
    else if (GICD_ITARGETSR(0) <= gpa && gpa < GICD_ITARGETSR(GIC_FIRST_SPI / 4)) {
        handle_gicd_itargetsr_sgi_ppi_read(curr, info, el2_ctx);
    }
    // SPI target register read
    else if (GICD_ITARGETSR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_ITARGETSR(SPI_ID_MAX / 4)) {
        handle_gicd_itargetsr_spi_read(vgic, info, el2_ctx, gpa);
    }
}

// handle gicd emu
void
intc_handler(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    tcb_t        *curr = curr_task_el2();
    struct _vm_t *vm   = curr->curr_vm;
    vgic_t       *vgic = vm->vgic;

    paddr_t gpa = info->gpa;
    if (GICD_BASE_ADDR <= gpa && gpa < (GICD_BASE_ADDR + 0x0010000)) {
        if (info->esr.dabt.write) {  // 寄存器写到内存
            handle_gicd_write_operations(curr, vm, vgic, info, el2_ctx, gpa);
        } else {  // 内存写到寄存器
            handle_gicd_read_operations(curr, vm, vgic, info, el2_ctx, gpa);
        }
        return;
    }

    logger_warn("VGIC: unsupported access gpa=0x%llx\n", gpa);
}
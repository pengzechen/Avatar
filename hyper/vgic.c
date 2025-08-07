

/*     目前只准备实现 vgic    */

#include <hyper/vgic.h>
#include <aj_types.h>
#include <hyper/vm.h>
#include <exception.h>
#include <hyper/hyper_cfg.h>
#include <io.h>
#include <mem/barrier.h>
#include <thread.h>
#include <mmio.h>
#include <lib/aj_string.h>
#include <exception.h>

#define HIGHEST_BIT_POSITION(x)        \
    ({                                 \
        uint32_t _i = 0;               \
        unsigned long long _val = (x); \
        while (_val >>= 1)             \
        {                              \
            _i++;                      \
        }                              \
        _i;                            \
    })

static vgic_t _vgic[VM_NUM_MAX];
static uint32_t _vgic_num = 0;

static vgic_core_state_t _state[VCPU_NUM_MAX];
static uint32_t _state_num = 0;

void vgic_inject_sgi(tcb_t *task, int32_t int_id);
void vgic_try_inject_pending(tcb_t *task);

vgic_core_state_t *get_vgicc_by_vcpu(tcb_t *task); // if task==NULL, return current task's core state structure
int32_t get_vcpuid(tcb_t *task);                   // if task==NULL, return current task's vcpu id
list_t *get_vcpus(tcb_t *task);                    // if task==NULL, return current vm's vcpus

// 辅助函数：操作 irq_pending_mask 位图
static inline bool vgic_is_irq_pending(vgic_core_state_t *vgicc, uint32_t irq_id)
{
    uint32_t word_idx = irq_id / 32;
    uint32_t bit_idx = irq_id % 32;
    return (vgicc->irq_pending_mask[word_idx] & (1U << bit_idx)) != 0;
}

static inline void vgic_set_irq_pending(vgic_core_state_t *vgicc, uint32_t irq_id)
{
    uint32_t word_idx = irq_id / 32;
    uint32_t bit_idx = irq_id % 32;
    vgicc->irq_pending_mask[word_idx] |= (1U << bit_idx);
}

static inline void vgic_clear_irq_pending(vgic_core_state_t *vgicc, uint32_t irq_id)
{
    uint32_t word_idx = irq_id / 32;
    uint32_t bit_idx = irq_id % 32;
    vgicc->irq_pending_mask[word_idx] &= ~(1U << bit_idx);
}

// 获取 SGI/PPI 的完整 pending 状态（软件 + 硬件）
static uint32_t vgic_get_sgi_ppi_pending_status(vgic_core_state_t *vgicc)
{
    uint32_t pending_status = 0;

    // 1. 从软件设置的 pending 状态开始
    pending_status = vgicc->irq_pending_mask[0];

    // 2. 检查 GICH_LR 中的硬件注入 pending 状态
    for (int32_t lr = 0; lr < GICH_LR_NUM; lr++)
    {
        uint32_t lr_val = vgicc->saved_lr[lr];
        if (lr_val != 0) // LR 不为空
        {
            uint32_t vid = lr_val & 0x3ff;         // Virtual ID
            uint32_t state = (lr_val >> 28) & 0x3; // State field

            // 如果是 SGI/PPI 且处于 pending 状态
            if (vid < 32 && (state == 1)) // state=1 表示 pending
            {
                pending_status |= (1U << vid);
            }
        }
    }

    return pending_status;
}

vgic_t *alloc_vgic()
{
    if (_vgic_num >= VM_NUM_MAX)
    {
        logger_error("No more VGIC can be allocated!\n");
        return NULL;
    }
    vgic_t *vgic = &_vgic[_vgic_num++];
    memset(vgic, 0, sizeof(vgic_t));
    return vgic;
}

vgic_core_state_t *alloc_gicc()
{
    return &_state[_state_num++];
}

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
void vgicd_write(stage2_fault_info_t *info, trap_frame_t *el2_ctx, void *paddr)
{
    uint32_t reg_num;           // Guest寄存器编号 (X0-X30)
    volatile uint64_t *r;       // 指向Guest寄存器的指针
    volatile void *buf;         // 寄存器数据缓冲区
    uint32_t len;              // MMIO操作的数据长度
    volatile uint32_t *dst;     // 目标地址指针

    // 从HSR寄存器中提取寄存器编号和操作大小
    reg_num = info->hsr.dabt.reg;                    // 获取源寄存器编号
    len = 1U << (info->hsr.dabt.size & 0x3U);       // 计算数据长度: 1,2,4,8字节

    // VGICD寄存器通常是32位的，检查操作大小
    if (len != 4U) {
        logger_warn("VGICD write size is not 4, but %u\n", len);
    }

    // 获取Guest寄存器的地址和数据
    r = &el2_ctx->r[reg_num];   // 指向Guest寄存器
    buf = (void *)r;            // 寄存器数据缓冲区

    // 设置目标地址（虚拟GIC寄存器）
    dst = (uint32_t *)paddr;
    logger_debug("VGICD write: (%u bytes) 0x%x R%u\n", len, *dst, reg_num);

    logger_debug("old data: 0x%x\n", *dst);

    // 执行写操作：将Guest寄存器数据写入虚拟GIC寄存器
    // 跳过X30寄存器（链接寄存器），避免破坏返回地址
    if (reg_num != 30U)
    {
        *dst = *(uint32_t *)buf;    // 只写入32位数据
    }

    // 内存屏障：确保写操作对所有CPU核心可见
    dsb(sy);    // 数据同步屏障
    isb();      // 指令同步屏障

    logger_debug("new data: 0x%x\n", *dst);
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
void vgicd_read(stage2_fault_info_t *info, trap_frame_t *el2_ctx, void *paddr)
{
    uint32_t reg_num;           // Guest目标寄存器编号 (X0-X30)
    volatile uint64_t *r;       // 指向Guest寄存器的指针
    volatile void *buf;         // 寄存器数据缓冲区
    volatile uint32_t *src;     // 源地址指针（虚拟GIC寄存器）
    uint32_t len;              // MMIO操作的数据长度
    uint32_t dat;              // 从虚拟GIC寄存器读取的数据

    // 从HSR寄存器中提取寄存器编号和操作大小
    reg_num = info->hsr.dabt.reg;                    // 获取目标寄存器编号
    len = 1U << (info->hsr.dabt.size & 0x3U);       // 计算数据长度: 1,2,4,8字节

    // VGICD寄存器通常是32位的，检查操作大小
    if (len != 4U) {
        logger_warn("VGICD read size is not 4, but %u\n", len);
    }

    // 获取Guest寄存器的地址
    r = &el2_ctx->r[reg_num];   // 指向Guest目标寄存器
    buf = (void *)r;            // 寄存器数据缓冲区

    // 从虚拟GIC寄存器读取数据
    src = (uint32_t *)paddr;    // 源地址（虚拟GIC寄存器）
    dat = *src;                 // 读取32位数据

    logger_debug("VGICD read: 0x%llx -> R%u (data: 0x%x)\n", (uint64_t)paddr, reg_num, dat);

    // 将读取的数据写入Guest寄存器
    // 跳过X30寄存器（链接寄存器），避免破坏返回地址
    if (reg_num != 30U)
    {
        *(uint32_t *)buf = dat;     // 写入寄存器低32位，高32位自动清零
    }

    // 内存屏障：确保读操作和寄存器更新的顺序
    dsb(sy);    // 数据同步屏障
    isb();      // 指令同步屏障
}

// handle gicd emu
void intc_handler(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    struct _vm_t *vm = curr->curr_vm;
    vgic_t *vgic = vm->vgic;

    paddr_t gpa = info->gpa;
    if (GICD_BASE_ADDR <= gpa && gpa < (GICD_BASE_ADDR + 0x0010000))
    {
        if (info->hsr.dabt.write)
        { // 寄存器写到内存
            if (gpa == GICD_CTLR)
            {
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                vgic->gicd_ctlr = r;
                logger_info("      <<< gicd emu write GICD_CTLR: ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d\n", gpa, r, len);
            }
            /* is enable reg - W1S */
            // SGI and PPI enable (GICD_ISENABLER(0))
            else if (gpa == GICD_ISENABLER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);

                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);
                // W1S: 写1置位，写0无效果
                vgicc->sgi_ppi_isenabler |= r;

                // SGI 不需要硬件使能，PPI 在启动时已经打开
                // 这里只是记录虚拟机的使能状态

                logger_info("      <<< gicd emu write GICD_ISENABLER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, enabled_mask: 0x%x\n",
                       gpa, r, len, vgicc->sgi_ppi_isenabler);
            }
            else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                int32_t reg_idx = (gpa - GICD_ISENABLER(0)) / 0x4;
                int32_t base_id = reg_idx * 32;

                // W1S: 写1置位
                vgic->gicd_scenabler[reg_idx-1] |= r;

                // 对每个置位的中断调用硬件使能
                for (int bit = 0; bit < 32; bit++)
                {
                    if (r & (1U << bit))
                    {
                        gic_enable_int(base_id + bit, 1);
                    }
                }

                logger_info("      <<< gicd emu write GICD_ISENABLER(%d): ", reg_idx);
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, enabled_mask: 0x%x\n",
                       gpa, r, len, vgic->gicd_scenabler[reg_idx]);
            }
            // SGI and PPI disable (GICD_ICENABLER(0)) - W1C
            else if (gpa == GICD_ICENABLER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);

                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);
                // W1C: 写1清零，写0无效果
                vgicc->sgi_ppi_isenabler &= ~r;

                // SGI 不需要硬件禁用，PPI 保持硬件使能状态
                // 这里只是记录虚拟机的禁用状态

                logger_info("      <<< gicd emu write GICD_ICENABLER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, enabled_mask: 0x%x\n",
                       gpa, r, len, vgicc->sgi_ppi_isenabler);
            }
            else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                int32_t reg_idx = (gpa - GICD_ICENABLER(0)) / 0x4;
                int32_t base_id = reg_idx * 32;

                // W1C: 写1清零
                vgic->gicd_scenabler[reg_idx-1] &= ~r;

                // 对每个要清零的中断调用硬件禁用
                for (int bit = 0; bit < 32; bit++)
                {
                    if (r & (1U << bit))
                    {
                        gic_enable_int(base_id + bit, 0);
                    }
                }

                logger_info("      <<< gicd emu write GICD_ICENABLER(%d): ", reg_idx);
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, enabled_mask: 0x%x\n",
                       gpa, r, len, vgic->gicd_scenabler[reg_idx]);
            }

            /* is pend reg*/
            // SGI+PPI set pending (GICD_ISPENDER(0)) - W1S
            else if (gpa == GICD_ISPENDER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);

                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);

                // W1S: 写1置位，写0无效果
                // 对每个要设置的位进行处理
                for (int32_t bit = 0; bit < 32; bit++)
                {
                    if (r & (1U << bit))
                    {
                        vgic_set_irq_pending(vgicc, bit);
                        logger_info("Set SGI/PPI %d pending\n", bit);
                    }
                }

                logger_info("      <<< gicd emu write GICD_ISPENDER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d\n", gpa, r, len);
            }
            /* ic pend reg*/
            // SGI+PPI clear pending (GICD_ICPENDER(0)) - W1C
            else if (gpa == GICD_ICPENDER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);

                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);

                // W1C: 写1清零，写0无效果
                // 对每个要清除的位进行处理
                for (int32_t bit = 0; bit < 32; bit++)
                {
                    if (r & (1U << bit))
                    {
                        vgic_clear_irq_pending(vgicc, bit);

                        // 同时需要清除 GICH_LR 中对应的 pending 状态
                        for (int32_t lr = 0; lr < GICH_LR_NUM; lr++)
                        {
                            uint32_t lr_val = vgicc->saved_lr[lr];
                            uint32_t vid = lr_val & 0x3ff;           // Virtual ID
                            if (vid == bit && (lr_val & (1U << 28))) // 检查是否是 pending 状态
                            {
                                // 清除 LR 中的 pending 位 (bit 28)
                                vgicc->saved_lr[lr] &= ~(1U << 28);
                                // 标记该 LR 为空闲
                                vgicc->saved_elsr0 |= (1U << lr);
                                logger_info("Cleared LR%d for SGI/PPI %d\n", lr, bit);
                                break;
                            }
                        }

                        logger_info("Clear SGI/PPI %d pending\n", bit);
                    }
                }

                logger_info("      <<< gicd emu write GICD_ICPENDER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d\n", gpa, r, len);
            }

            // SPI set pending (GICD_ISPENDER(1) and above)
            else if (GICD_ISPENDER(1) <= gpa && gpa < GICD_ICPENDER(0))
            {
                logger_info("      <<< gicd emu write GICD_ISPENDER(spi) - not implemented\n");
            }
            // SPI clear pending (GICD_ICPENDER(1) and above)
            else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_ISACTIVER(0))
            {
                logger_info("      <<< gicd emu write GICD_ICPENDER(spi) - not implemented\n");
            }

            /* I priority reg*/
            // SGI + PPI priority write (per-vCPU)
            else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t len = 1 << (info->hsr.dabt.size & 0x3);
                uint32_t val = el2_ctx->r[reg_num];

                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);
                int32_t offset = gpa - GICD_IPRIORITYR(0);

                // 直接写入 per-vCPU 的优先级数组
                for (int32_t i = 0; i < len; ++i)
                {
                    uint32_t int_id = offset + i;
                    uint8_t pri_raw = (val >> (8 * i)) & 0xFF;

                    if (int_id < GIC_FIRST_SPI)
                    {
                        vgicc->sgi_ppi_ipriorityr[int_id] = pri_raw;

                        // 同时设置硬件优先级（用于实际的中断处理）
                        uint32_t pri = pri_raw >> 3; // 还原 priority 值（只保留高 5 位）
                        gic_set_ipriority(int_id, pri);
                    }
                }

                logger_info("      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi): ");
                logger("offset=%d, len=%d, val=0x%llx\n", offset, len, val);
            }
            // SPI priority write (VM-wide)
            else if (GICD_IPRIORITYR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_IPRIORITYR(SPI_ID_MAX / 4))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t len = 1 << (info->hsr.dabt.size & 0x3);
                uint32_t val = el2_ctx->r[reg_num];

                int32_t offset = gpa - GICD_IPRIORITYR(0);

                // 直接写入 VM 级别的优先级数组
                for (int32_t i = 0; i < len; ++i)
                {
                    uint32_t int_id = offset + i;
                    uint8_t pri_raw = (val >> (8 * i)) & 0xFF;

                    if (int_id < SPI_ID_MAX)
                    {
                        vgic->gicd_ipriorityr[int_id] = pri_raw;

                        // 同时设置硬件优先级（用于实际的中断处理）
                        uint32_t pri = pri_raw >> 3;
                        gic_set_ipriority(int_id, pri);
                    }
                }

                logger_info("      <<< gicd emu write GICD_IPRIORITYR(spi): ");
                logger("offset=%d, len=%d, val=0x%llx\n", offset, len, val);
            }

            /* I cfg reg*/
            // SPI configuration register write
            // SGI (0-15)：配置固定为边沿触发，ICFGR 寄存器对应位为只读
            // PPI (16-31)：配置通常由硬件或系统固定，很少允许软件修改
            else if (GICD_ICFGR(GIC_FIRST_SPI / 16) <= gpa && gpa < GICD_ICFGR(SPI_ID_MAX / 16))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t reg_value = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x3);

                uint32_t reg_offset = (gpa - GICD_ICFGR(0)) / 4;

                // 保存到虚拟 ICFGR 寄存器
                vgic->gicd_icfgr[reg_offset] = reg_value;

                // 直接写入硬件寄存器（简化版本，没有虚拟到物理映射）
                // 注意：bit[0] 保留为0，bit[1] 控制触发方式（0=电平，1=边沿）
                uint32_t masked_value = reg_value & 0xAAAAAAAA; // 只保留奇数位
                write32(masked_value, (void *)gpa);

                logger_info("      <<< gicd emu write GICD_ICFGR(spi): ");
                logger("reg_offset=%d, len=%d, reg_value=0x%x, masked=0x%x\n",
                       reg_offset, len, reg_value, masked_value);
            }

            /* I target reg*/
            // SPI target register write
            else if (GICD_ITARGETSR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_ITARGETSR(SPI_ID_MAX / 4))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t reg_value = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x3);

                uint32_t word_offset = (gpa - GICD_ITARGETSR(0)) / 4;
                ((uint32_t *)vgic->gicd_itargetsr)[word_offset] = reg_value;

                logger_info("      <<< gicd emu write GICD_ITARGETSR(spi): ");
                logger("word_offset=%d, len=%d, reg_value=0x%x\n", word_offset, len, reg_value);
            }

            /* sgi reg*/
            else if (gpa == GICD_SGIR) // wo
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t sgir_val = el2_ctx->r[reg_num];

                uint8_t sgi_int_id = sgir_val & 0xF;
                uint8_t target_list_filter = (sgir_val >> 24) & 0x3;
                uint8_t cpu_target_list = (sgir_val >> 16) & 0xFF;

                tcb_t *curr = (tcb_t *)read_tpidr_el2();
                struct _vm_t *vm = curr->curr_vm;

                uint32_t curr_id = get_vcpuid(curr);

                list_node_t *iter = list_first(&vm->vcpus);
                while (iter)
                {
                    tcb_t *task = list_node_parent(iter, tcb_t, vm_node);
                    uint32_t vcpuid = get_vcpuid(task);
                    switch (target_list_filter)
                    {
                    case 0: // 指定目标 CPU
                        if ((cpu_target_list >> vcpuid) & 1)
                        {
                            vgic_inject_sgi(task, sgi_int_id);
                        }
                        break;
                    case 1: // 其他 CPU
                        if (vcpuid != curr_id)
                        {
                            vgic_inject_sgi(task, sgi_int_id);
                        }
                        break;
                    case 2: // 当前 CPU
                        if (vcpuid == curr_id)
                        {
                            vgic_inject_sgi(task, sgi_int_id);
                        }
                        break;
                    default:
                        logger_error("SGIR: invalid target_list_filter = %d\n", target_list_filter);
                        break;
                    }
                    iter = list_node_next(iter);
                }
                logger_info("      <<< gicd emu write GICD_SGIR(i)\n");
            }
            /* pend sgi reg*/
            else if (GICD_SPENDSGIR(0) <= gpa && gpa < GICD_SPENDSGIR(MAX_SGI_ID / 4))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t reg_value = el2_ctx->r[reg_num];
                uint32_t sgi_reg_idx = (gpa - GICD_SPENDSGIR(0)) / 4;

                // // 处理 4 个 SGI（每个寄存器管理 4 个 SGI）
                // for (int sgi_offset = 0; sgi_offset < 4; sgi_offset++) {
                //     uint32_t sgi_id = sgi_reg_idx * 4 + sgi_offset;
                //     if (sgi_id >= MAX_SGI_ID) break;

                //     uint32_t cpu_mask = (reg_value >> (sgi_offset * 8)) & 0xFF;

                //     // 向每个目标 CPU 注入 SGI
                //     for (int cpu = 0; cpu < 8; cpu++) {
                //         if (cpu_mask & (1 << cpu)) {
                //             // 找到目标 vCPU 并注入 SGI
                //             tcb_t *target_task = find_vcpu_task(cpu);
                //             if (target_task) {
                //                 vgic_inject_sgi(target_task, sgi_id);
                //             }
                //         }
                //     }
                // }
                logger_info("GICD_SPENDSGIR write: reg_idx=%d, value=0x%x\n", sgi_reg_idx, reg_value);
            }
            /* clear pend sgi reg*/
            else if (GICD_CPENDSGIR(0) <= gpa && gpa < GICD_CPENDSGIR(MAX_SGI_ID / 4))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                uint32_t reg_value = el2_ctx->r[reg_num];
                uint32_t sgi_reg_idx = (gpa - GICD_CPENDSGIR(0)) / 4;

                // // 清除 SGI pending 状态
                // for (int sgi_offset = 0; sgi_offset < 4; sgi_offset++) {
                //     uint32_t sgi_id = sgi_reg_idx * 4 + sgi_offset;
                //     if (sgi_id >= MAX_SGI_ID) break;

                //     uint32_t cpu_mask = (reg_value >> (sgi_offset * 8)) & 0xFF;

                //     for (int cpu = 0; cpu < 8; cpu++) {
                //         if (cpu_mask & (1 << cpu)) {
                //             tcb_t *target_task = find_vcpu_task(cpu);
                //             if (target_task) {
                //                 vgic_clear_sgi_pending(target_task, sgi_id);
                //             }
                //         }
                //     }
                // }
                logger_info("GICD_CPENDSGIR write: reg_idx=%d, value=0x%x\n", sgi_reg_idx, reg_value);
            }
            else
            {
            }
        }
        else
        { // 内存写到寄存器
            if (gpa == GICD_CTLR)
            {
                vgicd_read(info, el2_ctx, &vgic->gicd_ctlr);

                logger_warn("      >>> gicd emu read GICD_CTLR: ");
                logger("ctlr: 0x%x\n", vgic->gicd_ctlr);
            }
            else if (gpa == GICD_TYPER) // ro
            {
                uint32_t typer = gic_get_typer();
                // 清除原有的 CPU 数量字段 (bits [7:5])
                typer &= ~(0x7 << 5);
                // 设置虚拟机的 vCPU 数量 (CPU number = value + 1)
                uint32_t vcpu_count = vgic->vm->vcpu_cnt;
                if (vcpu_count > 0)
                {
                    typer |= ((vcpu_count - 1) & 0x7) << 5;
                }
                vgicd_read(info, el2_ctx, &typer);
                logger_warn("      >>> gicd emu read GICD_TYPER: ");
                logger("typer: 0x%x, vcpu_cnt: %d\n", typer, vcpu_count);
            }
            else if (gpa == GICD_IIDR) // ro
            {
                uint32_t iidr = gic_get_iidr();
                vgicd_read(info, el2_ctx, &iidr);
                logger_warn("      >>> gicd emu read GICD_IIDR: ");
                logger("iidr: 0x%x\n", iidr);
            }

            /*  is enable reg*/
            // SGI and PPI enable read (GICD_ISENABLER(0))
            else if (gpa == GICD_ISENABLER(0))
            {
                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);
                uint32_t enabled_mask = vgicc->sgi_ppi_isenabler;
                vgicd_read(info, el2_ctx, &enabled_mask);

                logger_warn("      >>> gicd emu read GICD_ISENABLER(0): ");
                logger("enabled_mask: 0x%x\n", enabled_mask);
            }
            // SPI enable read (GICD_ISENABLER(1) and above)
            else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0))
            {
                int32_t reg_idx = (gpa - GICD_ISENABLER(0)) / 0x4;
                uint32_t enabled_mask = vgic->gicd_scenabler[reg_idx];
                vgicd_read(info, el2_ctx, &enabled_mask);

                logger_warn("      >>> gicd emu read GICD_ISENABLER(%d): ", reg_idx);
                logger("enabled_mask: 0x%x\n", enabled_mask);
            }
            // SGI and PPI disable read (GICD_ICENABLER(0))
            else if (gpa == GICD_ICENABLER(0))
            {
                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);
                uint32_t enabled_mask = vgicc->sgi_ppi_isenabler;
                vgicd_read(info, el2_ctx, &enabled_mask);

                logger_warn("      >>> gicd emu read GICD_ICENABLER(0): ");
                logger("enabled_mask: 0x%x\n", enabled_mask);
            }
            // SPI disable read (GICD_ICENABLER(1) and above)
            else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0))
            {
                int32_t reg_idx = (gpa - GICD_ICENABLER(0)) / 0x4;
                uint32_t enabled_mask = vgic->gicd_scenabler[reg_idx];
                vgicd_read(info, el2_ctx, &enabled_mask);

                logger_warn("      >>> gicd emu read GICD_ICENABLER(%d): ", reg_idx);
                logger("enabled_mask: 0x%x\n", enabled_mask);
            }

            /* is pend reg*/
            // SGI+PPI pending read (GICD_ISPENDER(0))
            else if (gpa == GICD_ISPENDER(0))
            {
                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);
                uint32_t pending_status = vgic_get_sgi_ppi_pending_status(vgicc);
                vgicd_read(info, el2_ctx, &pending_status);

                logger_warn("      >>> gicd emu read GICD_ISPENDER(0): ");
                logger("pending_status: 0x%x\n", pending_status);
            }
            /* ic pend reg*/
            // SGI+PPI pending read (GICD_ICPENDER(0)) - 返回相同的值
            else if (gpa == GICD_ICPENDER(0))
            {
                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);
                uint32_t pending_status = vgic_get_sgi_ppi_pending_status(vgicc);
                vgicd_read(info, el2_ctx, &pending_status);

                logger_warn("      >>> gicd emu read GICD_ICPENDER(0): ");
                logger("pending_status: 0x%x\n", pending_status);
            }

            // SPI pending read (GICD_ISPENDER(1) and above)
            else if (GICD_ISPENDER(1) <= gpa && gpa < GICD_ICPENDER(0))
            {
                logger_warn("      >>> gicd emu read GICD_ISPENDER(spi) - not implemented\n");
            }
            // SPI pending read (GICD_ICPENDER(1) and above)
            else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_IPRIORITYR(0))
            {
                logger_warn("      >>> gicd emu read GICD_ICPENDER(spi) - not implemented\n");
            }

            /* I priority reg*/
            // SGI+PPI priority read (per-vCPU)
            else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4))
            {
                vgic_core_state_t *vgicc = get_vgicc_by_vcpu(curr);
                uint32_t word_offset = (gpa - GICD_IPRIORITYR(0)) / 4;
                uint32_t priority_word = ((uint32_t *)vgicc->sgi_ppi_ipriorityr)[word_offset];
                vgicd_read(info, el2_ctx, &priority_word);

                logger_warn("      >>> gicd emu read GICD_IPRIORITYR(sgi-ppi): ");
                logger("word_offset=%d, priority_word=0x%x\n", word_offset, priority_word);
            }
            // SPI priority read (VM-wide)
            else if (GICD_IPRIORITYR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_IPRIORITYR(SPI_ID_MAX / 4))
            {
                uint32_t word_offset = (gpa - GICD_IPRIORITYR(0)) / 4;
                uint32_t priority_word = ((uint32_t *)vgic->gicd_ipriorityr)[word_offset];
                vgicd_read(info, el2_ctx, &priority_word);

                logger_warn("      >>> gicd emu read GICD_IPRIORITYR(spi): ");
                logger("word_offset=%d, priority_word=0x%x\n", word_offset, priority_word);
            }

            /* I cfg reg*/
            // SGI+PPI configuration register read (固定配置)
            else if (GICD_ICFGR(0) <= gpa && gpa < GICD_ICFGR(GIC_FIRST_SPI / 16))
            {
                // SGI (0-15): 固定为边沿触发 (0xAAAAAAAA 的低16位部分)
                // PPI (16-31): 通常固定配置，这里假设为电平触发
                uint32_t cfg_word;
                uint32_t reg_offset = (gpa - GICD_ICFGR(0)) / 4;

                if (reg_offset == 0)
                {
                    // GICD_ICFGR(0): 包含 SGI 0-15 和 PPI 16-31
                    // SGI 0-15: 边沿触发 (0xAAAA)
                    // PPI 16-31: 电平触发 (0x0000)
                    cfg_word = 0x0000AAAA;
                }
                else
                {
                    // GICD_ICFGR(1): 只包含 PPI，电平触发
                    cfg_word = 0x00000000;
                }

                vgicd_read(info, el2_ctx, &cfg_word);

                logger_warn("      >>> gicd emu read GICD_ICFGR(sgi-ppi): ");
                logger("reg_offset=%d, cfg_word=0x%x\n", reg_offset, cfg_word);
            }
            // SPI configuration register read
            else if (GICD_ICFGR(GIC_FIRST_SPI / 16) <= gpa && gpa < GICD_ICFGR(SPI_ID_MAX / 16))
            {
                uint32_t reg_offset = (gpa - GICD_ICFGR(0)) / 4;
                uint32_t cfg_word = vgic->gicd_icfgr[reg_offset];
                vgicd_read(info, el2_ctx, &cfg_word);

                logger_warn("      >>> gicd emu read GICD_ICFGR(spi): ");
                logger("reg_offset=%d, cfg_word=0x%x\n", reg_offset, cfg_word);
            }

            /* I target reg*/
            // SGI+PPI target register read (returns current vCPU mask)
            // SGI 和 PPI 的目标寄存器总是返回当前处理器的位掩码，因为这些中断只能由当前处理器处理。
            else if (GICD_ITARGETSR(0) <= gpa && gpa < GICD_ITARGETSR(GIC_FIRST_SPI / 4))
            {
                uint32_t vcpu_id = get_vcpuid(curr);
                uint8_t value = 1 << vcpu_id;
                uint32_t target_word = (value << 24) | (value << 16) | (value << 8) | value;
                vgicd_read(info, el2_ctx, &target_word);

                logger_warn("      >>> gicd emu read GICD_ITARGETSR(sgi-ppi): ");
                logger("vcpu_id=%d, target_word=0x%x\n", vcpu_id, target_word);
            }
            // SPI target register read
            else if (GICD_ITARGETSR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_ITARGETSR(SPI_ID_MAX / 4))
            {
                uint32_t word_offset = (gpa - GICD_ITARGETSR(0)) / 4;
                uint32_t target_word = ((uint32_t *)vgic->gicd_itargetsr)[word_offset];
                vgicd_read(info, el2_ctx, &target_word);

                logger_warn("      >>> gicd emu read GICD_ITARGETSR(spi): ");
                logger("word_offset=%d, target_word=0x%x\n", word_offset, target_word);
            }
        }
        return;
    }

    logger_warn("[intc_handler]: unsupported access gpa: ");
    logger("0x%llx\n", gpa);
}

list_t *get_vcpus(tcb_t *task)
{
    struct _vm_t *vm = NULL;
    if (!task)
    {
        tcb_t *task = (tcb_t *)read_tpidr_el2();
    }
    vm = task->curr_vm;
    return &vm->vcpus;
}

int32_t get_vcpuid(tcb_t *task)
{
    if (!task)
    {
        tcb_t *task = (tcb_t *)read_tpidr_el2();
    }
    return (task->cpu_info->sys_reg->mpidr_el1 & 0xff);
}

vgic_core_state_t *get_vgicc_by_vcpu(tcb_t *task)
{
    if (!task)
    {
        tcb_t *task = (tcb_t *)read_tpidr_el2();
    }
    struct _vm_t *vm = task->curr_vm;
    vgic_t *vgic = vm->vgic;
    vgic_core_state_t *vgicc = NULL;

    list_node_t *iter = list_first(&vm->vcpus);
    int32_t i = 0;
    while (iter)
    {
        tcb_t *iter_vcpu = list_node_parent(iter, tcb_t, vm_node);
        if (task == iter_vcpu)
        {
            vgicc = vgic->core_state[i];
            break; // 找到了，直接退出
        }
        i++;
        iter = list_node_next(iter);
    }
    if (!vgicc)
    {
        logger("vgicc from task: %d\n", task->task_id);
        logger("vm id: %d\n", vm->vm_id);
        while (1)
        {
            ;
        }
        return NULL;
    }
    return vgicc;
}

// --------------------------------------------------------
// ==================      Debug 使用     ==================
// --------------------------------------------------------
void vgicc_dump(vgic_core_state_t *vgicc)
{
    logger_info("====== VGICC Dump (vCPU ID: %u) ======\n", vgicc->id);

    logger_info("VMCR  = 0x%08x\n", vgicc->vmcr);
    logger_info("ELSR0 = 0x%08x\n", vgicc->saved_elsr0);
    logger_info("APR   = 0x%08x\n", vgicc->saved_apr);
    logger_info("HCR   = 0x%08x\n", vgicc->saved_hcr);

    for (int32_t i = 0; i < GICH_LR_NUM; i++)
    {
        logger_info("LR[%1d] = 0x%08x\n", i, vgicc->saved_lr[i]);
    }

    logger_info("Pending IRQs:\n");
    for (int32_t i = 0; i < SPI_ID_MAX; i++)
    {
        if (vgic_is_irq_pending(vgicc, i))
        {
            logger_info("  IRQ %d is pending\n", i);
        }
    }

    logger_info("======================================\n");
}

void vgicc_hw_dump(void)
{
    logger_info("====== VGICC HW Dump ======\n");

    uint32_t vmcr = mmio_read32((void *)(GICH_VMCR));
    uint32_t elsr0 = mmio_read32((void *)(GICH_ELSR0));
    uint32_t elsr1 = mmio_read32((void *)(GICH_ELSR1));
    uint32_t apr = mmio_read32((void *)(GICH_APR));
    uint32_t hcr = mmio_read32((void *)(GICH_HCR));
    uint32_t vtr = mmio_read32((void *)(GICH_VTR));
    uint32_t misr = mmio_read32((void *)(GICH_MISR));

    logger_info("VMCR  = 0x%08x\n", vmcr);
    logger_info("ELSR0 = 0x%08x\n", elsr0);
    logger_info("ELSR1 = 0x%08x\n", elsr1);
    logger_info("APR   = 0x%08x\n", apr);
    logger_info("HCR   = 0x%08x\n", hcr);
    logger_info("VTR   = 0x%08x\n", vtr);
    logger_info("MISR  = 0x%08x\n", misr);

    for (int32_t i = 0; i < GICH_LR_NUM; i++)
    {
        uint32_t lr = mmio_read32((void *)(GICH_LR(i)));
        if (lr != 0)
        {
            uint32_t vid = lr & 0x3ff;
            uint32_t pid = (lr >> 10) & 0x3ff;
            uint32_t pri = (lr >> 23) & 0x1f;
            uint32_t state = (lr >> 28) & 0x3;
            uint32_t grp1 = (lr >> 30) & 0x1;
            uint32_t hw = (lr >> 31) & 0x1;

            logger_info("LR[%1d] = 0x%08x (VID=%d, PID=%d, PRI=%d, STATE=%d, GRP1=%d, HW=%d)\n",
                        i, lr, vid, pid, pri, state, grp1, hw);
        }
        else
        {
            logger_info("LR[%1d] = 0x%08x (empty)\n", i, lr);
        }
    }

    logger_info("======================================\n");
}

static void vgicc_dump_if_changed(vgic_core_state_t *state, uint32_t vcpu_id, uint32_t task_id, uint32_t vm_id)
{
    static uint32_t last_vmcr[8] = {0};
    static uint32_t last_elsr0[8] = {0};
    static uint32_t last_hcr[8] = {0};
    static uint32_t last_lr[8][GICH_LR_NUM] = {0};
    static bool initialized[8] = {false};

    if (vcpu_id >= 8)
        return;

    bool changed = false;

    // 首次初始化或检测变化
    if (!initialized[vcpu_id])
    {
        changed = true;
        initialized[vcpu_id] = true;
    }
    else
    {
        if (last_vmcr[vcpu_id] != state->vmcr ||
            last_elsr0[vcpu_id] != state->saved_elsr0 ||
            last_hcr[vcpu_id] != state->saved_hcr)
        {
            changed = true;
        }

        if (!changed)
        {
            for (int32_t i = 0; i < GICH_LR_NUM; i++)
            {
                if (last_lr[vcpu_id][i] != state->saved_lr[i])
                {
                    changed = true;
                    break;
                }
            }
        }
    }

    if (changed)
    {
        logger_warn("====== vCPU %d Hardware State Changed ======\n", vcpu_id);
        logger_info("Task ID: %d, VM ID: %d\n", task_id, vm_id);
        logger_info("VMCR: 0x%08x, ELSR0: 0x%08x, HCR: 0x%08x\n",
                    state->vmcr, state->saved_elsr0, state->saved_hcr);

        for (int32_t i = 0; i < GICH_LR_NUM; i++)
        {
            if (state->saved_lr[i] != 0)
            {
                logger_info("LR[%d]: 0x%08x\n", i, state->saved_lr[i]);
            }
        }
        logger_warn("==========================================\n");

        // 更新缓存
        last_vmcr[vcpu_id] = state->vmcr;
        last_elsr0[vcpu_id] = state->saved_elsr0;
        last_hcr[vcpu_id] = state->saved_hcr;
        for (int32_t i = 0; i < GICH_LR_NUM; i++)
        {
            last_lr[vcpu_id][i] = state->saved_lr[i];
        }
    }
}

// 检查虚拟中断注入的完整状态
void vgic_check_injection_status(void)
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();

    logger_info("====== VGIC Injection Status Check ======\n");

    if (!curr)
    {
        logger_error("No current task\n");
        return;
    }

    logger_info("Current task ID: %d\n", curr->task_id);

    if (!curr->curr_vm)
    {
        logger_warn("Current task is not a VM task\n");
        return;
    }

    logger_info("Current VM ID: %d\n", curr->curr_vm->vm_id);

    // 检查 HCR_EL2 设置
    uint64_t hcr_el2 = read_hcr_el2();
    logger_info("HCR_EL2: 0x%llx\n", hcr_el2);
    logger_info("  IMO (bit 4): %s\n", (hcr_el2 & (1ULL << 4)) ? "enabled" : "disabled");
    logger_info("  FMO (bit 3): %s\n", (hcr_el2 & (1ULL << 3)) ? "enabled" : "disabled");
    logger_info("  VM (bit 0): %s\n", (hcr_el2 & (1ULL << 0)) ? "enabled" : "disabled");

    // 检查 DAIF 状态
    uint32_t daif = get_daif();
    logger_info("DAIF: 0x%x\n", daif);
    logger_info("  I (IRQ masked): %s\n", (daif & (1 << 7)) ? "yes" : "no");
    logger_info("  F (FIQ masked): %s\n", (daif & (1 << 6)) ? "yes" : "no");

    // 检查 GICH 状态
    vgicc_hw_dump();

    logger_info("==========================================\n");
}

// --------------------------------------------------------
// ==================       中断注入       ==================
// --------------------------------------------------------
// 给指定vcpu注入一个sgi中断
void vgic_inject_sgi(tcb_t *task, int32_t int_id)
{
    // 参数检查
    if (int_id < 0 || int_id > 15)
    {
        logger_error("Invalid SGI ID: %d (must be 0-15)\n", int_id);
        return;
    }

    if (!task)
    {
        logger_error("Invalid task for SGI injection\n");
        return;
    }

    if (!task->curr_vm)
    {
        logger_error("Task is not a VM task\n");
        return;
    }

    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);
    if (!vgicc)
    {
        logger_error("Failed to get VGICC for task %d\n", task->task_id);
        return;
    }

    // 如果已 pending，不重复注入
    if (vgic_is_irq_pending(vgicc, int_id))
    {
        // logger_warn("SGI %d already pending on vCPU %d, skip inject.\n", int_id, task->task_id);
        return;
    }

    // 检查中断是否使能
    if (!(vgicc->sgi_ppi_isenabler & (1U << int_id)))
    {
        // logger_warn("SGI %d is disabled on vCPU %d, still inject to pending.\n", int_id, task->task_id);
    }

    // 标记为 pending
    vgic_set_irq_pending(vgicc, int_id);

    logger_info("[pcpu: %d]: Inject SGI id: %d to vCPU: %d(task: %d)\n",
                get_current_cpu_id(), int_id, get_vcpuid(task), task->task_id);

    // 如果当前正在运行此 vCPU，尝试立即注入
    if (task == (tcb_t *)read_tpidr_el2())
    {
        logger_info("[pcpu: %d]: (Is running)Try to inject pending SGI for vCPU: %d (task: %d)\n",
                    get_current_cpu_id(), get_vcpuid(task), task->task_id);
        vgic_try_inject_pending(task);
    }

    // vgicc_dump(vgicc);
}

// 给指定vcpu注入一个ppi中断
void vgic_inject_ppi(tcb_t *task, int32_t irq_id)
{
    // irq_id: 16~31

    // 参数检查
    if (irq_id < 16 || irq_id > 31)
    {
        logger_error("Invalid PPI ID: %d (must be 16-31)\n", irq_id);
        return;
    }

    if (!task)
    {
        logger_error("Invalid task for PPI injection\n");
        return;
    }

    if (!task->curr_vm)
    {
        logger_error("Task is not a VM task\n");
        return;
    }

    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);
    if (!vgicc)
    {
        logger_error("Failed to get VGICC for task %d\n", task->task_id);
        return;
    }

    // 检查中断是否已经 pending
    if (vgic_is_irq_pending(vgicc, irq_id))
    {
        // logger_warn("PPI %d already pending on vCPU %d, skip inject.\n", irq_id, task->task_id);
        return;
    }

    // 检查中断是否使能
    if (!(vgicc->sgi_ppi_isenabler & (1U << irq_id)))
    {
        // logger_warn("PPI %d is disabled on vCPU %d, abort inject.\n", irq_id, task->task_id);
        return;
    }

    // 标记为 pending
    vgic_set_irq_pending(vgicc, irq_id);

    // logger_debug("[pcpu: %d]: Inject PPI id: %d to vCPU: %d(task: %d)\n",
    //              get_current_cpu_id(), irq_id, get_vcpuid(task), task->task_id);

    // 如果当前正在运行此 vCPU，尝试立即注入到 GICH_LR
    if (task == (tcb_t *)read_tpidr_el2())
    {
        // logger_info("[pcpu: %d]: (Is running)Try to inject pending PPI for vCPU: %d (task: %d)\n",
        //             get_current_cpu_id(), get_vcpuid(task), task->task_id);
        vgic_try_inject_pending(task);
    }
}

// 给指定vcpu注入一个spi中断
void vgic_inject_spi(tcb_t *task, int32_t irq_id)
{
    // irq_id: 32~1019
    if (irq_id < 32 || irq_id > 1019)
    {
        logger_error("Invalid SPI ID: %d (must be 32-1019)\n", irq_id);
        return;
    }
    if (!task)
    {
        logger_error("Invalid task for SPI injection\n");
        return;
    }
    if (!task->curr_vm)
    {
        logger_error("Task is not a VM task\n");
        return;
    }

    vgic_t *vgic = task->curr_vm->vgic;
    // 检查中断是否使能
    if (!(vgic->gicd_scenabler[(irq_id / 32) - 1] & (1U << (irq_id % 32))))
    {
        logger_warn("SPI %d is disabled on vCPU %d, abort inject.\n", irq_id, task->task_id);
        // return;
    }

    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);
    if (!vgicc)
    {
        logger_error("Failed to get VGICC for task %d\n", task->task_id);
        return;
    }

    // 检查中断是否已经 pending
    if (vgic_is_irq_pending(vgicc, irq_id))
    {
        // logger_warn("SPI %d already pending on vCPU %d, skip inject.\n", irq_id, task->task_id);
        return;
    }

    // 标记为 pending
    vgic_set_irq_pending(vgicc, irq_id);

    // logger_debug("[pcpu: %d]: Inject SPI id: %d to vCPU: %d(task: %d)\n",
    //              get_current_cpu_id(), irq_id, get_vcpuid(task), task->task_id);

    // 如果当前正在运行此 vCPU，尝试立即注入到 GICH_LR
    if (task == (tcb_t *)read_tpidr_el2())
    {
        // logger_info("[pcpu: %d]: (Is running)Try to inject pending SPI for vCPU: %d (task: %d)\n",
        //             get_current_cpu_id(), get_vcpuid(task), task->task_id);
        vgic_try_inject_pending(task);
    }
}


// vm进入的时候把 pending 的中断注入到 GICH_LR
void vgic_try_inject_pending(tcb_t *task)
{
    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);
    vgic_t *vgic = task->curr_vm->vgic;

    // 使用软件保存的 ELSR0 来判断空闲的 LR
    uint64_t elsr = vgicc->saved_elsr0; // 目前你只有 ELSR0，够用（最多 32 个 LR）

    // 处理 SGI (0-15) 和 PPI (16-31)
    for (int i = 0; i < 32; ++i)
    {
        if (!vgic_is_irq_pending(vgicc, i))
            continue;

        int freelr = -1;
        for (int lr = 0; lr < GICH_LR_NUM; lr++)
        {
            if ((elsr >> lr) & 0x1)
            {
                freelr = lr;
                break;
            }

            // 防止重复注入：判断 saved_lr 中是否已经有相同中断
            uint32_t val = vgicc->saved_lr[lr];
            uint32_t vid = (val >> GICH_LR_PID_SHIFT) & 0x3ff;
            if (vid == i)
            {
                freelr = -1;
                break;
            }
        }

        if (freelr < 0)
        {
            // logger_warn("No free LR for IRQ %d (in memory), delay inject.\n", i);
            break;
        }

        int32_t vcpu_id = get_vcpuid(task);
        uint32_t lr_val;

        // 根据中断类型创建不同的 LR 值
        if (i < 16)
        {
            // SGI: 使用软件中断格式
            lr_val = gic_make_virtual_software_sgi(i, /*cpu_id=*/vcpu_id, 0, 0);
        }
        else
        {
            // PPI: 使用硬件中断格式
            lr_val = gic_make_virtual_hardware_interrupt(i, i, 0, 0);
        }

        // 将虚拟中断写入到内存中的 LR
        vgicc->saved_lr[freelr] = lr_val;
        // 标记该 LR 不再空闲（ELSR 置位为 0 表示 occupied）
        vgicc->saved_elsr0 &= ~(1U << freelr);

        // 清除 pending 标志
        vgic_clear_irq_pending(vgicc, i);

        // const char *irq_type = (i < 16) ? "SGI" : "PPI";
        // logger_info("[pcpu: %d]: Injected %s %d into LR%d for vCPU: %d (task: %d), LR value: 0x%x\n",
        //             get_current_cpu_id(), irq_type, i, freelr, vcpu_id, task->task_id, lr_val);

        // dev use
        // gicc_restore_core_state();
        // vgicc_hw_dump();
        // vgic_sw_inject_test(i);
        // vgicc_hw_dump();
    }

    // 处理 SPI (32-SPI_ID_MAX)
    for (int i = 32; i < SPI_ID_MAX; ++i)
    {
        if (!vgic_is_irq_pending(vgicc, i))
            continue;

        // 检查 SPI 是否使能
        if (!(vgic->gicd_scenabler[(i / 32) - 1] & (1U << (i % 32))))
        {
            // SPI 未使能，跳过注入
            continue;
        }

        int freelr = -1;
        for (int lr = 0; lr < GICH_LR_NUM; lr++)
        {
            if ((elsr >> lr) & 0x1)
            {
                freelr = lr;
                break;
            }

            // 防止重复注入：判断 saved_lr 中是否已经有相同中断
            uint32_t val = vgicc->saved_lr[lr];
            uint32_t vid = (val >> GICH_LR_PID_SHIFT) & 0x3ff;
            if (vid == i)
            {
                freelr = -1;
                break;
            }
        }

        if (freelr < 0)
        {
            // logger_warn("No free LR for SPI %d (in memory), delay inject.\n", i);
            break;
        }

        // SPI: 使用硬件中断格式
        uint32_t lr_val = gic_make_virtual_hardware_interrupt(i, i, 0, 0);

        // 将虚拟中断写入到内存中的 LR
        vgicc->saved_lr[freelr] = lr_val;
        // 标记该 LR 不再空闲（ELSR 置位为 0 表示 occupied）
        vgicc->saved_elsr0 &= ~(1U << freelr);

        // 清除 pending 标志
        vgic_clear_irq_pending(vgicc, i);

        // logger_info("[pcpu: %d]: Injected SPI %d into LR%d for vCPU: %d (task: %d), LR value: 0x%x\n",
        //             get_current_cpu_id(), i, freelr, get_vcpuid(task), task->task_id, lr_val);

        // dev use
        // gicc_restore_core_state();
        // vgicc_hw_dump();
        // vgic_sw_inject_test(i);
        // vgicc_hw_dump();
    }
}

// --------------------------------------------------------
// ==================    GICC 状态保存     ==================
// --------------------------------------------------------
void gicc_save_core_state()
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    if (!curr->curr_vm)
        return;
    vgic_core_state_t *state = get_vgicc_by_vcpu(curr);

    state->vmcr = mmio_read32((void *)GICH_VMCR);
    state->saved_elsr0 = mmio_read32((void *)GICH_ELSR0);
    state->saved_apr = mmio_read32((void *)GICH_APR);
    state->saved_hcr = mmio_read32((void *)GICH_HCR);

    for (int32_t i = 0; i < GICH_LR_NUM; i++)
        state->saved_lr[i] = gic_read_lr(i);
}

void gicc_restore_core_state()
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    if (!curr->curr_vm)
        return;
    vgic_core_state_t *state = get_vgicc_by_vcpu(curr);

    mmio_write32(state->vmcr, (void *)GICH_VMCR);
    mmio_write32(state->saved_elsr0, (void *)GICH_ELSR0);
    mmio_write32(state->saved_apr, (void *)GICH_APR);
    mmio_write32(state->saved_hcr, (void *)GICH_HCR);

    for (int32_t i = 0; i < GICH_LR_NUM; i++)
        gic_write_lr(i, state->saved_lr[i]);

    // vgicc_dump_if_changed(state, get_vcpuid(curr), curr->task_id, curr->curr_vm->vm_id);
}


// --------------------------------------------------------
// ==================    直通中断操作     ===================
// --------------------------------------------------------
void vgic_passthrough_irq(int32_t irq_id)
{
    // find vms and vcpus need to inject 
    // todo
    tcb_t * task = (tcb_t *)read_tpidr_el2();
    vgic_inject_spi(task, irq_id);
}


// vgic inject test
void vgic_hw_inject_test(uint32_t vector)
{
    logger_info("vgic inject vector: %d\n", vector);

    // 检查当前是否有运行的虚拟机
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    if (!curr || !curr->curr_vm)
    {
        logger_warn("No current VM for interrupt injection\n");
        return;
    }

    uint32_t mask = gic_make_virtual_hardware_interrupt(vector, vector, 0, 0);

    uint32_t elsr0 = gic_elsr0();
    uint32_t elsr1 = gic_elsr1();
    uint64_t elsr = ((uint64_t)elsr1 << 32) | elsr0;

    uint32_t is_active = gic_apr();
    uint32_t pri = gic_lr_read_pri(mask);
    uint32_t irq_no = gic_lr_read_vid(mask);
    int32_t freelr = -1;

    logger_info("ELSR: 0x%llx, APR: 0x%x, PRI: 0x%x, IRQ: %d\n", elsr, is_active, pri, irq_no);

    for (int32_t i = 0; i < GICH_LR_NUM; i++)
    {
        if ((elsr >> i) & 0x1)
        {
            if (freelr < 0)
                freelr = i;

            continue;
        }

        uint32_t lr_val = gic_read_lr(i);
        uint32_t existing_vector = (lr_val >> GICH_LR_PID_SHIFT) & 0x3ff;
        if (existing_vector == vector)
        {
            logger_warn("vgic inject, vector %d already in lr%d (val=0x%x)\n", vector, i, lr_val);
            return; // busy
        }
    }

    if (freelr < 0)
    {
        logger_error("No free LR available for vector %d\n", vector);
        return;
    }

    logger_info("Injecting vector %d into LR%d, mask=0x%x\n", vector, freelr, mask);
    gic_write_lr(freelr, mask);

    // 确保写入生效
    dsb(sy);
    isb();

    // 验证写入
    uint32_t written_val = gic_read_lr(freelr);
    logger_info("LR%d written value: 0x%x\n", freelr, written_val);
}

void vgic_sw_inject_test(uint32_t vector)
{
    logger_info("vgic inject vector: %d\n", vector);

    // 检查当前是否有运行的虚拟机
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    if (!curr || !curr->curr_vm)
    {
        logger_warn("No current VM for interrupt injection\n");
        return;
    }

    uint32_t mask = gic_make_virtual_software_sgi(vector, /*cpu_id=*/0, 0, 0);

    uint32_t elsr0 = gic_elsr0();
    uint32_t elsr1 = gic_elsr1();
    uint64_t elsr = ((uint64_t)elsr1 << 32) | elsr0;

    uint32_t is_active = gic_apr();
    uint32_t pri = gic_lr_read_pri(mask);
    uint32_t irq_no = gic_lr_read_vid(mask);
    int32_t freelr = -1;

    logger_info("ELSR: 0x%llx, APR: 0x%x, PRI: 0x%x, IRQ: %d\n", elsr, is_active, pri, irq_no);

    for (int32_t i = 0; i < GICH_LR_NUM; i++)
    {
        if ((elsr >> i) & 0x1)
        {
            if (freelr < 0)
                freelr = i;

            continue;
        }

        uint32_t lr_val = gic_read_lr(i);
        uint32_t existing_vector = (lr_val >> GICH_LR_PID_SHIFT) & 0x3ff;
        if (existing_vector == vector)
        {
            logger_warn("vgic inject, vector %d already in lr%d (val=0x%x)\n", vector, i, lr_val);
            return; // busy
        }
    }

    if (freelr < 0)
    {
        logger_error("No free LR available for vector %d\n", vector);
        return;
    }

    logger_info("Injecting vector %d into LR%d, mask=0x%x\n", vector, freelr, mask);
    gic_write_lr(freelr, mask);

    // 确保写入生效
    dsb(sy);
    isb();

    // 验证写入
    uint32_t written_val = gic_read_lr(freelr);
    logger_info("LR%d written value: 0x%x\n", freelr, written_val);
}
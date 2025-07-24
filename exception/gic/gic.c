

/*   ============= gic.c ================*/

#include <gic.h>
#include <aj_types.h>
#include <io.h>
#include <mmio.h>

struct gic_t _gicv2;

void gic_test_init(void)
{
    logger("    gicd enable %s\n", read32((void *)GICD_CTLR) ? "ok" : "error");
    logger("    gicc enable %s\n", read32((void *)GICC_CTLR) ? "ok" : "error");
    logger("    irq numbers: %d\n", _gicv2.irq_nr);
    logger("    cpu num: %d\n", cpu_num());
}

// ===========================================
// 下面是gic的初始化函数，包括el1kernel的初始化和hypervisor的初始化
// ===========================================

// el1 kernel gicc的初始化。smp启动副核执行
void gicc_init()
{
    // 设置优先级 为 0xf8
    write32(0xff - 7, (void *)GICC_PMR);
    // EOImodeNS, bit [9] Controls the behavior of Non-secure accesses to GICC_EOIR GICC_AEOIR, and GICC_DIR
    write32(GICC_CTRL_ENABLE_GROUP0, (void *)GICC_CTLR);
}

// el2 hypervisor gicc的初始化。smp启动副核执行
void gicc_el2_init()
{
    // 设置优先级 为 0xf8
    write32(0xff - 7, (void *)GICC_PMR);
    // EOImodeNS, bit [9] Controls the behavior of Non-secure accesses to GICC_EOIR GICC_AEOIR, and GICC_DIR
    write32(GICC_CTRL_ENABLE_GROUP0 | (1 << 9), (void *)GICC_CTLR);

    // bit [2] 当虚拟中断列表寄存器中没有条目时，会产生中断。
    write32((1 << 0), (void *)GICH_HCR);
    write32((1 << 0), (void *)GICH_VMCR);
}

// gicd g0, g1  gicc enable。smp启动首核执行
void gic_init(void)
{
    _gicv2.irq_nr = GICD_TYPER_IRQS(read32((void *)GICD_TYPER));
    if (_gicv2.irq_nr > 1020)
    {
        _gicv2.irq_nr = 1020;
    }

    // GICD 启用组0中断、组1中断转发，服从优先级规则
    write32(GICD_CTRL_ENABLE_GROUP0 | GICD_CTRL_ENABLE_GROUP1, (void *)GICD_CTLR);

    gicc_init();

    gic_test_init();
}

// gicd g0, g1  gicc,  gich enable。 smp启动首核执行
void gic_virtual_init(void)
{
    // 获得 gicd irq numbers
    _gicv2.irq_nr = GICD_TYPER_IRQS(read32((void *)GICD_TYPER));
    if (_gicv2.irq_nr > 1020)
    {
        _gicv2.irq_nr = 1020;
    }

    // GICD 启用组0中断、组1中断转发，服从优先级规则
    write32(GICD_CTRL_ENABLE_GROUP0 | GICD_CTRL_ENABLE_GROUP1, (void *)GICD_CTLR);

    gicc_el2_init();

    for (int i = 0; i < GIC_NR_PRIVATE_IRQS; i++)
        gic_enable_int(i, 0);

    gic_test_init();

    logger("    gich enable %s\n", read32((void *)GICH_HCR) ? "ok" : "error");
}

// ===========================================
// 下面是一些get、set函数。 读取寄存器值
// ===========================================

uint32_t gic_get_typer(void)
{
    return read32((void *)GICD_TYPER);
}

uint32_t gic_get_iidr(void)
{
    return read32((void *)GICD_IIDR);
}

uint32_t gic_read_iar(void)
{
    return read32((void *)GICC_IAR);
}

uint32_t gic_iar_irqnr(uint32_t iar)
{
    return iar & GICC_IAR_INT_ID_MASK;
}

void gic_write_eoir(uint32_t irqstat)
{
    write32(irqstat, (void *)GICC_EOIR);
}

void gic_write_dir(uint32_t irqstat)
{
    write32(irqstat, (void *)GICC_DIR);
}

// 发送给特定的核（某个核）
void gic_ipi_send_single(int irq, int cpu)
{
    // assert(cpu < 8);
    // assert(irq < 16);
    write32(1 << (cpu + 16) | irq, (void *)GICD_SGIR);
}

// The number of implemented CPU interfaces.
uint32_t cpu_num(void)
{
    return GICD_TYPER_CPU_NUM(read32((void *)GICD_TYPER));
}

// Enables the given interrupt.
// W1S	Write-1-to-Set	写入 1 会**置位（set）**对应位，写 0 不影响
// W1C	Write-1-to-Clear	写入 1 会**清零（clear）**对应位，写 0 不影响
void gic_enable_int(int vector, int enabled)
{
    int reg = vector >> 5;                     //  vec / 32
    int mask = 1 << (vector & ((1 << 5) - 1)); //  vec % 32
    if (enabled)
        write32(mask, (void *)GICD_ISENABLER(reg));
    else
        write32(mask, (void *)GICD_ICENABLER(reg));
    logger("set enable: reg: %d, mask: 0x%llx\n", reg, mask);
}

// Check the given interrupt.
int gic_get_enable(int vector)
{
    int reg = vector >> 5;                     //  vec / 32
    int mask = 1 << (vector & ((1 << 5) - 1)); //  vec % 32
    uint32_t val = read32((void *)GICD_ISENABLER(reg));

    logger("get enable: reg: %llx, mask: %llx, value: %llx\n", reg, mask, val);
    return val & mask != 0;
}

// An auxiliary function for determining whether it is SGI, returning 1 indicates it is SGI and 0 indicates it is not
static int gic_is_sgi(int int_id) {
    return int_id >= 0 && int_id <= 15;  // SGI 一般是0-15号中断
}

// Set the Active status of the interrupt. act=1 activates, act=0 clears the activation
void gic_set_active(int int_id, int act)
{
    int reg = int_id / 32;
    int mask = 1 << (int_id % 32);

    if (act) {
        write32(mask, (void *)GICD_ISACTIVER(reg));
    } else {
        write32(mask, (void *)GICD_ICACTIVER(reg));
    }
    logger("set active: reg: %d, mask: 0x%x, act: %d\n", reg, mask, act);
}

// Set the Pending status of the interrupt. pend=1 sets Pending, pend=0 clears Pending
void gic_set_pending(int int_id, int pend, int target_cpu)
{
    if (gic_is_sgi(int_id)) {
        int reg = int_id / 4;             // 每个 GICD_SPENDSGIR 管 4 个 SGI
        int off = (int_id % 4) * 8;       // 每个 SGI 占 8 bit（一个字节，每个 bit 表示一个 CPU）

        if (target_cpu < 0 || target_cpu >= 8) {
            logger("Invalid CPU ID: %d\n", target_cpu);
            return;
        }

        if (pend) {
            // 设置指定 CPU 的 SGI pending 位
            write32(1 << (off + target_cpu), (void *)GICD_SPENDSGIR(reg));
        } else {
            // 清除该 SGI 在所有 CPU 上的 pending（你也可以只清除指定 CPU 的位）
            write32(1 << (off + target_cpu), (void *)GICD_CPENDSGIR(reg));
        }

        logger("set SGI pending: int_id: %d, reg: %d, off: %d, cpu: %d, pend: %d\n", int_id, reg, off, target_cpu, pend);
    } else {
        int reg = int_id / 32;
        int mask = 1 << (int_id % 32);

        if (pend) {
            write32(mask, (void *)GICD_ISPENDER(reg));
        } else {
            write32(mask, (void *)GICD_ICPENDER(reg));
        }

        logger("set pending: int_id: %d, reg: %d, mask: 0x%x, pend: %d\n", int_id, reg, mask, pend);
    }
}


// Set the interrupt priority
void gic_set_ipriority(uint32_t vector, uint32_t pri)
{
    uint32_t n = vector >> 2;        // 哪个寄存器
    uint32_t m = vector & 3;         // 寄存器内的哪个字节
    uint64_t addr = GICD_IPRIORITYR(n);
    uint32_t val = read32((void *)(uint64_t)addr);

    // 设置第 m 字节的优先级
    uint8_t priority = (pri << 3) | (1 << 7);  // GICv2: [7]=1 表示 Group1 (非 secure)
    val &= ~(0xFF << (8 * m));
    val |= (priority << (8 * m));

    write32(val, (void *)(uint64_t)addr);
    logger("set priority: n: %u, m: %u, pri: %u\n", n, m, pri);
}

// Get the interrupt priority
int gic_get_ipriority(int vector) {
    int n = vector >> 2;                      // 每个 IPRIORITYR 寄存器存储 4 个中断（每个占 8 bit）
    int m = vector & 0x3;                     // 当前中断在该寄存器中的偏移（0~3）
    uint32_t reg_val = read32((void *)GICD_IPRIORITYR(n));  // 读取整个 32-bit 寄存器
    int priority = (reg_val >> (m * 8)) & 0xFF;              // 提取目标中断的 8-bit 优先级
    return priority;
}

// Get the target CPU for a specific interrupt
int gic_get_target(int int_id) {
    int idx = (int_id * 8) / 32;
    int off = (int_id * 8) % 32;
    uint32_t val = read32((void *)(GICD_ITARGETSR(idx)));
    return (val >> off) & 0xFF;
}

// Set the target CPU for a specific interrupt
void gic_set_target(int int_id, uint8_t target) {
    int idx = (int_id * 8) / 32;
    int off = (int_id * 8) % 32;
    uint32_t mask = 0xFF << off;

    uint32_t old_val = read32((void *)(GICD_ITARGETSR(idx)));
    uint32_t new_val = (old_val & ~mask) | ((target << off) & mask);
    write32(new_val, (void *)(GICD_ITARGETSR(idx)));
}

// Set the interrupt configuration (edge/level)
void gic_set_icfgr(uint32_t int_id, uint8_t cfg)
{
    uint32_t reg_index = (int_id * 2) / 32;
    uint32_t bit_offset = (int_id * 2) % 32;
    uint32_t mask = 0b11 << bit_offset;

    volatile uint32_t *reg = (volatile uint32_t *)GICD_ICFGR(reg_index);
    uint32_t val = *reg;

    val = (val & ~mask) | (((uint32_t)(cfg & 0x3) << bit_offset) & mask);
    *reg = val;
}

uint32_t gic_make_virtual_hardware_interrupt(uint32_t vector, uint32_t pintvec, int pri, bool grp1)
{
    uint32_t mask = 0x90000000; // grp0 hw pending
    mask |= ((uint32_t)(pri & 0xf8) << 20) | (vector & (0x1ff)) | ((pintvec & 0x1ff) << 10) | ((uint32_t)grp1 << 30);
    return mask;
}

uint32_t gic_make_virtual_software_interrupt(uint32_t vector, int pri, bool grp1)
{
    uint32_t mask = 0x10000000; // grp0  pending
    mask |= ((uint32_t)(pri & 0xf8) << 20) | (vector & (0x1ff)) | ((uint32_t)grp1 << 30);
    return mask;
}

uint32_t gic_make_virtual_software_sgi(uint32_t vector, int cpu_id, int pri, bool grp1)
{
    uint32_t mask = 0x10000000; // grp0  pending
    mask |= ((uint32_t)(pri & 0xf8) << 20) | (vector & (0x1ff)) | ((uint32_t)grp1 << 30) | ((uint32_t)cpu_id << 10);
    return mask;
}

// ===================================
// 下面是关于 GICH 的函数
// ===================================

uint32_t gic_read_lr(int n)
{
    return read32((void *)GICH_LR(n));
}

int gic_lr_read_pri(uint32_t lr_value)
{
    return (lr_value & (0xf8 << 20)) >> 20;
}

uint32_t gic_lr_read_vid(uint32_t lr_value)
{
    return lr_value & 0x1ff;
}

uint32_t gic_apr()
{
    return read32((void *)GICH_APR);
}

uint32_t gic_elsr0()
{
    return read32((void *)GICH_ELSR0);
}

uint32_t gic_elsr1()
{
    return read32((void *)GICH_ELSR1);
}

void gic_write_lr(int n, uint32_t mask)
{
    write32(mask, (void *)GICH_LR(n));
}

// 启用非优先级中断，这种中断允许在虚拟化环境下处理一些低优先级的中断。
void gic_set_np_int(void)
{
    write32(read32((void *)GICH_HCR) | (1 << 3), (void *)GICH_HCR);
}

// 禁用非优先级中断，确保虚拟机只处理优先级较高的中断。
void gic_clear_np_int(void)
{
    write32(read32((void *)GICH_HCR) & ~(1 << 3), (void *)GICH_HCR);
}
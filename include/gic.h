

#ifndef __GIC_H__
#define __GIC_H__

#include "aj_types.h"

#define GIC_NR_PRIVATE_IRQS 32
#define GIC_FIRST_SPI GIC_NR_PRIVATE_IRQS

#define SPI_ID_MAX 512

#include "os_cfg.h"

/* Distributor registers */
#define GICD_CTLR (GICD_BASE_ADDR + 0x000)
#define GICD_TYPER (GICD_BASE_ADDR + 0x004)
#define GICD_IIDR (GICD_BASE_ADDR + 0x008)
#define GICD_IGROUPR(x) (GICD_BASE_ADDR + (0x080 + 0x004 * (x)))
#define GICD_ISENABLER(x) (GICD_BASE_ADDR + (0x100 + 0x004 * (x)))
#define GICD_ICENABLER(x) (GICD_BASE_ADDR + (0x180 + 0x004 * (x)))
#define GICD_ISPENDER(x) (GICD_BASE_ADDR + (0x200 + 0x004 * (x)))
#define GICD_ICPENDER(x) (GICD_BASE_ADDR + (0x280 + 0x004 * (x)))
#define GICD_ISACTIVER(x) (GICD_BASE_ADDR + (0x300 + 0x004 * (x)))
#define GICD_ICACTIVER(x) (GICD_BASE_ADDR + (0x380 + 0x004 * (x)))
#define GICD_IPRIORITYR(x) (GICD_BASE_ADDR + (0x400 + 0x004 * (x)))
#define GICD_ITARGETSR(x) (GICD_BASE_ADDR + (0x800 + 0x004 * (x)))
#define GICD_ICFGR(x) (GICD_BASE_ADDR + (0xc00 + 0x004 * (x)))
#define GICD_PPISR (GICD_BASE_ADDR + 0xd00)
#define GICD_SPISR(x) (GICD_BASE_ADDR + (0xd04 + 0x004 * (x)))
#define GICD_NSACR(x) (GICD_BASE_ADDR + (0xe00 + 0x004 * (x)))
#define GICD_SGIR (GICD_BASE_ADDR + 0xf00)
#define GICD_CPENDSGIR(x) (GICD_BASE_ADDR + (0xf10 + 0x004 * (x)))
#define GICD_SPENDSGIR(x) (GICD_BASE_ADDR + (0xf20 + 0x004 * (x)))

#define GICD_TYPER_IRQS(typer) ((((typer) & 0x1f) + 1) * 32)
#define GICD_TYPER_CPU_NUM(typer) ((((typer) >> 5) & 0b111) + 1)

#define GICD_INT_EN_SET_SGI 0x0000ffff
#define GICD_INT_DEF_PRI_X4 0xa0a0a0a0

/* CPU interface registers */
#define GICC_CTLR (GICC_BASE_ADDR + 0x0000)
#define GICC_PMR (GICC_BASE_ADDR + 0x0004)
#define GICC_BPR (GICC_BASE_ADDR + 0x0008)
#define GICC_IAR (GICC_BASE_ADDR + 0x000c)
#define GICC_EOIR (GICC_BASE_ADDR + 0x0010)
#define GICC_RPR (GICC_BASE_ADDR + 0x0014)
#define GICC_HPPIR (GICC_BASE_ADDR + 0x0018)
#define GICC_ABPR (GICC_BASE_ADDR + 0x001c)
#define GICC_AIAR (GICC_BASE_ADDR + 0x0020)
#define GICC_AEOIR (GICC_BASE_ADDR + 0x0024)
#define GICC_AHPPIR (GICC_BASE_ADDR + 0x0028)
#define GICC_APR(x) (GICC_BASE_ADDR + (0x00d0 + 0x0004 * (x)))
#define GICC_NSAPR(x) (GICC_BASE_ADDR + (0x00e0 + 0x0004 * (x)))
#define GICC_IIDR (GICC_BASE_ADDR + 0x00fc)
#define GICC_DIR (GICC_BASE_ADDR + 0x1000)

#define GICC_INT_PRI_THRESHOLD 0xf0
#define GICC_INT_SPURIOUS 0x3ff

#define GICH_HCR (GICH_BASE_ADDR + 0x0000)
#define GICH_VTR (GICH_BASE_ADDR + 0x0004)
#define GICH_VMCR (GICH_BASE_ADDR + 0x0008)
#define GICH_MISR (GICH_BASE_ADDR + 0x0010)
#define GICH_EISR0 (GICH_BASE_ADDR + 0x0020)
#define GICH_EISR1 (GICH_BASE_ADDR + 0x0024)
#define GICH_ELSR0 (GICH_BASE_ADDR + 0x0030)
#define GICH_ELSR1 (GICH_BASE_ADDR + 0x0034)
#define GICH_APR (GICH_BASE_ADDR + 0x00f0)
#define GICH_LR(x) (GICH_BASE_ADDR + 0x0100 + 0x4 * (x))
#define GICH_LR_NUM 4
#define GICH_LR_PID_SHIFT 10

/*  GICD 操作掩码 */

#define GICD_CTRL_ENABLE_GROUP0 (1 << 0)	// 启用组0中断
#define GICD_CTRL_ENABLE_GROUP1 (1 << 1)	// 启用组1中断
#define GICD_CTRL_ENABLE_GROUP1_NS (1 << 2) // 启用非安全组1中断
#define GICD_CTRL_DS (1 << 31)				// 区分安全和非安全状态行为

/* GICC 操作掩码 */

#define GICC_CTRL_ENABLE_GROUP0 (1 << 0) // 启用GIC CPU接口
#define GICC_CTRL_ENABLE_GROUP1 (1 << 1) // 启用GIC CPU接口

#define GICC_CTRL_FIQEN (1 << 1)	  // 启用快速中断（FIQ）模式
#define GICC_CTRL_ACKCTL (1 << 2)	  // 控制中断确认机制
#define GICC_CTRL_SBPR (1 << 3)		  // 启用安全中断的优先级分离
#define GICC_CTRL_EOIMODE (1 << 4)	  // 控制中断结束模式
#define GICC_CTRL_EOIMODENS (1 << 31) // 控制非安全状态的结束中断处理模式

#define GICC_IAR_INT_ID_MASK 0x3ff

typedef struct gic_t
{
	unsigned int irq_nr;
} gic_t;

typedef enum
{
	EDGE = 1,
	LEVEL = 0,
} trigger_mode_t;

extern struct gic_t _gicv2;

#define gicv2_dist_base() (_gicv2.dist_base)
#define gicv2_cpu_base() (_gicv2.cpu_base)

static inline uint32_t get_daif()
{
	uint32_t value;
	asm volatile("mrs %0, daif"
				 : "=r"(value)
				 : /* no input*/
				 :);
	return value;
}

static inline void enable_interrupts(void)
{
	__asm__ __volatile__("msr daifclr, #2" : : : "memory");
}

static inline void disable_interrupts(void)
{
	__asm__ __volatile__("msr daifset, #2" : : : "memory");
}

void gic_init();
void gicc_init();
void gic_virtual_init(void);
void gic_test_init();

uint32_t gic_read_iar(void);
uint32_t gic_iar_irqnr(uint32_t iar);

void gic_write_eoir(uint32_t irqstat);
void gic_write_dir(uint32_t irqstat);
void gic_ipi_send_single(int irq, int cpu);
void gic_enable_int(int vector, int enable);
void gic_disable_int(int vector);
int gic_get_enable(int vector);
uint32_t cpu_num();

void gic_set_isenabler(uint32_t n, uint32_t value);
void gic_set_ipriority(uint32_t n, uint32_t value);
void gic_set_icenabler(uint32_t n, uint32_t value);

uint32_t gic_make_virtual_hardware_interrupt(uint32_t vector, uint32_t pintvec, int pri, bool grp1);
uint32_t gic_make_virtual_software_interrupt(uint32_t vector, int pri, bool grp1);
uint32_t gic_make_virtual_software_sgi(uint32_t vector, int cpu_id, int pri, bool grp1);

uint32_t gic_read_lr(int n);
int gic_lr_read_pri(uint32_t lr_value);
uint32_t gic_lr_read_vid(uint32_t lr_value);
void gic_write_lr(int n, uint32_t mask);
void gic_set_np_int(void);
void gic_clear_np_int(void);

uint32_t gic_apr();
uint32_t gic_elsr0();
uint32_t gic_elsr1();

#endif // __GIC_H__
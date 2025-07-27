

/*     目前只准备实现 vgic    */

#include <hyper/vgic.h>
#include <aj_types.h>
#include <hyper/vm.h>
#include <exception.h>
#include <hyper/hyper_cfg.h>
#include <io.h>
#include <mem/barrier.h>

#define HIGHEST_BIT_POSITION(x)        \
    ({                                 \
        unsigned int _i = 0;           \
        unsigned long long _val = (x); \
        while (_val >>= 1)             \
        {                              \
            _i++;                      \
        }                              \
        _i;                            \
    })

static struct vgic_t _vgic[VM_NUM_MAX];
static uint32_t _vgic_num = 0;

vgic_core_state_t _state[VCPU_NUM_MAX];
static uint32_t _state_num = 0;


struct vgic_t *get_vgic(uint8_t id)
{
    return &_vgic[id];
}

// 建立 vint 和 pint 的映射关系
void virtual_gic_register_int(struct vgic_t *vgic, uint32_t pintvec, uint32_t vintvec)
{
    vgic->ptov[pintvec] = vintvec;
    vgic->vtop[vintvec] = pintvec;
    // vgic->use_irq[pintvec/32] |= 1 << (pintvec % 32);
}

void vgicd_write(ept_violation_info_t *info, trap_frame_t *el2_ctx, void *paddr)
{
    unsigned long reg_num;
    volatile uint64_t *r;
    volatile void *buf;
    volatile unsigned long len;
    volatile unsigned long *dst;

    // 获取寄存器编号和 MMIO 操作的大小
    reg_num = info->hsr.dabt.reg;
    len = 1 << (info->hsr.dabt.size & 0x00000003);

    // 计算目标缓冲区
    r = &el2_ctx->r[reg_num];
    buf = (void *)r;

    // 从 MMIO 地址读取数据
    dst = (unsigned long *)(unsigned long)paddr;
    logger("(%d bytes) 0x%llx  R%d\n", (unsigned long)len, *dst, (unsigned long)reg_num);

    logger("old data: 0x%llx\n", *dst);
    // 将数据写入寄存器或进行其他必要的操作
    if (reg_num != 30)
    {
        *dst = *(unsigned long *)buf;
    }
    // 确保所有更改都能被看到
    dsb(sy);
    isb();
    logger("new data: 0x%llx\n", *dst);
}

void vgicd_read(ept_violation_info_t *info, trap_frame_t *el2_ctx, void *paddr)
{
    unsigned long reg_num;
    volatile uint64_t *r;
    volatile void *buf;
    volatile unsigned long *src;
    volatile unsigned long len;
    volatile unsigned long dat;

    reg_num = info->hsr.dabt.reg;
    len = 1 << (info->hsr.dabt.size & 0x3);
    
    r = &el2_ctx->r[reg_num];
    buf = (void *)r;

    src = (unsigned long *)(unsigned long)paddr;
    dat = *src;

    if (reg_num != 30)
    {
        *(unsigned long *)buf = dat;
    }
    dsb(sy);
    isb();
}

// handle gicd emu
void intc_handler(ept_violation_info_t *info, trap_frame_t *el2_ctx)
{
    struct vgic_t *vgic = get_vgic(0);
    paddr_t gpa = info->gpa;
    if (GICD_BASE_ADDR <= gpa && gpa < (GICD_BASE_ADDR + 0x0010000))
    {
        if (info->hsr.dabt.write)
        { // 寄存器写到内存
            if (gpa == GICD_CTLR)
            {
                int reg_num = info->hsr.dabt.reg;
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);
                vgic->gicd_ctlr = r;
                logger_info("      <<< gicd emu write GICD_CTLR: ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d\n", gpa, r, len);
            }
            /*  is enable reg*/
            else if (gpa == GICD_ISENABLER(0))
            {
                int reg_num = info->hsr.dabt.reg;
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);
                
                gic_enable_int(HIGHEST_BIT_POSITION(r), 1);
                logger_info("      <<< gicd emu write GICD_ISENABLER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int id: %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r));
            }
            else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0))
            {
                int reg_num = info->hsr.dabt.reg;
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);
                int id = ((gpa - GICD_ISENABLER(0)) / 0x4) * 32;
                
                gic_enable_int(HIGHEST_BIT_POSITION(r) + id, 1);
                logger_info("      <<< gicd emu write GICD_ISENABLER(i): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int id: %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r) + id);
            }
            /* ic enable reg*/
            else if (gpa == GICD_ICENABLER(0))
            {
                int reg_num = info->hsr.dabt.reg;
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);
                
                gic_enable_int(HIGHEST_BIT_POSITION(r), 0);
                logger_info("      <<< gicd emu write GICD_ICENABLER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int id: %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r));
            }
            else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0))
            {
                int reg_num = info->hsr.dabt.reg;
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);
                int id = ((gpa - GICD_ICENABLER(0)) / 0x4) * 32;

                gic_enable_int(HIGHEST_BIT_POSITION(r) + id, 0);
                logger_info("      <<< gicd emu write GICD_ICENABLER(i): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int id: %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r) + id);
            }
            /* is pend reg*/
            else if (gpa == GICD_ISPENDER(0))
            {
                logger_info("      <<< gicd emu write GICD_ISPENDER(0)\n");
            }
            else if (GICD_ISPENDER(1) <= gpa && gpa < GICD_ICPENDER(0))
            {
                logger_info("      <<< gicd emu write GICD_ISPENDER(i)\n");
            }
            /* ic pend reg*/
            else if (gpa == GICD_ICPENDER(0))
            {   
                int reg_num = info->hsr.dabt.reg;
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);
                
                gic_set_pending(HIGHEST_BIT_POSITION(r), 0, 0);
                logger_info("      <<< gicd emu write GICD_ICPENDER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int id: %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r));
            }
            else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_ISACTIVER(0))
            {
                int reg_num = info->hsr.dabt.reg;
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);
                int id = ((gpa - GICD_ICPENDER(0)) / 0x4) * 32;

                gic_set_pending(HIGHEST_BIT_POSITION(r) + id, 0, 0);
                logger_info("      <<< gicd emu write GICD_ICPENDER(i): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int id: %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r) + id);
            }
            /* I priority reg*/
            else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4))
            {
                // SGI + PPI priority write
                int reg_num = info->hsr.dabt.reg;
                int len = 1 << (info->hsr.dabt.size & 0x3);
                uint32_t val = el2_ctx->r[reg_num];

                int offset = gpa - GICD_IPRIORITYR(0);
                int int_id = offset;  // 每字节一个中断，直接用 offset

                for (int i = 0; i < len; ++i) {
                    uint32_t vector = int_id + i;
                    uint8_t pri_raw = (val >> (8 * i)) & 0xFF;
                    uint32_t pri = pri_raw >> 3;  // 还原 priority 值（只保留高 5 位）

                    gic_set_ipriority(vector, pri);
                }

                logger_info("      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi): ");
                logger("int_id=%d, len=%d, val=0x%llx\n", int_id, len, val);
            }
            else if (GICD_IPRIORITYR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_IPRIORITYR(SPI_ID_MAX / 4))
            {
                // SPI priority write
                int reg_num = info->hsr.dabt.reg;
                int len = 1 << (info->hsr.dabt.size & 0x3);
                uint32_t val = el2_ctx->r[reg_num];

                int offset = gpa - GICD_IPRIORITYR(0);
                int int_id = offset;

                for (int i = 0; i < len; ++i) {
                    uint32_t vector = int_id + i;
                    uint8_t pri_raw = (val >> (8 * i)) & 0xFF;
                    uint32_t pri = pri_raw >> 3;

                    gic_set_ipriority(vector, pri);
                }

                logger_info("      <<< gicd emu write GICD_IPRIORITYR(spi):");
                logger("int_id=%d, len=%d, val=0x%llx\n", int_id, len, val);
            }

            /* I cfg reg*/
            else if (GICD_ICFGR(GIC_FIRST_SPI / 16) <= gpa && gpa < GICD_ICFGR(SPI_ID_MAX / 16))
            {
                logger_info("      <<< gicd emu write GICD_ICFGR(i)\n");
            }
            /* I target reg*/
            else if (GICD_ITARGETSR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_ITARGETSR(SPI_ID_MAX / 4))
            {
                logger_info("      <<< gicd emu write GICD_ITARGETSR(i)\n");
            }
            /* sgi reg*/
            else if (gpa == GICD_SGIR) // wo
            {
                logger_info("      <<< gicd emu write GICD_SGIR(i)\n");
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
                vgicd_read(info, el2_ctx, &typer);
                
                logger_warn("      >>> gicd emu read GICD_TYPER: ");
                logger("typer: 0x%x\n", typer);
            }
            else if (gpa == GICD_IIDR) // ro
            {
                uint32_t iidr = gic_get_iidr();
                vgicd_read(info, el2_ctx, &iidr);
                logger_warn("      >>> gicd emu read GICD_IIDR: ");
                logger("iidr: 0x%x\n", iidr);
            }

            /*  is enable reg*/
            else if (gpa == GICD_ISENABLER(0))
            {
                logger_warn("      >>> gicd emu read GICD_ISENABLER(0)\n");
            }
            else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0))
            {
                logger_warn("      >>> gicd emu read GICD_ISENABLER(i)\n");
            }
            /* ic enable reg*/
            else if (gpa == GICD_ICENABLER(0))
            {
                logger_warn("      >>> gicd emu read GICD_ICENABLER(0)\n");
            }
            else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0))
            {
                logger_warn("      >>> gicd emu read GICD_ICENABLER(i)\n");
            }
            /* is pend reg*/
            else if (gpa == GICD_ISPENDER(0))
            {
                logger_warn("      >>> gicd emu read GICD_ISPENDER(0)\n");
            }
            else if (GICD_ISPENDER(1) <= gpa && gpa < GICD_ICPENDER(0))
            {
                logger_warn("      >>> gicd emu read GICD_ISPENDER(i)\n");
            }
            /* ic pend reg*/
            else if (gpa == GICD_ICPENDER(0))
            {
                logger_warn("      >>> gicd emu read GICD_ICPENDER(0)\n");
            }
            else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_IPRIORITYR(0))
            {
                logger_warn("      >>> gicd emu read GICD_ICPENDER(i)\n");
            }
            /* I priority reg*/
            else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4))
            {
                logger_warn("      >>> gicd emu read GICD_IPRIORITYR(sgi-ppi)\n");
            }
            else if (GICD_IPRIORITYR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_IPRIORITYR(SPI_ID_MAX / 4))
            {
                logger_warn("      >>> gicd emu read GICD_IPRIORITYR(spi)\n");
            }
            /* I cfg reg*/
            else if (GICD_ICFGR(GIC_FIRST_SPI / 16) <= gpa && gpa < GICD_ICFGR(SPI_ID_MAX / 16))
            {
                logger_warn("      >>> gicd emu read GICD_ICFGR(i)\n");
            }
            /* I target reg*/
            else if (GICD_ITARGETSR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_ITARGETSR(SPI_ID_MAX / 4))
            {
                logger_warn("      >>> gicd emu read GICD_ITARGETSR(i)\n");
            }
        }
        return;
    }

    logger_warn("[intc_handler]: unsupported access gpa: ");
    logger("0x%llx\n", gpa);
}

// vgic inject
void vgic_inject(uint32_t vector)
{
    // logger("vgic inject vector: %d\n", vector);
    uint32_t mask = gic_make_virtual_hardware_interrupt(vector, vector, 0, 0);

    uint32_t elsr0 = gic_elsr0();
    uint32_t elsr1 = gic_elsr1();
    uint64_t elsr = ((uint64_t)elsr1 << 32) | elsr0;

    uint32_t is_active = gic_apr();
    uint32_t pri = gic_lr_read_pri(mask);
    uint32_t irq_no = gic_lr_read_vid(mask);
    int freelr = -1;

    for (int i = 0; i < 4; i++)
    {
        if ((elsr >> i) & 0x1)
        {
            if (freelr < 0)
                freelr = i;

            continue;
        }
        if (((gic_read_lr(i) >> GICH_LR_PID_SHIFT) & 0x3ff) == vector)
        {
            logger("vgic inject, vector %d already in lr%d\n", vector, i);
            return; // busy
        }
    }

    // logger("is_empty: 0x%llx, is_active: 0x%llx, pri: 0x%llx, irq_no: %d\n", elsr, is_active, pri, irq_no);
    gic_write_lr(freelr, mask);
}


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
static uint8_t _vgic_num;

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
    printf("(%d bytes) 0x%x  R%d\n", (unsigned long)len, *dst, (unsigned long)reg_num);

    printf("old data: 0x%x\n", *dst);
    // 将数据写入寄存器或进行其他必要的操作
    if (reg_num != 30)
    {
        *dst = *(unsigned long *)buf;
    }
    // 确保所有更改都能被看到
    dsb(sy);
    isb();
    printf("new data: 0x%x\n", *dst);
}

void vgicd_read(ept_violation_info_t *info, trap_frame_t *el2_ctx, void *paddr)
{
    unsigned long reg_num;
    volatile uint64_t *r;
    volatile void *buf;
    volatile unsigned long *src;
    volatile unsigned long len;
    volatile unsigned long dat;
    // spin_lock(&vcpu.lock);
 
    reg_num = info->hsr.dabt.reg;
    // r = (uint64_t *)select_user_reg(reg_num);
    // r = &vcpu.pctx->r[reg_num];
    r = &el2_ctx->r[reg_num];
    len = 1 << (info->hsr.dabt.size & 0x00000003);
    buf = (void *)r;

    src = (unsigned long *)(unsigned long)paddr;
    dat = *src;
    printf("(%d bytes) 0x%x R%d\n", (unsigned long)len, *src, (unsigned long)reg_num);

    printf("old data: 0x%x\n", *r);
    if (reg_num != 30)
    {
        *(unsigned long *)buf = dat;
    }
    dsb(sy);
    isb();
    printf("new data: 0x%x\n", *r);

    // spin_unlock(&vcpu.lock);
}

// handle gicd emu
void intc_handler(ept_violation_info_t *info, trap_frame_t *el2_ctx)
{
    struct vgic_t *vgic = get_vgic(0);
    paddr_t gpa = info->gpa;
    if (GICD_BASE_ADDR <= gpa && gpa <= (GICD_BASE_ADDR + 0x0010000))
    {
        if (info->hsr.dabt.write)
        { // 寄存器写到内存
            if (gpa == GICD_CTLR)
            {
                print_info("      <<< gicd emu write GICD_CTLR\n");
            }
            /*  is enable reg*/
            else if (gpa == GICD_ISENABLER(0))
            {
                int reg_num = info->hsr.dabt.reg;
                // r = (uint64_t *)select_user_reg(reg_num);
                // r = &vcpu.pctx->r[reg_num];
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);
                printf("gpa: %x, r: %x, len: %d, int id: %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r));

                // 给它最高优先级
                gic_enable_int(HIGHEST_BIT_POSITION(r), 0);
                print_info("      <<< gicd emu write GICD_ISENABLER(0)\n");
            }
            else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0))
            {
                int reg_num = info->hsr.dabt.reg;
                // r = (uint64_t *)select_user_reg(reg_num);
                // r = &vcpu.pctx->r[reg_num];
                int r = el2_ctx->r[reg_num];
                int len = 1 << (info->hsr.dabt.size & 0x00000003);

                int id = ((gpa - GICD_ISENABLER(0)) / 0x4) * 32;
                printf("gpa: %x, r: %x, len: %d, int id: %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r) + id);

                // 给它最高优先级
                gic_enable_int(HIGHEST_BIT_POSITION(r) + id, 0);

                print_info("      <<< gicd emu write GICD_ISENABLER(i)\n");
            }
            /* ic enable reg*/
            else if (gpa == GICD_ICENABLER(0))
            {
                print_info("      <<< gicd emu write GICD_ICENABLER(0)\n");
            }
            else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0))
            {
                print_info("      <<< gicd emu write GICD_ICENABLER(i)\n");
            }
            /* is pend reg*/
            else if (gpa == GICD_ISPENDER(0))
            {
                print_info("      <<< gicd emu write GICD_ISPENDER(0)\n");
            }
            else if (GICD_ISPENDER(1) <= gpa && gpa < GICD_ICPENDER(0))
            {
                print_info("      <<< gicd emu write GICD_ISPENDER(i)\n");
            }
            /* ic pend reg*/
            else if (gpa == GICD_ICPENDER(0))
            {
                print_info("      <<< gicd emu write GICD_ICPENDER(0)\n");
            }
            else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_IPRIORITYR(0))
            {
                print_info("      <<< gicd emu write GICD_ICPENDER(i)\n");
            }
            /* I priority reg*/
            else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4))
            {
                print_info("      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)\n");
            }
            else if (GICD_IPRIORITYR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_IPRIORITYR(SPI_ID_MAX / 4))
            {
                print_info("      <<< gicd emu write GICD_IPRIORITYR(spi)\n");
            }
            /* I cfg reg*/
            else if (GICD_ICFGR(GIC_FIRST_SPI / 16) <= gpa && gpa < GICD_ICFGR(SPI_ID_MAX / 16))
            {
                print_info("      <<< gicd emu write GICD_ICFGR(i)\n");
            }
            /* I target reg*/
            else if (GICD_ITARGETSR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_ITARGETSR(SPI_ID_MAX / 4))
            {
                print_info("      <<< gicd emu write GICD_ITARGETSR(i)\n");
            }
            /* sgi reg*/
            else if (gpa == GICD_SGIR) // wo
            {
                print_info("      <<< gicd emu write GICD_SGIR(i)\n");
            }
            else
            {
                print_info("# unsupported access\n");
                printf("gpa: 0x%x\n", gpa);
            }
        }
        else
        { // 内存写到寄存器
            if (gpa == GICD_CTLR)
            {
                print_warn("      >>> gicd emu read GICD_CTLR\n");
            }
            else if (gpa == GICD_TYPER) // ro
            {
                print_warn("      >>> gicd emu read GICD_TYPER\n");
            }
            else if (gpa == GICD_IIDR) // ro
            {
                print_warn("      >>> gicd emu read GICD_IIDR\n");
            }

            /*  is enable reg*/
            else if (gpa == GICD_ISENABLER(0))
            {
                print_warn("      >>> gicd emu read GICD_ISENABLER(0)\n");
            }
            else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0))
            {
                print_warn("      >>> gicd emu read GICD_ISENABLER(i)\n");
            }
            /* ic enable reg*/
            else if (gpa == GICD_ICENABLER(0))
            {
                print_warn("      >>> gicd emu read GICD_ICENABLER(0)\n");
            }
            else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0))
            {
                print_warn("      >>> gicd emu read GICD_ICENABLER(i)\n");
            }
            /* is pend reg*/
            else if (gpa == GICD_ISPENDER(0))
            {
                print_warn("      >>> gicd emu read GICD_ISPENDER(0)\n");
            }
            else if (GICD_ISPENDER(1) <= gpa && gpa < GICD_ICPENDER(0))
            {
                print_warn("      >>> gicd emu read GICD_ISPENDER(i)\n");
            }
            /* ic pend reg*/
            else if (gpa == GICD_ICPENDER(0))
            {
                print_warn("      >>> gicd emu read GICD_ICPENDER(0)\n");
            }
            else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_IPRIORITYR(0))
            {
                print_warn("      >>> gicd emu read GICD_ICPENDER(i)\n");
            }
            /* I priority reg*/
            else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4))
            {
                print_warn("      >>> gicd emu read GICD_IPRIORITYR(sgi-ppi)\n");
            }
            else if (GICD_IPRIORITYR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_IPRIORITYR(SPI_ID_MAX / 4))
            {
                print_warn("      >>> gicd emu read GICD_IPRIORITYR(spi)\n");
            }
            /* I cfg reg*/
            else if (GICD_ICFGR(GIC_FIRST_SPI / 16) <= gpa && gpa < GICD_ICFGR(SPI_ID_MAX / 16))
            {
                print_warn("      >>> gicd emu read GICD_ICFGR(i)\n");
            }
            /* I target reg*/
            else if (GICD_ITARGETSR(GIC_FIRST_SPI / 4) <= gpa && gpa < GICD_ITARGETSR(SPI_ID_MAX / 4))
            {
                print_warn("      >>> gicd emu read GICD_ITARGETSR(i)\n");
            }
        }
        return;
    }

    print_info("emu gicd error\n");
}

// vgic inject
void vgic_inject(uint32_t vector)
{

    // printf("vgic inject vector: %d\n", vector);
    uint32_t mask = gic_make_virtual_hardware_interrupt(vector, vector, 0, 0);

    // 降低优先级，host将收不到timer
    gic_set_ipriority(6, 0xf8000000);
    gic_set_ipriority(8, 0x0000f800);

    uint32_t is_empty = gic_elsr0();
    uint32_t is_active = gic_apr();
    uint32_t pri = gic_lr_read_pri(mask);
    uint32_t irq_no = gic_lr_read_vid(mask);

    // printf("is_empty: 0x%x, is_active: 0x%x, pri: 0x%x, irq_no: %d\n", is_empty, is_active, pri, irq_no);

    for (int i = 0; i < GICH_LR_NUM; i++)
    {
        if (is_empty & (1 << i))
        {
            gic_write_lr(i, mask);
            return;
        }
        uint32_t prev_lr = gic_read_lr(i);
        uint8_t prev_pri = gic_lr_read_pri(prev_lr);

        if (prev_pri > pri)
        {
            gic_write_lr(i, mask);
            mask = prev_lr;
            pri = prev_pri;
            printf("!!! premeption !!!\n");
        }
    }
    // gic_write_lr(0, mask);

    // gic_disable_int(27);
    // gic_set_np_int();
}
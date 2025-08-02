

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

static struct vgic_t _vgic[VM_NUM_MAX];
static uint32_t _vgic_num = 0;

vgic_core_state_t _state[VCPU_NUM_MAX];
static uint32_t _state_num = 0;

void vgic_inject_sgi(tcb_t *task, uint32_t int_id);
void vgic_try_inject_pending(tcb_t *task);

vgic_core_state_t *get_vgicc_by_vcpu(tcb_t *task); // if task==NULL, return current task's core state structure
int32_t get_vcpuid(tcb_t *task);                   // if task==NULL, return current task's vcpu id
list_t *get_vcpus(tcb_t *task);                    // if task==NULL, return current vm's vcpus

struct vgic_t *alloc_vgic()
{
    if (_vgic_num >= VM_NUM_MAX) {
        logger_error("No more VGIC can be allocated!\n");
        return NULL;
    }
    struct vgic_t *vgic = &_vgic[_vgic_num++];
    memset(vgic, 0, sizeof(struct vgic_t));
    return vgic;
}

vgic_core_state_t *alloc_gicc()
{
    return &_state[_state_num++];
}

// 建立 vint 和 pint 的映射关系
// void virtual_gic_register_int(struct vgic_t *vgic, uint32_t pintvec, uint32_t vintvec)
// {
//     vgic->ptov[pintvec] = vintvec;
//     vgic->vtop[vintvec] = pintvec;
//     // vgic->use_irq[pintvec/32] |= 1 << (pintvec % 32);
// }

void vgicd_write(stage2_fault_info_t *info, trap_frame_t *el2_ctx, void *paddr)
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

void vgicd_read(stage2_fault_info_t *info, trap_frame_t *el2_ctx, void *paddr)
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
void intc_handler(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    struct _vm_t *vm = curr->curr_vm;
    struct vgic_t *vgic = vm->vgic;

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
            /*  is enable reg*/
            else if (gpa == GICD_ISENABLER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                
                gic_enable_int(HIGHEST_BIT_POSITION(r), 1);
                logger_info("      <<< gicd emu write GICD_ISENABLER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r));
            }
            else if (GICD_ISENABLER(1) <= gpa && gpa < GICD_ICENABLER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                int32_t id = ((gpa - GICD_ISENABLER(0)) / 0x4) * 32;
                
                gic_enable_int(HIGHEST_BIT_POSITION(r) + id, 1);
                logger_info("      <<< gicd emu write GICD_ISENABLER(i): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r) + id);
            }
            /* ic enable reg*/
            else if (gpa == GICD_ICENABLER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                
                gic_enable_int(HIGHEST_BIT_POSITION(r), 0);
                logger_info("      <<< gicd emu write GICD_ICENABLER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r));
            }
            else if (GICD_ICENABLER(1) <= gpa && gpa < GICD_ISPENDER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                int32_t id = ((gpa - GICD_ICENABLER(0)) / 0x4) * 32;

                gic_enable_int(HIGHEST_BIT_POSITION(r) + id, 0);
                logger_info("      <<< gicd emu write GICD_ICENABLER(i): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r) + id);
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
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                
                gic_set_pending(HIGHEST_BIT_POSITION(r), 0, 0);
                logger_info("      <<< gicd emu write GICD_ICPENDER(0): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r));
            }
            else if (GICD_ICPENDER(1) <= gpa && gpa < GICD_ISACTIVER(0))
            {
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t r = el2_ctx->r[reg_num];
                int32_t len = 1 << (info->hsr.dabt.size & 0x00000003);
                int32_t id = ((gpa - GICD_ICPENDER(0)) / 0x4) * 32;

                gic_set_pending(HIGHEST_BIT_POSITION(r) + id, 0, 0);
                logger_info("      <<< gicd emu write GICD_ICPENDER(i): ");
                logger("gpa: 0x%llx, r: 0x%llx, len: %d, int %d\n", gpa, r, len, HIGHEST_BIT_POSITION(r) + id);
            }
            /* I priority reg*/
            else if (GICD_IPRIORITYR(0) <= gpa && gpa < GICD_IPRIORITYR(GIC_FIRST_SPI / 4))
            {
                // SGI + PPI priority write
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t len = 1 << (info->hsr.dabt.size & 0x3);
                uint32_t val = el2_ctx->r[reg_num];

                int32_t offset = gpa - GICD_IPRIORITYR(0);
                int32_t int_id = offset;  // 每字节一个中断，直接用 offset

                for (int32_t i = 0; i < len; ++i) {
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
                int32_t reg_num = info->hsr.dabt.reg;
                int32_t len = 1 << (info->hsr.dabt.size & 0x3);
                uint32_t val = el2_ctx->r[reg_num];

                int32_t offset = gpa - GICD_IPRIORITYR(0);
                int32_t int_id = offset;

                for (int32_t i = 0; i < len; ++i) {
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
                            logger("case 0\n");
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
            else if (GICD_SPENDSGIR(0) <= gpa && gpa < GICD_SPENDSGIR(MAX_SGI_ID/4))
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
            else if (GICD_CPENDSGIR(0) <= gpa && gpa < GICD_CPENDSGIR(MAX_SGI_ID/4))
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
    struct vgic_t *vgic = vm->vgic;
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
        while(1) {;}
        return NULL;
    }
    return vgicc;
}

void vgicc_dump(vgic_core_state_t *vgicc)
{
    logger_info("====== VGICC Dump (vCPU ID: %u) ======\n", vgicc->id);

    logger_info("VMCR  = 0x%08x\n", vgicc->vmcr);
    logger_info("ELSR0 = 0x%08x\n", vgicc->saved_elsr0);
    logger_info("APR   = 0x%08x\n", vgicc->saved_apr);
    logger_info("HCR   = 0x%08x\n", vgicc->saved_hcr);

    for (int i = 0; i < GICH_LR_NUM; i++)
    {
        logger_info("LR[%1d] = 0x%08x\n", i, vgicc->saved_lr[i]);
    }

    logger_info("Pending IRQs:\n");
    for (int i = 0; i < SPI_ID_MAX; i++)
    {
        if (vgicc->irq_pending_mask[i])
        {
            logger_info("  IRQ %d is pending (val=0x%08x)\n", i, vgicc->irq_pending_mask[i]);
        }
    }

    logger_info("======================================\n");
}

void vgicc_hw_dump(void)
{
    logger_info("====== VGICC HW Dump ======\n");

    uint32_t vmcr  = mmio_read32((void *)(GICH_VMCR));
    uint32_t elsr0 = mmio_read32((void *)(GICH_ELSR0));
    uint32_t elsr1 = mmio_read32((void *)(GICH_ELSR1));
    uint32_t apr   = mmio_read32((void *)(GICH_APR));
    uint32_t hcr   = mmio_read32((void *)(GICH_HCR));
    uint32_t vtr   = mmio_read32((void *)(GICH_VTR));
    uint32_t misr  = mmio_read32((void *)(GICH_MISR));

    logger_info("VMCR  = 0x%08x\n", vmcr);
    logger_info("ELSR0 = 0x%08x\n", elsr0);
    logger_info("ELSR1 = 0x%08x\n", elsr1);
    logger_info("APR   = 0x%08x\n", apr);
    logger_info("HCR   = 0x%08x\n", hcr);
    logger_info("VTR   = 0x%08x\n", vtr);
    logger_info("MISR  = 0x%08x\n", misr);

    for (int i = 0; i < GICH_LR_NUM; i++) {
        uint32_t lr = mmio_read32((void *)(GICH_LR(i)));
        if (lr != 0) {
            uint32_t vid = lr & 0x3ff;
            uint32_t pid = (lr >> 10) & 0x3ff;
            uint32_t pri = (lr >> 23) & 0x1f;
            uint32_t state = (lr >> 28) & 0x3;
            uint32_t grp1 = (lr >> 30) & 0x1;
            uint32_t hw = (lr >> 31) & 0x1;

            logger_info("LR[%1d] = 0x%08x (VID=%d, PID=%d, PRI=%d, STATE=%d, GRP1=%d, HW=%d)\n",
                       i, lr, vid, pid, pri, state, grp1, hw);
        } else {
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

    if (vcpu_id >= 8) return;

    bool changed = false;
    
    // 首次初始化或检测变化
    if (!initialized[vcpu_id]) {
        changed = true;
        initialized[vcpu_id] = true;
    } else {
        if (last_vmcr[vcpu_id] != state->vmcr || 
            last_elsr0[vcpu_id] != state->saved_elsr0 || 
            last_hcr[vcpu_id] != state->saved_hcr) {
            changed = true;
        }
        
        if (!changed) {
            for (int32_t i = 0; i < GICH_LR_NUM; i++) {
                if (last_lr[vcpu_id][i] != state->saved_lr[i]) {
                    changed = true;
                    break;
                }
            }
        }
    }

    if (changed) {
        logger_warn("====== vCPU %d Hardware State Changed ======\n", vcpu_id);
        logger_info("Task ID: %d, VM ID: %d\n", task_id, vm_id);
        logger_info("VMCR: 0x%08x, ELSR0: 0x%08x, HCR: 0x%08x\n", 
            state->vmcr, state->saved_elsr0, state->saved_hcr);
        
        for (int32_t i = 0; i < GICH_LR_NUM; i++) {
            if (state->saved_lr[i] != 0) {
                logger_info("LR[%d]: 0x%08x\n", i, state->saved_lr[i]);
            }
        }
        logger_warn("==========================================\n");

        // 更新缓存
        last_vmcr[vcpu_id] = state->vmcr;
        last_elsr0[vcpu_id] = state->saved_elsr0;
        last_hcr[vcpu_id] = state->saved_hcr;
        for (int32_t i = 0; i < GICH_LR_NUM; i++) {
            last_lr[vcpu_id][i] = state->saved_lr[i];
        }
    }
}

// 检查虚拟中断注入的完整状态
void vgic_check_injection_status(void)
{
    tcb_t *curr = (tcb_t *)read_tpidr_el2();

    logger_info("====== VGIC Injection Status Check ======\n");

    if (!curr) {
        logger_error("No current task\n");
        return;
    }

    logger_info("Current task ID: %d\n", curr->task_id);

    if (!curr->curr_vm) {
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


void vgic_inject_sgi(tcb_t *task, uint32_t int_id)
{
    // assert(int_id < 16);  // SGI 范围应是 0~15

    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);

    // 如果已 pending，不重复注入
    if (vgicc->irq_pending_mask[int_id])
    {
        logger_warn("SGI %d already pending on vCPU %d, skip inject.\n", int_id, task->task_id);
        return;
    }

    // 标记为 pending
    vgicc->irq_pending_mask[int_id] = 1;

    logger_info("[pcpu: %d]: Inject SGI id: %d to vCPU: %d(task: %d)\n", 
        get_current_cpu_id(), int_id, get_vcpuid(task), task->task_id);

    // 如果当前正在运行此 vCPU，尝试立即注入
    if (task == (tcb_t *)read_tpidr_el2())
    {
        logger_info("[pcpu: %d]: (Is running)Try to inject pending SGI for vCPU: %d (task: %d)\n", 
            get_current_cpu_id(), get_vcpuid(task), task->task_id);
        vgic_try_inject_pending(task); // 你可以实现这个函数尝试把 pending 的 SGI 填进 GICH_LR
    }

    // vgicc_dump(vgicc);
}

void vgic_try_inject_pending(tcb_t *task)
{
    vgic_core_state_t *vgicc = get_vgicc_by_vcpu(task);

    // 使用软件保存的 ELSR0 来判断空闲的 LR
    uint64_t elsr = vgicc->saved_elsr0; // 目前你只有 ELSR0，够用（最多 32 个 LR）

    for (int i = 0; i < MAX_SGI_ID; ++i)
    {
        if (!vgicc->irq_pending_mask[i])
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
            logger_warn("No free LR for SGI %d (in memory), delay inject.\n", i);
            break;
        }

        int32_t vcpu_id = get_vcpuid(task);
        uint32_t lr_val = gic_make_virtual_software_sgi(i, i, /*cpu_id=*/vcpu_id, 0);
        
        // 将虚拟中断写入到内存中的 LR
        vgicc->saved_lr[freelr] = lr_val;
        // 标记该 LR 不再空闲（ELSR 置位为 0 表示 occupied）
        vgicc->saved_elsr0 &= ~(1U << freelr);

        vgicc->irq_pending_mask[i] = 0;

        logger_info("[pcpu: %d]: Injected SGI %d into LR%d for vCPU: %d (task: %d), LR value: 0x%lx\n", 
            get_current_cpu_id(), i, freelr, vcpu_id, task->task_id, lr_val);
        
        // dev use
        // gicc_restore_core_state();
        // vgicc_hw_dump();
        // vgic_sw_inject_test(i);
        // vgicc_hw_dump();
    }
}


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

// vgic inject
void vgic_hw_inject_test(uint32_t vector)
{
    logger_info("vgic inject vector: %d\n", vector);

    // 检查当前是否有运行的虚拟机
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    if (!curr || !curr->curr_vm) {
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

    if (freelr < 0) {
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

void vgic_sw_inject_test(uint32_t vector) {
        logger_info("vgic inject vector: %d\n", vector);

    // 检查当前是否有运行的虚拟机
    tcb_t *curr = (tcb_t *)read_tpidr_el2();
    if (!curr || !curr->curr_vm) {
        logger_warn("No current VM for interrupt injection\n");
        return;
    }

    uint32_t mask = gic_make_virtual_software_sgi(vector, vector, /*cpu_id=*/0, 0);

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

    if (freelr < 0) {
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

#ifndef __VMM_CFG_H__
#define __VMM_CFG_H__

/* ============================================================================
 * 虚拟化配置参数
 * ============================================================================ */

/* 虚拟机数量配置 */
#define VCPU_NUM_MAX 8
#define VM_NUM_MAX 4


/* MMIO页面映射配置 */
#define MMIO_PAGES_GICD 16
#define MMIO_PAGES_GICC 16
#define MMIO_PAGES_PL011 1
#define MMIO_PAGE_SIZE 0x1000

/* 虚拟CPU绑定配置 */
#define PRIMARY_VCPU_PCPU_MASK (1 << 0)  // 主vCPU绑定到pCPU 0
#define SECONDARY_VCPU_PCPU_MASK (1 << 1)  // 从vCPU绑定到pCPU 1

/* 虚拟栈配置 */
#define VM_STACK_PAGES 2  // 每个vCPU分配2页栈空间
#define VM_STACK_SIZE (VM_STACK_PAGES * 4096)  // 8KB

#endif // __VMM_CFG_H__
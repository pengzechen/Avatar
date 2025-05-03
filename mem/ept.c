
#include <mem/page.h>
#include <mem/ept.h>
#include <os_cfg.h>
#include <io.h>
#include <exception.h>
#include <hyper/vcpu.h>
#include <hyper/vgic.h>

extern lpae_t ept_L1[];
lpae_t *ept_L2_root;
lpae_t *ept_L3_root;

static int handle_mmio(ept_violation_info_t *info, trap_frame_t *el2_ctx);

/* Return the cache property of the input gpa */
/* It is determined depending on whether the */
/* address is for device or memory */
static bool isInMemory(unsigned long gpa)
{
	if (GUEST_RAM_START <= gpa && gpa < GUEST_RAM_END)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void apply_ept(void *ept)
{
	isb();
	dsb(sy);
	clean_and_invalidate_dcache_va_range(ept, PAGE_SIZE);
	isb();
	dsb(sy);
	flush_tlb();
	isb();
	dsb(sy);
}

// 2 9 9 12 最多管理4G内存
void guest_ept_init(void)
{
	int index_l1, index_l2, index_l3;
	unsigned long long gpa = 0;
	unsigned long vttbr_val = (unsigned long)ept_L1;
	unsigned long hcr;

	/* Calculate next level page table address */
	/* Level 1 */
	void *ept_L1_root = &ept_L1[LPAE_L1_SIZE];
	/* Level 2 */
	ept_L2_root = (lpae_t *)(((unsigned long)ept_L1_root + (1 << 12)) & ~0xFFF); /* Align */
	/* Level 3 */
	ept_L3_root = &ept_L2_root[LPAE_L2_SIZE];

	printf("Initialize EPT...\n");
	printf("EPT root address : 0x%x\n", ept_L1);
	printf("ept_L2_root : 0x%x\n", ept_L2_root);
	printf("ept_L3_root : 0x%x\n", ept_L3_root);
	printf("LPAE_L1_SIZE : %d\n", LPAE_L1_SIZE);
	printf("LPAE_L2_SIZE : %d\n", LPAE_L2_SIZE);
	printf("LPAE_L3_SIZE : %d\n", LPAE_L3_SIZE);

	for (index_l1 = 0; index_l1 < LPAE_L1_SIZE; index_l1++)
	{
		lpae_t entry_l1;
		lpae_t *ept_l2 = &ept_L2_root[LPAE_ENTRIES * index_l1];

		/* Set first level page table entries */
		entry_l1.bits = 0;
		entry_l1.p2m.valid = 1;
		entry_l1.p2m.table = 1;
		entry_l1.bits |= (unsigned long)ept_l2;
		ept_L1[index_l1].bits = entry_l1.bits;
		for (index_l2 = 0; index_l2 < LPAE_ENTRIES; index_l2++)
		{
			lpae_t entry_l2;
			lpae_t *ept_l3 = &ept_L3_root[LPAE_ENTRIES * LPAE_ENTRIES * index_l1 + LPAE_ENTRIES * index_l2];
			// printf("(EPT_L3)0x%x - %d/%d (%d)\n",ept_l3, index_l1,index_l2, LPAE_L2_SIZE * LPAE_ENTRIES * index_l1  + LPAE_ENTRIES * index_l2);

			/* Set second level page table entries */
			entry_l2.bits = 0;
			entry_l2.p2m.valid = 1;
			entry_l2.p2m.table = 1;
			entry_l2.bits |= (unsigned long)ept_l3;
			ept_l2[index_l2].bits = entry_l2.bits;

			for (index_l3 = 0; index_l3 < LPAE_ENTRIES; index_l3++)
			{
				lpae_t entry_l3;
				/* Set third level page table entries */
				/* 4KB Page */
				entry_l3.bits = 0;
				entry_l3.p2m.valid = 1;
				entry_l3.p2m.table = 1;
				entry_l3.p2m.af = 1;
				entry_l3.p2m.read = 1;
				entry_l3.p2m.write = 1;
				entry_l3.p2m.mattr = 0xF;
				entry_l3.p2m.sh = 0x03;
				entry_l3.p2m.xn = 0x0;

				if (isInMemory(gpa))
				{
					/* RAM area */
					entry_l3.p2m.sh = 0x03;
					entry_l3.p2m.mattr = 0xF; /* 1111b: Outer Write-back Cacheable / Inner write-back cacheable */
				}
				else
				{
					/* Device area */
					entry_l3.p2m.mattr = 0x1; /* 0001b: Device Memory */
					entry_l3.p2m.sh = 0x0;
					entry_l3.p2m.xn = 1;
				}
				entry_l3.bits |= gpa;
				ept_l3[index_l3].bits = entry_l3.bits;
				// {
				//   /* For logging.. */
				//   lpae_t *pept;
				//   pept = get_ept_entry(gpa);
				//   if(pept != &ept_l3[index_l3])
				//   {
				//     printf("(Index)%d/%d/%d - ", index_l1,index_l2,index_l3);
				//     printf("(L1)0x%x - ",(unsigned long)entry_l1.bits);
				//     printf("(L2Adr)0x%x - ",(unsigned long)&ept_l2[index_l2]);
				//     printf("(L2)0x%x - ",(unsigned long)entry_l2.bits);
				//     printf("(L3Adr)0x%x - ",(unsigned long)&ept_l3[index_l3]);
				//     printf("(L3)0x%x - ",(unsigned long)entry_l3.bits);
				//     printf("(GPA)0x%x - ",(unsigned long)gpa);
				//     printf("Error - ");
				//     printf("(EPT)0x%x - ",ept_l3);
				//     printf("(PAddr)0x%x - (PVAL)0x%x\n",pept,(unsigned long)pept->bits);
				//   }
				// }
				gpa += (4 * 1024); /* 4KB page frame */
			}
			apply_ept(ept_l3);
		}
		apply_ept(ept_l2);
	}
	apply_ept(ept_L1);

	// Write EPT to VTTBR
	asm volatile(
		"msr VTTBR_EL2 , %0\n\t"
		: /*  no out put */
		: "r"(vttbr_val)
		: "r1");
	isb();
}

lpae_t *get_ept_entry(paddr_t gpa)
{
	unsigned long page_num;
	page_num = (gpa >> 12);
	return &ept_L3_root[page_num];
}

static inline uint64_t gva_to_ipa_par(uint64_t va)
{
	uint64_t par, tmp;
	tmp = read_par();  // 保存当前 PAR 寄存器值
	write_ats1cpr(va); // 写入 VA 以触发地址转换
	isb();			   // 确保转换结果可用
	par = read_par();  // 读取转换结果
	write_par(tmp);	   // 恢复 PAR 寄存器值
	return par;		   // 返回转换后的物理地址
}

int gva_to_ipa(uint64_t va, uint64_t *paddr)
{
	uint64_t par = gva_to_ipa_par(va);
	if (par & PAR_F)
	{
		return -1; // 转换失败
	}
	*paddr = (par & PADDR_MASK & PAGE_MASK) | (va & ~PAGE_MASK);
	return 0; // 转换成功
}

void data_abort_handler(ept_violation_info_t *info, trap_frame_t *el2_ctx)
{
	lpae_t *ept;
	unsigned long tmp;

	// printf("EPT Violation : %s\n", info->reason == PREFETCH ? "prefetch" : "data");
	// printf("PC : %x\n", el2_ctx->elr);
	// printf("GVA : 0x%x\n", info->gva);
	// printf("GPA : 0x%x\n", (unsigned long)info->gpa);
	// printf("Register : R%d\n", info->hsr.dabt.reg);

	ept = get_ept_entry(info->gpa);
	tmp = ept->bits & 0xFFFFFFFF;
	// printf("EPT Entry : 0x%x(0x%x)\n", ept, tmp);
	/*
	if (handle_mmio(info, el2_ctx))
	{
	}
	*/
	if (GICD_BASE_ADDR <= info->gpa && info->gpa <= (GICD_BASE_ADDR + 0x0010000)) {
		intc_handler(info, el2_ctx);
		return;
	}
	
	if (GICC_BASE_ADDR <= info->gpa && info->gpa <= (GICC_BASE_ADDR + 0x0010000)) {
		info->gpa = info->gpa + 0x30000;
		handle_mmio(info, el2_ctx);
		return;
	}

	/* Do not delete following code block */
	/* A sample code for modifying EPT */
	/* After modifying EPT, we must flush both cache and TLB */
	// {
	//   // isb();
	//   // dsb();
	//   // WRITE_SYSREG(hcr & ~HCR_VM, HCR_EL2);
	//   // isb();
	//   // dsb();
	//   // ept = get_ept_entry(info.gpa);
	//   // tmp = ept->bits & 0xFFFFFFFF;
	//   // printf("EPT Entry : 0x%x(0x%x)\n",ept,tmp);
	//   // printf("Enable EPT Access\n");
	//   // ept->p2m.read = 1;
	//   // ept->p2m.write = 1;
	//   // isb();
	//   // dsb(sy);
	//   // WRITE_SYSREG(hcr | HCR_VM, HCR_EL2);
	//   // isb();
	//   // dsb(sy);
	//   // clean_and_invalidate_dcache_va_range(ept, PAGE_SIZE);
	//   // flush_tlb();
	// }
}

int handle_mmio(ept_violation_info_t *info, trap_frame_t *el2_ctx)
{
	paddr_t gpa = info->gpa;
	// printf("operation gpa: 0x%x\n", gpa);
	// if (MMIO_ARREA <= gpa && gpa <= (MMIO_ARREA + 4096))
	// {
		if (info->hsr.dabt.write)
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
			dst = (unsigned long *)(unsigned long)gpa;
			//printf("(%d bytes) 0x%x  R%d\n", (unsigned long)len, *dst, (unsigned long)reg_num);
			
			//printf("old data: 0x%x\n", *dst);
			// 将数据写入寄存器或进行其他必要的操作
			if (reg_num != 30)
			{
				*dst = *(unsigned long *)buf;
			}
			// 确保所有更改都能被看到
			dsb(sy);
			isb();
			//printf("new data: 0x%x\n", *dst);
		}
		else
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

			src = (unsigned long *)(unsigned long)gpa;
			dat = *src;
			//printf("(%d bytes) 0x%x R%d\n", (unsigned long)len, *src, (unsigned long)reg_num);
			
			//printf("old data: 0x%x\n", *r);
			if (reg_num != 30)
			{
				*(unsigned long *)buf = dat;
			}
			dsb(sy);
			isb();
			//printf("new data: 0x%x\n", *r);
			
			// spin_unlock(&vcpu.lock);
		}
		return 1;
	// }

	// return 0;
}
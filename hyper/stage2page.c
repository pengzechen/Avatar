
#include <mem/page.h>
#include <mem/stage2page.h>
#include <os_cfg.h>
#include <io.h>
#include <exception.h>
#include <hyper/vcpu.h>
#include <hyper/vgic.h>
#include <lib/aj_string.h>

extern lpae_t ept_L1[];
lpae_t *ept_L2_root;
lpae_t *ept_L3_root;

static int32_t handle_mmio(stage2_fault_info_t *info, trap_frame_t *el2_ctx);
static int32_t handle_mmio_hack(stage2_fault_info_t *info, trap_frame_t *el2_ctx);

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
	int32_t index_l1, index_l2, index_l3;
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

	logger("Initialize EPT...\n");
	logger_warn("EPT root address : 0x%llx\n", ept_L1);
	logger("ept_L2_root : 0x%llx\n", ept_L2_root);
	logger("ept_L3_root : 0x%llx\n", ept_L3_root);
	logger("LPAE_L1_SIZE : %d\n", LPAE_L1_SIZE);
	logger("LPAE_L2_SIZE : %d\n", LPAE_L2_SIZE);
	logger("LPAE_L3_SIZE : %d\n", LPAE_L3_SIZE);

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
			// logger("(EPT_L3)0x%llx - %d/%d (%d)\n",ept_l3, index_l1,index_l2, LPAE_L2_SIZE * LPAE_ENTRIES * index_l1  + LPAE_ENTRIES * index_l2);

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
				//     logger("(Index)%d/%d/%d - ", index_l1,index_l2,index_l3);
				//     logger("(L1)0x%llx - ",(unsigned long)entry_l1.bits);
				//     logger("(L2Adr)0x%llx - ",(unsigned long)&ept_l2[index_l2]);
				//     logger("(L2)0x%llx - ",(unsigned long)entry_l2.bits);
				//     logger("(L3Adr)0x%llx - ",(unsigned long)&ept_l3[index_l3]);
				//     logger("(L3)0x%llx - ",(unsigned long)entry_l3.bits);
				//     logger("(GPA)0x%llx - ",(unsigned long)gpa);
				//     logger("Error - ");
				//     logger("(EPT)0x%llx - ",ept_l3);
				//     logger("(PAddr)0x%llx - (PVAL)0x%llx\n",pept,(unsigned long)pept->bits);
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

int32_t gva_to_ipa(uint64_t va, uint64_t *paddr)
{
	uint64_t par = gva_to_ipa_par(va);
	if (par & PAR_F)
	{
		return -1; // 转换失败
	}
	*paddr = (par & PADDR_MASK & PAGE_MASK) | (va & ~PAGE_MASK);
	return 0; // 转换成功
}

void data_abort_handler(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
	lpae_t *ept;
	unsigned long tmp;

	// logger("EPT Violation : %s\n", info->reason == PREFETCH ? "prefetch" : "data");
	// logger("PC : %llx\n", el2_ctx->elr);
	// logger("GVA : 0x%llx\n", info->gva);
	// logger("GPA : 0x%llx\n", (unsigned long)info->gpa);
	// logger("Register : R%d\n", info->hsr.dabt.reg);

	ept = get_ept_entry(info->gpa);
	tmp = ept->bits & 0xFFFFFFFF;
	// logger("EPT Entry : 0x%llx(0x%llx)\n", ept, tmp);
	/*
	if (handle_mmio(info, el2_ctx))
	{
	}
	*/
	if (GICD_BASE_ADDR <= info->gpa && info->gpa < (GICD_BASE_ADDR + 0x0010000))
	{
		intc_handler(info, el2_ctx);
		return;
	}

	if (GICC_BASE_ADDR <= info->gpa && info->gpa < (GICC_BASE_ADDR + 0x0010000))
	{
		info->gpa = info->gpa + 0x30000;
		handle_mmio(info, el2_ctx);
		// handle_mmio_hack(info, el2_ctx);
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
	//   // logger("EPT Entry : 0x%llx(0x%llx)\n",ept,tmp);
	//   // logger("Enable EPT Access\n");
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

// 用这个函数可以在smp=1 hack的跑起linux
int32_t handle_mmio_hack(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
	paddr_t gpa = info->gpa;
	if (gpa == 0x8040000)
		return 0;
	
	// 防止霸屏，eoir 和 iar 不输出
	if (gpa != 0x8040010 && gpa != 0x804000c)
		logger("====> [vgicc: ]operation gpa: 0x%llx\n", gpa);

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
		// logger("(%d bytes) 0x%llx  R%d\n", (unsigned long)len, *dst, (unsigned long)reg_num);

		// logger("old data: 0x%llx\n", *dst);
		//  将数据写入寄存器或进行其他必要的操作
		if (reg_num != 30)
		{
			*dst = *(unsigned long *)buf;
		}
		// 确保所有更改都能被看到
		dsb(sy);
		isb();
		// logger("new data: 0x%llx\n", *dst);
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
		// logger("(%d bytes) 0x%llx R%d\n", (unsigned long)len, *src, (unsigned long)reg_num);

		// logger("old data: 0x%llx\n", *r);
		if (reg_num != 30)
		{
			*(unsigned long *)buf = dat;
		}
		dsb(sy);
		isb();
		// logger("new data: 0x%llx\n", *r);

		// spin_unlock(&vcpu.lock);
	}
	return 1;
	// }

	// return 0;
}

int32_t handle_mmio(stage2_fault_info_t *info, trap_frame_t *el2_ctx)
{
	paddr_t gpa = info->gpa;
	
	// 防止霸屏，eoir 和 iar 和 dir 不输出
	if (gpa != 0x8040010 && gpa != 0x804000c && gpa != 0x8041000)
		logger("====> [vgicc: ]operation gpa: 0x%llx\n", gpa);
	
	if (info->hsr.dabt.write)
	{
		unsigned long reg_num;
		volatile uint64_t *r;
		volatile void *buf;
		volatile unsigned long len;
		volatile unsigned long *dst;

		// 获取寄存器编号和 MMIO 操作的大小
		reg_num = info->hsr.dabt.reg;
		len = 1UL << (info->hsr.dabt.size & 0x3); // size = 0(b) 1(h) 2(w) 3(x)

		// 计算源缓冲区（从寄存器读取数据）
		r = &el2_ctx->r[reg_num];
		buf = (void *)r;

		// 目标地址：Guest Physical Address
		dst = (unsigned long *)(unsigned long)gpa;

		// 安全写入
		if (reg_num != 30)
		{
			if (((unsigned long)dst % len) == 0)
			{
				// 对齐，直接写入
				memcpy((void *)dst, (void *)buf, len);
			}
			else
			{
				// 不对齐，安全写入
				uint8_t tmp[8];
				memcpy(tmp, (const void *)buf, len);
				memcpy((void *)dst, (const void *)tmp, len);
			}
		}

		// 确保所有更改都能被看到
		dsb(sy);
		isb();
	}

	else
	{
		unsigned long reg_num;
		volatile uint64_t *r;
		volatile void *buf;
		volatile unsigned long *src;
		volatile unsigned long len;
		unsigned long dat = 0;

		reg_num = info->hsr.dabt.reg;
		r = &el2_ctx->r[reg_num];
		len = 1UL << (info->hsr.dabt.size & 0x3); // 1, 2, 4, or 8
		buf = (void *)r;

		src = (unsigned long *)(unsigned long)gpa;

		if (((unsigned long)src % len) == 0)
		{
			// 对齐访问，安全读取
			memcpy(&dat, (const void *)src, len);
		}
		else
		{
			// 非对齐访问，使用 memcpy 避免崩溃
			uint8_t tmp[8] = {0};
			memcpy(tmp, (const void *)src, len);
			memcpy(&dat, tmp, len);
		}

		if (reg_num != 30)
		{
			memcpy((void *)buf, &dat, len);
		}

		dsb(sy);
		isb();
	}
	return 1;
}

#ifndef __VCPU_H__
#define __VCPU_H__

#include "../spinlock.h"
#include "../aj_types.h"
#include "../exception.h"

typedef struct _cpu_sysregs
{
	// not trapped (save & restore performed)
	unsigned long sctlr_el1;
	unsigned long ttbr0_el1;
	unsigned long ttbr1_el1;
	unsigned long tcr_el1;
	unsigned long esr_el1;
	unsigned long far_el1;
	unsigned long afsr0_el1;
	unsigned long afsr1_el1;
	unsigned long mair_el1;
	unsigned long amair_el1;
	unsigned long contextidr_el1;

	unsigned long cpacr_el1;
	unsigned long elr_el1;
	unsigned long fpcr;
	unsigned long fpsr;
	unsigned long midr_el1;	 // ro
	unsigned long mpidr_el1; // ro
	unsigned long par_el1;
	unsigned long sp_el0;
	unsigned long sp_el1;
	unsigned long spsr_el1;
	unsigned long tpidr_el0;
	unsigned long tpidr_el1;
	unsigned long tpidrro_el0;
	unsigned long vbar_el1;

	// trapped by TACR
	unsigned long actlr_el1; // rw

	// trapped by TID3
	unsigned long id_pfr0_el1;		// ro
	unsigned long id_pfr1_el1;		// ro
	unsigned long id_mmfr0_el1;		// ro
	unsigned long id_mmfr1_el1;		// ro
	unsigned long id_mmfr2_el1;		// ro
	unsigned long id_mmfr3_el1;		// ro
	unsigned long id_isar0_el1;		// ro
	unsigned long id_isar1_el1;		// ro
	unsigned long id_isar2_el1;		// ro
	unsigned long id_isar3_el1;		// ro
	unsigned long id_isar4_el1;		// ro
	unsigned long id_isar5_el1;		// ro
	unsigned long mvfr0_el1;		// ro
	unsigned long mvfr1_el1;		// ro
	unsigned long mvfr2_el1;		// ro
	unsigned long id_aa64pfr0_el1;	// ro
	unsigned long id_aa64pfr1_el1;	// ro
	unsigned long id_aa64dfr0_el1;	// ro
	unsigned long id_aa64dfr1_el1;	// ro
	unsigned long id_aa64isar0_el1; // ro
	unsigned long id_aa64isar1_el1; // ro
	unsigned long id_aa64mmfr0_el1; // ro
	unsigned long id_aa64mmfr1_el1; // ro
	unsigned long id_aa64afr0_el1;	// ro
	unsigned long id_aa64afr1_el1;	// ro

	// trapped by TID2
	unsigned long ctr_el0;	  // ro
	unsigned long ccsidr_el1; // ro
	unsigned long clidr_el1;  // ro
	unsigned long csselr_el1; // rw

	// trapped by TID1
	unsigned long aidr_el1;	  // ro
	unsigned long revidr_el1; // ro

	// system timer
	unsigned long cntkctl_el1;
	unsigned long cntp_ctl_el0;
	unsigned long cntp_cval_el0;
	unsigned long cntp_tval_el0;
	unsigned long cntv_ctl_el0;
	unsigned long cntv_cval_el0;
	unsigned long cntv_tval_el0;

} cpu_sysregs_t;

extern cpu_sysregs_t cpu_sysregs[];

// #define select_user_reg(reg) (&(vcpu.regs[reg]))

extern void set_stage2_pgd(unsigned long, unsigned long);
extern void restore_sysregs(cpu_sysregs_t *);
extern void save_sysregs(cpu_sysregs_t *);
extern void get_all_sysregs(cpu_sysregs_t *);
extern void save_cpu_ctx(trap_frame_t *);

#endif // __VCPU_H__
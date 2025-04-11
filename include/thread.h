
#ifndef __THREAD_H__
#define __THREAD_H__

struct thread_info {
	int             cpu;
	unsigned int    flags;
	void            *pgtable;
	void            *current_thread;
#if 0
	vector_fn       vector_handlers[VECTOR_MAX];
	exception_fn    exception_handlers[VECTOR_MAX][EC_MAX];
#endif

	char ext[0];		/* allow unit tests to add extended info */
};

#define MIN_THREAD_SHIFT	14	/* THREAD_SIZE == 16K */

#define THREAD_SIZE		(1UL << MIN_THREAD_SHIFT)


register unsigned long current_stack_pointer asm("sp");

static inline struct thread_info *thread_info_sp(unsigned long sp)
{
	return (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
}

static inline struct thread_info * current_thread_info(void)
{
	return thread_info_sp(current_stack_pointer);
}

static inline struct thread_info * set_thread_info(void * new_sp)
{
	return thread_info_sp((unsigned long)new_sp);
}

static inline unsigned int get_current_cpu_id(void)
{
    unsigned long mpidr;
    __asm__ __volatile__("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (unsigned int)(mpidr & 0xff);
}

static inline uint64_t read_tpidr_el0(void) {
    uint64_t val;
    asm volatile("mrs %0, tpidr_el0" : "=r"(val));
    return val;
}

static inline void write_tpidr_el0(uint64_t val) {
    asm volatile("msr tpidr_el0, %0" :: "r"(val));
}

static inline uint64_t read_tpidr_el2(void) {
    uint64_t val;
    asm volatile("mrs %0, tpidr_el2" : "=r"(val));
    return val;
}

static inline void write_tpidr_el2(uint64_t val) {
    asm volatile("msr tpidr_el2, %0" :: "r"(val));
}


#endif // __THREAD_H__
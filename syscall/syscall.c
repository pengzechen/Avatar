
#include "io.h"
#include "syscall_num.h"

uint64_t sys_debug(void* args)
{
	printf("[syscall] sys_debug: %c, %c\n", *(uint64_t*)args, * ((uint64_t*)args + 1) );
	return 0;
}

uint64_t sys_putc(void* args)
{
	putc(*(char*)args);
	return 0;
}

uint64_t sys_getc(void* args)
{
	return getc();
}

const void *syscall_table[NR_SYSCALL] = {
	[SYS_putc] = sys_putc,
	[SYS_getc] = sys_getc,
	
    [SYS_debug] = sys_debug,
};
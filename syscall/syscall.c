
#include "io.h"
#include "syscall_num.h"
#include "task.h"

uint64_t _debug(void* args)
{
	printf("[syscall] sys_debug: %c, %c\n", *(uint64_t*)args, * ((uint64_t*)args + 1) );
	return 0;
}

uint64_t _putc(void* args)
{
	putc(*(char*)args);
	return 0;
}

uint64_t _getc(void* args)
{
	return getc();
}

void _sleep_tick(void* args)
{
	sys_sleep_tick(*(uint64_t*)args);
}

const void *syscall_table[NR_SYSCALL] = {
	[SYS_putc] = _putc,
	[SYS_getc] = _getc,
	[SYS_sleep] = _sleep_tick,
	
    [SYS_debug] = _debug,
};
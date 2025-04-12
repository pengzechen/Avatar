
#include "io.h"
#include "syscall_num.h"
#include "task/task.h"
#include "task/mutex.h"
#include "mem/mem.h"

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
	
	[SYS_mutex_test_print] = mutex_test_print,
	[SYS_mutex_add] = mutex_test_add,
	[SYS_mutex_minus] = mutex_test_minus,
    [SYS_debug] = _debug,
};
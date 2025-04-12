
#include "mem/mem.h"
#include "os_cfg.h"

static addr_alloc_t g_alloc;

void alloctor_init() 
{
    bitmap_create_from_memory(&g_alloc.bitmap, 0, KERNEL_RAM_END);
    g_alloc.start = 0;
    g_alloc.size = KERNEL_RAM_END;
    g_alloc.page_size = PAGE_SIZE;
}
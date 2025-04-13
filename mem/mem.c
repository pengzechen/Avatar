
#include "mem/mem.h"
#include "os_cfg.h"
#include "task/mutex.h"
#include "io.h"
#include "thread.h"

static addr_alloc_t g_alloc;

void alloctor_init() 
{
    bitmap_create_from_memory(&g_alloc.bitmap, 0, KERNEL_RAM_END);
    g_alloc.start = 0;
    g_alloc.size = KERNEL_RAM_END;
    g_alloc.page_size = PAGE_SIZE;

    mutex_init(&g_alloc.mutex);
}


uint64_t mutex_test_num = 0;

uint64_t mutex_test_add() {
    mutex_lock(&g_alloc.mutex);
    for (int i=0; i<10000; i++) {
        mutex_test_num ++;
        // mutex_test_num --;
        mutex_test_num ++;
        // mutex_test_num --;
        mutex_test_num ++;
        // mutex_test_num --;
        mutex_test_num ++;
        // mutex_test_num --;
    }
    mutex_test_num ++;
    mutex_unlock(&g_alloc.mutex);
    return mutex_test_num;
}

uint64_t mutex_test_minus() {
    mutex_lock(&g_alloc.mutex);
    for (int i=0; i<10000; i++) {
        // mutex_test_num ++;
        mutex_test_num --;
        // mutex_test_num ++;
        mutex_test_num --;
        // mutex_test_num ++;
        mutex_test_num --;
        // mutex_test_num ++;
        mutex_test_num --;
    }
    mutex_test_num --;
    mutex_unlock(&g_alloc.mutex);
    return mutex_test_num;
}

void mutex_test_print() {
    printf("mutex_test_num = %d, current task: %d\n", mutex_test_num, ((tcb_t*)(void*)read_tpidr_el0())->id);
}
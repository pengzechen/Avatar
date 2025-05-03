
#include <pro.h>
#include <list.h>
#include <aj_string.h>

void init_process(process_t * pro, uint32_t id, char * name) {
    
    pro->id = id;
    
    memcpy(pro->name, name, strlen(name));
    pro->name[PRO_MAX_NAME_LEN] = '\0';
    
    list_init(&pro->threads);
}

void run_process(process_t * pro) {
    // 遍历所有的 thread 把他们的状态设置为就绪

}

void exit_process(process_t * pro) {
    // 释放内存，把状态设置为退出
}

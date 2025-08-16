#include "vmm/virtio.h"
#include "vmm/vm.h"
#include "io.h"
#include "lib/avatar_string.h"

// VirtIO Console 配置结构
typedef struct virtio_console_config {
    uint16_t cols;          // 列数
    uint16_t rows;          // 行数
    uint32_t max_nr_ports;  // 最大端口数
    uint32_t emerg_wr;      // 紧急写入
} virtio_console_config_t;

// VirtIO Console 设备结构
typedef struct virtio_console {
    virtio_device_t *dev;
    virtio_console_config_t config;
    
    // 输入/输出缓冲区
    char input_buffer[1024];
    uint32_t input_head;
    uint32_t input_tail;
    
    char output_buffer[1024];
    uint32_t output_head;
    uint32_t output_tail;
    
    bool input_available;
} virtio_console_t;

// Console 设备池
static virtio_console_t _console_devices[VM_NUM_MAX];
static uint32_t _console_count = 0;

// VirtIO Console 特性位
#define VIRTIO_CONSOLE_F_SIZE           0   // 配置cols和rows
#define VIRTIO_CONSOLE_F_MULTIPORT      1   // 多端口支持
#define VIRTIO_CONSOLE_F_EMERG_WRITE    2   // 紧急写入支持

// 队列索引
#define VIRTIO_CONSOLE_RX_QUEUE         0   // 接收队列
#define VIRTIO_CONSOLE_TX_QUEUE         1   // 发送队列

// 前向声明
static void console_queue_notify(virtio_device_t *dev, uint32_t queue_idx);
static void console_reset(virtio_device_t *dev);

virtio_console_t *virtio_console_create(uint64_t base_addr, uint32_t irq)
{
    if (_console_count >= VM_NUM_MAX) {
        logger_error("No more VirtIO console devices can be allocated\n");
        return NULL;
    }
    
    virtio_console_t *console = &_console_devices[_console_count++];
    memset(console, 0, sizeof(virtio_console_t));
    
    // 创建基础VirtIO设备
    console->dev = virtio_create_device(VIRTIO_ID_CONSOLE, base_addr, irq);
    if (!console->dev) {
        logger_error("Failed to create VirtIO console device\n");
        return NULL;
    }
    
    // 设置Console特定配置
    console->config.cols = 80;
    console->config.rows = 24;
    console->config.max_nr_ports = 1;
    console->config.emerg_wr = 0;
    
    // 设置设备特性
    console->dev->device_features = (1ULL << VIRTIO_CONSOLE_F_SIZE) |
                                   (1ULL << VIRTIO_CONSOLE_F_EMERG_WRITE);
    
    // 设置配置空间
    console->dev->config = &console->config;
    console->dev->config_len = sizeof(virtio_console_config_t);
    
    // 设置回调函数
    console->dev->queue_notify = console_queue_notify;
    console->dev->reset = console_reset;
    
    // 初始化缓冲区
    console->input_head = 0;
    console->input_tail = 0;
    console->output_head = 0;
    console->output_tail = 0;
    console->input_available = false;
    
    logger_info("VirtIO Console created: base=0x%llx, irq=%d\n", base_addr, irq);
    return console;
}

void virtio_console_destroy(virtio_console_t *console)
{
    if (!console) return;
    
    if (console->dev) {
        virtio_destroy_device(console->dev);
        console->dev = NULL;
    }
    
    memset(console, 0, sizeof(virtio_console_t));
}

// 处理发送队列（Guest -> Host）
// 会调用 vgic inject
static void console_handle_tx_queue(virtio_console_t *console)
{
    if (!console || !console->dev) return;
    
    virtqueue_t *vq = &console->dev->queues[VIRTIO_CONSOLE_TX_QUEUE];
    if (!vq->ready) return;
    
    virtq_desc_t desc_chain[16];
    uint32_t desc_count;
    
    while (virtio_queue_available(vq)) {
        int head_idx = virtio_queue_pop(vq, desc_chain, &desc_count);
        if (head_idx < 0) break;
        
        uint32_t total_len = 0;
        
        // 处理描述符链中的每个缓冲区
        for (uint32_t i = 0; i < desc_count; i++) {
            virtq_desc_t *desc = &desc_chain[i];
            
            // 只处理可读缓冲区（Guest发送的数据）
            if (!(desc->flags & VIRTQ_DESC_F_WRITE)) {
                // 这里应该从Guest物理地址读取数据
                // 简化实现：假设可以直接访问
                char *data = (char *)desc->addr;
                uint32_t len = desc->len;
                
                // 输出到Host控制台（简化实现）
                for (uint32_t j = 0; j < len; j++) {
                    if (data[j] != 0) {
                        // 这里可以输出到Host的控制台或日志
                        // 简化实现：存储到输出缓冲区
                        uint32_t next_tail = (console->output_tail + 1) % sizeof(console->output_buffer);
                        if (next_tail != console->output_head) {
                            console->output_buffer[console->output_tail] = data[j];
                            console->output_tail = next_tail;
                        }
                    }
                }
                
                total_len += len;
            }
        }
        
        // 将处理完的描述符放回已用环
        virtio_queue_push(vq, head_idx, total_len);
    }
    
    // 通知Guest
    virtio_queue_notify(console->dev, VIRTIO_CONSOLE_TX_QUEUE);
}

// 处理接收队列（Host -> Guest）
// 会调用 vgic inject
static void console_handle_rx_queue(virtio_console_t *console)
{
    if (!console || !console->dev) return;
    
    virtqueue_t *vq = &console->dev->queues[VIRTIO_CONSOLE_RX_QUEUE];
    if (!vq->ready) return;
    
    // 检查是否有输入数据
    if (console->input_head == console->input_tail) {
        return;  // 没有输入数据
    }
    
    virtq_desc_t desc_chain[16];
    uint32_t desc_count;
    uint32_t total_len = 0;

    while (virtio_queue_available(vq) && console->input_head != console->input_tail) {
        int head_idx = virtio_queue_pop(vq, desc_chain, &desc_count);
        if (head_idx < 0) break;

        uint32_t current_len = 0;
        
        // 处理描述符链中的每个缓冲区
        for (uint32_t i = 0; i < desc_count && console->input_head != console->input_tail; i++) {
            virtq_desc_t *desc = &desc_chain[i];
            
            // 只处理可写缓冲区（发送给Guest的数据）
            if (desc->flags & VIRTQ_DESC_F_WRITE) {
                char *data = (char *)desc->addr;
                uint32_t len = desc->len;
                uint32_t copied = 0;
                
                // 从输入缓冲区复制数据到Guest缓冲区
                while (copied < len && console->input_head != console->input_tail) {
                    data[copied] = console->input_buffer[console->input_head];
                    console->input_head = (console->input_head + 1) % sizeof(console->input_buffer);
                    copied++;
                }
                
                current_len += copied;
            }
        }

        total_len += current_len;

        // 将处理完的描述符放回已用环
        virtio_queue_push(vq, head_idx, current_len);
    }
    
    // 通知Guest
    if (total_len > 0) {
        virtio_queue_notify(console->dev, VIRTIO_CONSOLE_RX_QUEUE);
    }
}

// 队列通知回调
static void console_queue_notify(virtio_device_t *dev, uint32_t queue_idx)
{
    virtio_console_t *console = NULL;
    
    // 查找对应的console设备
    for (uint32_t i = 0; i < _console_count; i++) {
        if (_console_devices[i].dev == dev) {
            console = &_console_devices[i];
            break;
        }
    }
    
    if (!console) return;
    
    switch (queue_idx) {
        case VIRTIO_CONSOLE_RX_QUEUE:
            console_handle_rx_queue(console);
            break;
        case VIRTIO_CONSOLE_TX_QUEUE:
            console_handle_tx_queue(console);
            break;
        default:
            logger_warn("Unknown console queue index: %d\n", queue_idx);
            break;
    }
}

// 设备重置回调
static void console_reset(virtio_device_t *dev)
{
    virtio_console_t *console = NULL;
    
    // 查找对应的console设备
    for (uint32_t i = 0; i < _console_count; i++) {
        if (_console_devices[i].dev == dev) {
            console = &_console_devices[i];
            break;
        }
    }
    
    if (!console) return;
    
    // 重置缓冲区
    console->input_head = 0;
    console->input_tail = 0;
    console->output_head = 0;
    console->output_tail = 0;
    console->input_available = false;
    
    logger_info("VirtIO Console reset\n");
}

// 向Console输入数据（从Host到Guest）
void virtio_console_input(virtio_console_t *console, const char *data, uint32_t len)
{
    if (!console || !data) return;
    
    for (uint32_t i = 0; i < len; i++) {
        uint32_t next_tail = (console->input_tail + 1) % sizeof(console->input_buffer);
        if (next_tail != console->input_head) {
            console->input_buffer[console->input_tail] = data[i];
            console->input_tail = next_tail;
            console->input_available = true;
        }
    }
    
    // 尝试处理接收队列
    console_handle_rx_queue(console);
}

// 从Console读取输出数据（从Guest到Host）
uint32_t virtio_console_output(virtio_console_t *console, char *buffer, uint32_t max_len)
{
    if (!console || !buffer) return 0;
    
    uint32_t copied = 0;
    while (copied < max_len && console->output_head != console->output_tail) {
        buffer[copied] = console->output_buffer[console->output_head];
        console->output_head = (console->output_head + 1) % sizeof(console->output_buffer);
        copied++;
    }
    
    return copied;
}

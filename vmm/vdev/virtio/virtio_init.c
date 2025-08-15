#include "vmm/virtio.h"
#include "vmm/vm.h"
#include "io.h"

// 前向声明
typedef struct virtio_console virtio_console_t;

// VirtIO 设备基地址定义
#define VIRTIO_MMIO_BASE        0x0a000000ULL
#define VIRTIO_MMIO_SIZE        0x1000
#define VIRTIO_IRQ_BASE         48

// 每个VM的VirtIO设备配置
typedef struct virtio_vm_config {
    uint32_t console_enabled;
    uint32_t block_enabled;
    uint32_t net_enabled;
} virtio_vm_config_t;

// 默认配置
static virtio_vm_config_t default_config = {
    .console_enabled = 1,
    .block_enabled = 0,
    .net_enabled = 0,
};

// VirtIO Console 设备声明
extern virtio_console_t *virtio_console_create(uint64_t base_addr, uint32_t irq);

void virtio_vm_init(struct _vm_t *vm, uint32_t vm_id)
{
    if (!vm) {
        logger_error("Invalid VM for VirtIO initialization\n");
        return;
    }
    
    logger_info("Initializing VirtIO devices for VM %d\n", vm_id);
    
    // 为每个VM分配不同的设备地址空间
    uint64_t vm_base = VIRTIO_MMIO_BASE + (vm_id * 0x10000);
    uint32_t vm_irq_base = VIRTIO_IRQ_BASE + (vm_id * 8);
    
    uint32_t device_count = 0;
    
    // 创建 VirtIO Console 设备
    if (default_config.console_enabled) {
        uint64_t console_addr = vm_base + (device_count * VIRTIO_MMIO_SIZE);
        uint32_t console_irq = vm_irq_base + device_count;
        
        virtio_console_t *console = virtio_console_create(console_addr, console_irq);
        if (console) {
            // 通过设备地址查找设备并设置VM
            virtio_device_t *dev = virtio_find_device_by_addr(console_addr);
            if (dev) {
                dev->vm = vm;
            }
            logger_info("VM %d: VirtIO Console created at 0x%llx, IRQ %d\n",
                       vm_id, console_addr, console_irq);
            device_count++;
        } else {
            logger_error("VM %d: Failed to create VirtIO Console\n", vm_id);
        }
    }
    
    // 这里可以添加其他VirtIO设备的创建
    // 例如：VirtIO Block, VirtIO Network 等
    
    if (default_config.block_enabled) {
        // TODO: 创建 VirtIO Block 设备
        logger_info("VM %d: VirtIO Block device creation not implemented yet\n", vm_id);
    }
    
    if (default_config.net_enabled) {
        // TODO: 创建 VirtIO Network 设备
        logger_info("VM %d: VirtIO Network device creation not implemented yet\n", vm_id);
    }
    
    logger_info("VM %d: VirtIO initialization completed, %d devices created\n", 
               vm_id, device_count);
}

// 处理VirtIO MMIO访问的陷入
bool virtio_handle_mmio_trap(uint64_t fault_addr, uint32_t *value, bool is_write, uint32_t size)
{
    // 检查地址是否在VirtIO MMIO范围内
    if (fault_addr < VIRTIO_MMIO_BASE || fault_addr >= (VIRTIO_MMIO_BASE + 0x100000)) {
        return false;  // 不是VirtIO设备访问
    }
    
    // 调用VirtIO框架的处理函数
    return handle_virtio_mmio_access(fault_addr, value, is_write, size);
}

// 配置VirtIO设备
void virtio_configure_vm(uint32_t vm_id, bool enable_console, bool enable_block, bool enable_net)
{
    logger_info("Configuring VirtIO for VM %d: console=%d, block=%d, net=%d\n",
               vm_id, enable_console, enable_block, enable_net);
    
    // 这里可以根据需要动态配置每个VM的VirtIO设备
    // 目前使用全局默认配置
    default_config.console_enabled = enable_console ? 1 : 0;
    default_config.block_enabled = enable_block ? 1 : 0;
    default_config.net_enabled = enable_net ? 1 : 0;
}

// 获取VirtIO设备信息
void virtio_print_device_info(void)
{
    logger_info("=== VirtIO Device Information ===\n");
    logger_info("MMIO Base Address: 0x%llx\n", VIRTIO_MMIO_BASE);
    logger_info("Device Size: 0x%x\n", VIRTIO_MMIO_SIZE);
    logger_info("IRQ Base: %d\n", VIRTIO_IRQ_BASE);
    logger_info("Default Configuration:\n");
    logger_info("  Console: %s\n", default_config.console_enabled ? "Enabled" : "Disabled");
    logger_info("  Block: %s\n", default_config.block_enabled ? "Enabled" : "Disabled");
    logger_info("  Network: %s\n", default_config.net_enabled ? "Enabled" : "Disabled");
    logger_info("================================\n");
}

// VirtIO 子系统初始化
void virtio_subsystem_init(void)
{
    logger_info("Initializing VirtIO subsystem...\n");
    
    // 初始化VirtIO全局状态
    virtio_global_init();
    
    // 打印设备信息
    virtio_print_device_info();
    
    logger_info("VirtIO subsystem initialization completed\n");
}

// 示例：向VirtIO Console输入数据
void virtio_console_input_example(uint32_t vm_id, const char *text)
{
    // 这是一个示例函数，展示如何向VirtIO Console输入数据
    // 实际使用中，这个函数可能会被键盘中断处理程序调用
    
    if (!text) return;
    
    logger_info("Sending text to VM %d console: '%s'\n", vm_id, text);
    
    // 查找对应VM的Console设备
    uint64_t console_addr = VIRTIO_MMIO_BASE + (vm_id * 0x10000);
    virtio_device_t *dev = virtio_find_device_by_addr(console_addr);
    
    if (dev && dev->device_id == VIRTIO_ID_CONSOLE) {
        // 找到Console设备，向其输入数据
        // 这里需要实现具体的输入逻辑
        logger_info("Found VirtIO Console for VM %d, sending input\n", vm_id);
        
        // TODO: 实现具体的输入数据传递
        // virtio_console_input(console, text, strlen(text));
    } else {
        logger_warn("VirtIO Console not found for VM %d\n", vm_id);
    }
}

// 调试函数：列出所有VirtIO设备
void virtio_list_all_devices(void)
{
    logger_info("=== VirtIO Device List ===\n");
    
    // 这里需要遍历所有设备
    // 由于设备数组是私有的，我们需要通过地址范围来查找
    for (uint32_t vm_id = 0; vm_id < VM_NUM_MAX; vm_id++) {
        for (uint32_t dev_idx = 0; dev_idx < 8; dev_idx++) {
            uint64_t addr = VIRTIO_MMIO_BASE + (vm_id * 0x10000) + (dev_idx * VIRTIO_MMIO_SIZE);
            virtio_device_t *dev = virtio_find_device_by_addr(addr);
            
            if (dev) {
                const char *type_name = "Unknown";
                switch (dev->device_id) {
                    case VIRTIO_ID_NET: type_name = "Network"; break;
                    case VIRTIO_ID_BLOCK: type_name = "Block"; break;
                    case VIRTIO_ID_CONSOLE: type_name = "Console"; break;
                    case VIRTIO_ID_RNG: type_name = "RNG"; break;
                    case VIRTIO_ID_BALLOON: type_name = "Balloon"; break;
                    case VIRTIO_ID_SCSI: type_name = "SCSI"; break;
                    case VIRTIO_ID_9P: type_name = "9P"; break;
                    case VIRTIO_ID_GPU: type_name = "GPU"; break;
                }
                
                logger_info("VM %d Device %d: %s (ID=%d) at 0x%llx, IRQ=%d, Status=0x%x\n",
                           vm_id, dev_idx, type_name, dev->device_id, 
                           dev->base_addr, dev->irq, dev->status);
            }
        }
    }
    
    logger_info("=========================\n");
}

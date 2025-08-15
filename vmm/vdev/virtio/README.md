# VirtIO 设备框架

这个目录包含了 Avatar 虚拟机监控器的 VirtIO 设备实现。

## 文件结构

```
vmm/vdev/virtio/
├── README.md           # 本文件
├── virtio.c           # VirtIO 核心实现
├── virtqueue.c        # VirtQueue 操作实现
├── virtio_console.c   # VirtIO Console 设备
├── virtio_init.c      # VirtIO 初始化和配置
└── (future files)     # 其他 VirtIO 设备实现
```

## 核心组件

### 1. VirtIO 核心 (virtio.c)
- VirtIO 设备的基础结构和 MMIO 访问处理
- 设备创建、销毁和管理
- 静态内存池管理（避免使用 malloc）

### 2. VirtQueue 操作 (virtqueue.c)
- VirtQueue 的初始化、重置和操作
- 描述符链的处理
- 中断注入和通知机制

### 3. VirtIO Console (virtio_console.c)
- 虚拟串口设备实现
- 支持 Guest 和 Host 之间的字符数据传输
- 示例设备，展示如何实现具体的 VirtIO 设备

### 4. 初始化框架 (virtio_init.c)
- VM 级别的 VirtIO 设备初始化
- 设备配置和地址分配
- MMIO 陷入处理

## 内存布局

### 设备地址分配
- 基地址：0x0a000000
- 每个 VM：64KB 地址空间 (0x10000)
- 每个设备：4KB 地址空间 (0x1000)
- VM0 设备：0x0a000000 - 0x0a00ffff
- VM1 设备：0x0a010000 - 0x0a01ffff
- ...

### 中断分配
- 基础 IRQ：48
- 每个 VM：8 个 IRQ
- VM0 IRQ：48-55
- VM1 IRQ：56-63
- ...

## 使用方法

### 1. 初始化 VirtIO 子系统

```c
#include "vmm/virtio.h"

// 在系统启动时调用
void system_init(void) {
    virtio_subsystem_init();
}
```

### 2. 为 VM 创建 VirtIO 设备

```c
// 在创建 VM 时调用
void create_vm(uint32_t vm_id) {
    vm_t *vm = create_vm_instance(vm_id);
    
    // 初始化 VirtIO 设备
    virtio_vm_init(vm, vm_id);
}
```

### 3. 处理 MMIO 陷入

```c
// 在 Stage-2 页面错误处理中调用
bool handle_mmio_fault(uint64_t fault_addr, uint32_t *value, bool is_write, uint32_t size) {
    // 尝试 VirtIO 处理
    if (virtio_handle_mmio_trap(fault_addr, value, is_write, size)) {
        return true;  // VirtIO 处理了这个访问
    }
    
    // 其他设备处理...
    return false;
}
```

### 4. 配置设备

```c
// 配置 VM 的 VirtIO 设备
virtio_configure_vm(0, true, false, false);  // VM0: 只启用 Console
virtio_configure_vm(1, true, true, false);   // VM1: 启用 Console 和 Block
```

## 添加新的 VirtIO 设备

### 1. 创建设备文件
创建 `virtio_<device>.c` 文件，实现设备特定的逻辑。

### 2. 实现必要的回调函数
```c
static void device_queue_notify(virtio_device_t *dev, uint32_t queue_idx);
static void device_reset(virtio_device_t *dev);
```

### 3. 创建设备实例
```c
virtio_device_t *virtio_device_create(uint64_t base_addr, uint32_t irq) {
    virtio_device_t *dev = virtio_create_device(VIRTIO_ID_DEVICE, base_addr, irq);
    if (!dev) return NULL;
    
    // 设置设备特定配置
    dev->device_features = /* 设备特性 */;
    dev->config = /* 配置空间 */;
    dev->config_len = /* 配置空间大小 */;
    dev->queue_notify = device_queue_notify;
    dev->reset = device_reset;
    
    return dev;
}
```

### 4. 在初始化中添加设备创建
在 `virtio_init.c` 的 `virtio_vm_init` 函数中添加新设备的创建逻辑。

## 调试功能

### 列出所有设备
```c
virtio_list_all_devices();
```

### 打印设备信息
```c
virtio_print_device_info();
```

### 调试特定设备
```c
virtio_debug_device(dev);
```

## 注意事项

1. **内存管理**：使用静态内存池，避免 malloc/free
2. **中断处理**：使用现有的 vgic_inject_spi 函数
3. **地址映射**：需要在 Stage-2 页表中正确映射 MMIO 地址
4. **线程安全**：当前实现假设单线程访问，多核环境需要添加锁

## 未来扩展

- [ ] VirtIO Block 设备
- [ ] VirtIO Network 设备
- [ ] VirtIO GPU 设备
- [ ] VirtIO 9P 文件系统
- [ ] VirtIO Balloon 内存管理
- [ ] 多队列支持
- [ ] 事件通知优化

## 参考资料

- [VirtIO Specification v1.1](https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html)
- [QEMU VirtIO Implementation](https://github.com/qemu/qemu/tree/master/hw/virtio)
- [Linux VirtIO Driver](https://github.com/torvalds/linux/tree/master/drivers/virtio)

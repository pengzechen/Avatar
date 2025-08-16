# Avatar VirtIO Block 前端驱动

## 概述

这是为 Avatar 虚拟机监控器实现的 VirtIO Block 前端驱动，用于与 QEMU 等虚拟化平台通信，获取存储数据并传递给 Guest 虚拟机。

## 架构

```
┌─────────────────┐    ┌─────────────────┐
│   Guest VM 1    │    │   Guest VM 2    │
│                 │    │                 │
│ VirtIO Block    │    │ VirtIO Block    │
│ Driver          │    │ Driver          │
└─────────┬───────┘    └─────────┬───────┘
          │                      │
          │    VirtIO MMIO       │
          │                      │
┌─────────▼──────────────────────▼───────┐
│         Avatar Hypervisor              │
│                                        │
│  ┌─────────────────────────────────┐   │
│  │      VMM VirtIO Backend         │   │
│  │                                 │   │
│  │  ┌─────────────────────────┐    │   │
│  │  │ VirtIO Frontend Driver  │    │   │ ← 这个实现
│  │  └─────────────────────────┘    │   │
│  └─────────────────────────────────┘   │
└────────────────┬───────────────────────┘
                 │ VirtIO MMIO
┌────────────────▼───────────────────────┐
│              QEMU                      │
│         VirtIO Backend                 │
│      (虚拟磁盘文件)                     │
└────────────────────────────────────────┘
```

## 文件结构

- `include/virtio_block_frontend.h` - 头文件，定义数据结构和接口
- `virtio_block_frontend.c` - 核心实现，VirtIO 协议和队列操作
- `virtio_block_example.c` - 使用示例和集成接口
- `virtio_block_test.c` - 测试代码
- `virtio_frontend_Makefile` - 编译配置

## 主要特性

### ✅ **使用项目现有基础设施**
- 使用 `mmio.h` 中的安全 MMIO 操作函数
- 使用 `mem/barrier.h` 中的内存屏障定义
- 使用 `io.h` 中的日志系统
- 使用项目的类型定义和编码风格

### ✅ **静态内存管理**
```c
// 预分配的内存池，避免动态分配
static uint8_t virtio_memory_pool[...] __attribute__((aligned(4096)));
static uint8_t virtio_buffer_pool[...] __attribute__((aligned(16)));
```

### ✅ **完整的 VirtIO 1.0/2.0 支持**
- 设备发现和初始化
- 特性协商
- 队列管理（描述符、可用环、已用环）
- 轮询模式操作（无中断）

### ✅ **与 VMM 后端集成**
```c
// 为 VMM 后端提供的接口
int vmm_backend_read_from_host_storage(uint64_t sector, void *buffer, uint32_t count);
int vmm_backend_write_to_host_storage(uint64_t sector, const void *buffer, uint32_t count);
int vmm_backend_get_storage_info(uint64_t *total_sectors, uint32_t *sector_size);
```

## 使用方法

### 1. 编译

```bash
# 使用提供的 Makefile
make -f virtio_frontend_Makefile

# 或集成到主项目 Makefile
# 在主 Makefile 中添加源文件到 SRC_DIRS
```

### 2. 初始化

```c
#include "virtio_block_frontend.h"

int main(void) {
    // ... 其他初始化 ...
    
    // 初始化 VirtIO Block 前端
    if (avatar_virtio_block_init() < 0) {
        logger_error("Failed to initialize VirtIO Block\n");
        return -1;
    }
    
    // ... 继续初始化 ...
}
```

### 3. 在 VMM 后端中使用

```c
// 在你的 VMM VirtIO 后端实现中
#include "virtio_block_frontend.h"

// 处理 Guest 的读请求
int handle_guest_read_request(uint64_t sector, void *buffer, uint32_t count) {
    // 从 QEMU 读取数据
    return vmm_backend_read_from_host_storage(sector, buffer, count);
}

// 处理 Guest 的写请求
int handle_guest_write_request(uint64_t sector, const void *buffer, uint32_t count) {
    // 写入到 QEMU
    return vmm_backend_write_to_host_storage(sector, buffer, count);
}
```

## 配置

### 内存池大小

```c
// 在 include/virtio_block_frontend.h 中调整
#define VIRTIO_MAX_DEVICES      4       // 最大设备数
#define VIRTIO_QUEUE_SIZE       16      // 队列大小
#define VIRTIO_BUFFER_POOL_SIZE 64      // 缓冲区池大小
#define VIRTIO_BUFFER_SIZE      4096    // 单个缓冲区大小
```

### 设备扫描地址

```c
// 在 virtio_block_frontend.c 中的 scan_for_virtio_block_device() 函数
uint64_t virtio_base_addresses[] = {
    0x0a000000,  // QEMU ARM virt 平台默认地址
    0x10001000,  // 其他可能的地址
    // 根据你的平台添加更多地址
    0
};
```

## 测试

### 运行基本测试

```c
// 在初始化后调用
if (avatar_virtio_block_test() < 0) {
    logger_error("VirtIO Block test failed\n");
}
```

### 查看设备状态

```c
// 打印设备信息
avatar_virtio_block_print_status();

// 获取设备信息
uint64_t capacity;
uint32_t block_size;
avatar_virtio_block_get_info(&capacity, &block_size);
```

## 调试

### 启用调试日志

在编译时定义调试宏：
```c
#define VIRTIO_DEBUG 1
```

### 常见问题

1. **设备未找到**
   - 检查 QEMU 是否正确配置了 VirtIO Block 设备
   - 确认设备地址是否正确

2. **内存分配失败**
   - 增加 `VIRTIO_BUFFER_POOL_SIZE`
   - 检查内存池是否足够大

3. **I/O 操作超时**
   - 检查 QEMU 后端是否正常工作
   - 确认 MMIO 地址映射是否正确

## 性能优化

### 批量操作

当前实现是逐扇区操作，可以优化为批量操作：

```c
// 优化前（当前实现）
for (uint32_t i = 0; i < sector_count; i++) {
    virtio_blk_read_sector(dev, sector + i, buffer + i * 512, 1);
}

// 优化后（可以实现）
virtio_blk_read_sectors(dev, sector, buffer, sector_count);
```

### 异步操作

当前使用轮询模式，可以扩展支持中断模式以提高性能。

## 集成到主项目

### 1. 修改主 Makefile ✅ 已完成

```makefile
# 已添加到 SRC_DIRS
SRC_DIRS := . boot exception io mem timer task process spinlock \
            vmm lib fs app syscall virtio_frontend

# 已添加编译规则
$(BUILD_DIR)/%.o: virtio_frontend/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@
```

### 2. 在主程序中初始化

```c
// 在 main.c 中
#include "virtio_block_frontend.h"

int main(void) {
    // ... 现有初始化代码 ...
    
    // 初始化 VirtIO 前端
    avatar_virtio_block_init();
    
    // ... 继续其他初始化 ...
}
```

### 3. 在 VMM 中使用

在你的 `vmm/vdev/virtio/virtio_block.c` 后端实现中，调用前端接口获取数据。

## 未来扩展

- [ ] 支持多个 VirtIO Block 设备
- [ ] 实现异步 I/O 和中断模式
- [ ] 添加错误恢复机制
- [ ] 支持 VirtIO 1.1 新特性
- [ ] 性能监控和统计

## 注意事项

1. **内存对齐**：队列内存必须 4KB 对齐
2. **内存屏障**：正确使用内存屏障确保操作顺序
3. **错误处理**：始终检查返回值
4. **线程安全**：当前实现假设单线程使用

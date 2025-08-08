# Avatar 操作系统配置文档

本文档描述了 Avatar 操作系统中所有可配置的参数，这些参数之前是硬编码在代码中的，现在已经统一移到配置文件中。

## 配置文件结构

- `include/os_cfg.h` - 主要系统配置
- `include/hyper/hyper_cfg.h` - 虚拟化相关配置

## 主要配置参数

### 1. SMP 配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `SMP_NUM` | 1 | CPU 核心数量 |
| `STACK_SIZE` | 16KB | 每个 CPU 的栈大小 |
| `SECONDARY_CPU_COUNT` | SMP_NUM-1 | 从核数量 |
| `CPU_STARTUP_WAIT_LOOPS` | 10 | CPU 启动等待循环次数 |
| `CPU_STARTUP_INNER_LOOPS` | 0xfffff | 内层等待循环次数 |

### 2. 定时器配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `TIMER_FREQUENCY_HZ` | 62500000 | 定时器频率 (62.5MHz) |
| `TIMER_TICK_INTERVAL_MS` | 10 | 定时器中断间隔 (10ms) |
| `TIMER_TVAL_VALUE` | 625000 | 定时器计数值 |
| `TIMER_VECTOR` | 30 | 定时器中断向量 |
| `HV_TIMER_VECTOR` | 27 | 虚拟化定时器中断向量 |

**计算公式**: `TIMER_TVAL_VALUE = TIMER_FREQUENCY_HZ * TIMER_TICK_INTERVAL_MS / 1000`

### 3. 内存配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `KERNEL_RAM_START` | 0x40000000 | 内核 RAM 起始地址 |
| `KERNEL_RAM_SIZE` | 0xc0000000 | 内核 RAM 大小 (4GB) |
| `PAGE_SIZE` | 4096 | 页面大小 |
| `HEAP_OFFSET` | 0x900000 | 堆偏移量 (9MB) |
| `GUEST_RAM_START` | 0x40000000 | Guest RAM 起始地址 |
| `GUEST_RAM_SIZE` | 0x40000000 | Guest RAM 大小 (1GB) |

### 4. 设备地址配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `UART0_BASE_ADDR` | 0x09000000 | UART0 基地址 |
| `GICD_BASE_ADDR` | 0x8000000 | GIC 分发器基地址 |
| `GICC_BASE_ADDR` | 0x8010000 | GIC CPU 接口基地址 |
| `GICH_BASE_ADDR` | 0x8030000 | GIC 虚拟化接口基地址 |
| `GICV_BASE_ADDR` | 0x8040000 | GIC 虚拟 CPU 接口基地址 |

### 5. 中断配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `PL011_INT` | 33 | PL011 UART 中断号 |
| `VIRTUAL_TIMER_IRQ` | 27 | 虚拟定时器中断号 |

### 6. 任务和调度配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `MAX_TASKS` | 64 | 最大任务数 |
| `TASK_PRIORITY_LEVELS` | 8 | 任务优先级级别数 |
| `TASK_PRIORITY_DEFAULT` | 4 | 默认任务优先级 |
| `SYS_TASK_TICK` | 10 | 系统任务时间片 |
| `SCHEDULE_TIME_SLICE_MS` | 100 | 调度时间片 (ms) |
| `SCHEDULE_PREEMPT_THRESHOLD` | 5 | 抢占阈值 |

### 7. 虚拟化配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `VM_NUM_MAX` | 4 | 最大虚拟机数量 |
| `VCPU_NUM_MAX` | 8 | 最大虚拟 CPU 数量 |
| `VM_STACK_PAGES` | 2 | 每个 vCPU 栈页数 |
| `VM_STACK_SIZE` | 8192 | 虚拟机栈大小 |
| `PRIMARY_VCPU_PCPU_MASK` | (1<<0) | 主 vCPU 绑定掩码 |
| `SECONDARY_VCPU_PCPU_MASK` | (1<<1) | 从 vCPU 绑定掩码 |

### 8. 虚拟机内存布局

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `VM0_BIN_LOADADDR` | 0x40080000 | VM0 二进制加载地址 |
| `VM0_DTB_LOADADDR` | 0x44000000 | VM0 设备树加载地址 |
| `VM0_FS_LOADADDR` | 0x45000000 | VM0 文件系统加载地址 |
| `VM0_SMP_NUM` | 1 | VM0 SMP 数量 |
| `VM1_BIN_LOADADDR` | 0x48080000 | VM1 二进制加载地址 |
| `VM1_DTB_LOADADDR` | 0x4c000000 | VM1 设备树加载地址 |
| `VM1_FS_LOADADDR` | 0x4d000000 | VM1 文件系统加载地址 |
| `VM1_SMP_NUM` | 1 | VM1 SMP 数量 |

### 9. MMIO 配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `MMIO_PAGES_GICD` | 16 | GICD MMIO 页数 |
| `MMIO_PAGES_GICC` | 16 | GICC MMIO 页数 |
| `MMIO_PAGES_PL011` | 1 | PL011 MMIO 页数 |
| `MMIO_PAGE_SIZE` | 0x1000 | MMIO 页大小 |

### 10. 页表配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `PAGE_TABLE_MAX_ENTRIES_L0` | 1 | L0 页表最大条目数 |
| `PAGE_TABLE_MAX_ENTRIES_L1` | 4 | L1 页表最大条目数 |
| `PAGE_TABLE_MAX_ENTRIES_L2` | 512 | L2 页表最大条目数 |
| `PAGE_TABLE_MAX_ENTRIES_L3` | 512 | L3 页表最大条目数 |

### 11. 测试和调试配置

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `TEST_MEM_ADDR` | 0x9000000 | 测试内存地址 |
| `TEST_MEM_MASK` | 97 | 测试内存掩码 |
| `MMIO_ARREA` | 0x50000000 | 测试 MMIO 区域 |
| `DEVICE_MEM_START` | 0x8000000 | 设备内存起始地址 |
| `DEVICE_MEM_END` | 0xa000000 | 设备内存结束地址 |

## 配置修改指南

### 修改定时器频率

如果要修改定时器中断频率，需要同时修改：
1. `TIMER_TICK_INTERVAL_MS` - 设置期望的中断间隔
2. `TIMER_TVAL_VALUE` 会自动计算

例如，改为 1ms 中断：
```c
#define TIMER_TICK_INTERVAL_MS 1    // 1ms
// TIMER_TVAL_VALUE 自动变为 62500
```

### 修改 SMP 配置

修改 CPU 数量时，确保：
1. 栈空间足够：检查 `SECONDARY_STACK_TOTAL` 计算
2. 虚拟机配置匹配：调整 `VM0_SMP_NUM` 等

### 修改内存布局

修改内存配置时注意：
1. 地址对齐：确保所有地址都是页对齐的
2. 地址不重叠：检查各个内存区域不重叠
3. 大小合理：确保内存大小满足系统需求

## 配置验证

使用 `config_test.c` 中的函数来验证配置：

```c
validate_all_configs();  // 验证所有配置
print_config_summary();  // 打印配置摘要
```

这些函数会检查：
- 配置参数的合理性
- 地址对齐和重叠
- 计算公式的正确性
- 潜在的配置冲突

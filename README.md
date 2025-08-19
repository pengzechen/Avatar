# Avatar

> A lightweight and high-performance Hypervisor and Kernel for ARM64 (AArch64) platforms.

**Avatar** 是一个面向 ARM64 架构的轻量级虚拟化管理程序 (Hypervisor) 以及自主内核，实现了多虚拟机（VM）并发运行，支持多核 SMP。项目兼顾裸机性能和虚拟化灵活性，适合嵌入式和云计算场景。

---

## 🚀 VMM (Virtual Machine Monitor)

Avatar VMM 是一个高性能的虚拟化管理程序，运行在 ARM64 EL2 特权级别，提供完整的虚拟化支持。

### 核心特性

- **多虚拟机管理**：支持同时运行多个 Guest 操作系统
- **动态 Guest 加载**：从文件系统动态加载 VM 镜像，无需重新编译
- **多核 SMP 支持**：支持多虚拟 CPU 调度和负载均衡
- **异常虚拟化**：完整的 EL2 异常向量和中断处理
- **内存虚拟化**：Stage-2 地址翻译和内存隔离
- **设备虚拟化**：虚拟设备和设备透传支持

### VMM 架构

```
┌─────────────────────────────────────────┐
│              Avatar VMM (EL2)           │
├─────────────────────────────────────────┤
│  Guest Management  │  Resource Manager  │
│  • Guest Loader    │  • Memory Manager  │
│  • vCPU Scheduler  │  • Interrupt Ctrl  │
│  • Guest Monitor   │  • Device Manager  │
├─────────────────────────────────────────┤
│            File System Layer            │
│  • FAT32 Support  │  • Guest Manifests │
├─────────────────────────────────────────┤
│               Hardware (EL1)            │
└─────────────────────────────────────────┘
```

### 支持的 Guest 类型

- **Linux Guest**: 完整的 Linux 内核，支持设备树和 initrd
- **NimbOS Guest**: Rust 编写的实时操作系统
- **TestOS Guest**: 简单的测试操作系统

### Guest 配置系统

- **基于 Manifest**: 通过 `guest/guest_manifests.c` 配置 Guest 参数
- **灵活配置**: 支持加载地址、SMP 核心数、依赖文件等配置
- **运行时管理**: 支持 Guest 的启动、停止、监控等操作

---

## 🔧 Kernel

Avatar Kernel 是一个自主设计的微内核，提供基础的操作系统服务和系统调用接口。

### 核心特性

- **任务调度**：抢占式多任务调度器，支持优先级调度
- **内存管理**：页式内存管理，支持虚拟内存和物理内存分配
- **同步原语**：互斥锁、信号量、条件变量等同步机制
- **中断处理**：完整的中断控制器支持和中断处理框架
- **系统调用**：标准的系统调用接口和用户态支持
- **文件系统**：FAT32 文件系统实现，支持文件和目录操作

### Kernel 架构

```
┌─────────────────────────────────────────┐
│           Application Layer             │
├─────────────────────────────────────────┤
│            System Call API             │
├─────────────────────────────────────────┤
│              Avatar Kernel              │
│  ┌─────────────┬─────────────────────┐  │
│  │   Scheduler │    Memory Manager   │  │
│  │   • Tasks   │    • Page Alloc    │  │
│  │   • SMP     │    • Virtual Mem   │  │
│  ├─────────────┼─────────────────────┤  │
│  │ Sync Prims  │    File System     │  │
│  │ • Mutex     │    • FAT32         │  │
│  │ • Semaphore │    • VFS           │  │
│  ├─────────────┼─────────────────────┤  │
│  │ Interrupt   │    Device Drivers   │  │
│  │ • GIC       │    • UART          │  │
│  │ • Timer     │    • Storage       │  │
│  └─────────────┴─────────────────────┘  │
├─────────────────────────────────────────┤
│              Hardware (EL1)             │
└─────────────────────────────────────────┘
```

### 内核模块

- **调度器**: 支持多核 SMP 和优先级调度
- **内存管理**: 页式内存管理和内存池分配
- **文件系统**: 完整的 FAT32 实现
- **设备驱动**: UART、存储设备等基础驱动
- **同步机制**: 完整的同步原语实现

---


## 🛠️ 构建和运行

### 环境要求

- **工具链**: AArch64 交叉编译工具链
- **模拟器**: QEMU (支持 ARM64 虚拟化)
- **系统**: Linux 开发环境
- **权限**: sudo 权限（用于文件系统操作）

### 快速开始

#### 1. 克隆项目
```bash
git clone https://github.com/pengzechen/Avatar.git
cd Avatar
git submodule update --init --recursive

# 或者一次性克隆
git clone --recurse-submodules https://github.com/pengzechen/Avatar.git
```

#### 2. 构建 Guest 镜像
```bash
# 构建应用程序
make app

# 构建 TestOS Guest
cd guest/testos/
mkdir -p build
make SMP=1 GUEST_LABEL='[TestOS] ' LOAD_ADDR=0x60200000
cd ../../

# 构建 NimbOS Guest
cd guest/nimbos/kernel/
make build ARCH=aarch64
cd ../../../

# Linux Guest 镜像需要单独准备（可选）
# 将 linux.bin, linux.dtb, initrd.gz 放置在 guest/linux/ 目录下
```

#### 3. 设置文件系统
```bash
# 创建 host.img 文件系统镜像
./scripts/create_host_img.sh

# 将 Guest 文件拷贝到文件系统中
sudo ./scripts/setup_guest_files.sh

# 查看帮助信息
./scripts/setup_guest_files.sh -h
```

#### 4. 编译和运行

**运行 VMM (虚拟化模式)**
```bash
# 编译 Avatar VMM
make clean && make

# 运行虚拟机监控器
make SMP=2 HV=1 run
```

**运行 Kernel (原生模式)**
```bash
# 编译 Avatar Kernel
make clean && make

# 运行内核
make SMP=2 run
```

### VMM 管理命令

在 Avatar VMM shell 中可以使用以下命令管理 Guest：

```bash
# Guest 管理
guest list              # 列出可用的 Guest
guest info <id>         # 显示 Guest 详细信息
guest validate <id>     # 验证 Guest 文件
guest start <id>        # 启动指定的 Guest

# 文件系统操作
fs ls                   # 列出文件系统内容
fs cat <file>           # 查看文件内容
fs info                 # 显示文件系统信息

# 系统信息
help                    # 显示帮助信息
version                 # 显示版本信息
```

---

## 📁 项目结构

```
Avatar/
├── vmm/                    # VMM 虚拟化管理程序
│   ├── src/               # VMM 源代码
│   ├── include/           # VMM 头文件
│   └── guest/             # Guest 管理模块
├── kernel/                # Avatar Kernel
│   ├── src/               # 内核源代码
│   ├── include/           # 内核头文件
│   ├── mm/                # 内存管理
│   ├── sched/             # 调度器
│   ├── fs/                # 文件系统
│   └── drivers/           # 设备驱动
├── guest/                 # Guest 操作系统
│   ├── linux/             # Linux Guest
│   ├── nimbos/            # NimbOS Guest
│   ├── testos/            # TestOS Guest
│   └── guest_manifests.c  # Guest 配置文件
├── scripts/               # 构建和部署脚本
│   ├── create_host_img.sh # 创建文件系统镜像
│   └── setup_guest_files.sh # 设置 Guest 文件
├── app/                   # 用户态应用程序
└── tools/                 # 开发工具
```

---

## 🔧 配置说明

### Guest 配置

Guest 配置通过 `guest/guest_manifests.c` 文件管理：

```c
// Guest 配置示例
static guest_manifest_t guest_manifests[] = {
    {
        .id = 0,
        .name = "Linux",
        .kernel_path = "/guests/linux/linux.bin",
        .dtb_path = "/guests/linux/linux.dtb",
        .initrd_path = "/guests/linux/initrd.gz",
        .load_addr = 0x60080000,
        .smp_cores = 1,
        .memory_size = 0x8000000,  // 128MB
    },
    // 更多 Guest 配置...
};
```

### 文件系统结构

```
host.img:/
└── guests/
    ├── linux/
    │   ├── linux.bin      # Linux 内核镜像
    │   ├── linux.dtb      # 设备树文件
    │   └── initrd.gz      # 初始化文件系统
    ├── nimbos/
    │   └── nimbos.bin     # NimbOS 内核镜像
    └── testos/
        └── testos.bin     # TestOS 内核镜像
```

---

## 📖 开发指南

### 添加新的 Guest

1. **准备 Guest 镜像**: 编译生成 Guest 内核镜像
2. **更新配置**: 在 `guest_manifests.c` 中添加 Guest 配置
3. **拷贝文件**: 使用脚本将文件拷贝到 `host.img`
4. **测试运行**: 使用 VMM 命令启动和测试 Guest

### 扩展内核功能

1. **添加系统调用**: 在 `kernel/src/syscall/` 中实现新的系统调用
2. **添加设备驱动**: 在 `kernel/drivers/` 中实现设备驱动
3. **扩展文件系统**: 在 `kernel/fs/` 中添加新的文件系统支持

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目采用 MIT 许可证。

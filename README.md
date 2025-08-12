# Avatar

> A lightweight and high-performance Hypervisor and Kernel for ARM64 (AArch64) platforms.

---

## 项目简介

**Avatar** 是一个面向 ARM64 架构的轻量级虚拟化管理程序 (Hypervisor) 以及自主内核，实现了多虚拟机（VM）并发运行，支持多核 SMP。项目兼顾裸机性能和虚拟化灵活性，适合嵌入式和云计算场景。

---

## 特性

- 支持多虚拟机 (VM) 启动与管理
- 支持多核 SMP，多虚拟 CPU 调度
- 独立内核设计，支持基本任务调度与同步原语
- 异常向量及中断处理支持（EL2）
- 简单易扩展的虚拟机镜像加载机制
- 完全自主实现，摆脱标准库依赖
- 适合裸机环境，提供底层工具链支持

---

## 架构设计

- **Hypervisor 层**：管理多个 VM，负责虚拟 CPU 调度和资源隔离
- **内核层**：任务调度、内存管理、同步机制、异常中断处理
- **启动流程**：基于 ARM PSCI 启动多核，加载 VM 镜像和设备树

---


## 运行

### step1 clone
```bash
git clone https://github.com/pengzechen/Avatar.git
cd Avatar
git submodule update --init --recursive

# or

git clone --recurse-submodules https://github.com/pengzechen/Avatar.git
```

### step2 make application and guest
```bash
make app
cd guest/testos/
mkdir build
make SMP=2 GUEST_LABEL='[guest:0] ' LOAD_ADDR=0x70200000
mv build/kernel.bin build/guest0_kernel.bin
make SMP=4 GUEST_LABEL='[guest:1] ' LOAD_ADDR=0x50200000
mv build/kernel.bin build/guest1_kernel.bin
cd ../../
```

### step3 compile and run
```bash
# run as a vmm
make SMP=2 HV=1 run

# run as a kernel
make SMP=2 HV=0 run
```

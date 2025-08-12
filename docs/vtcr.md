在 AArch64 架构中，VTCR_EL2（Virtualization Translation Control Register）是一个虚拟化相关的寄存器，用于控制第二级地址转换的配置。具体来说，它主要用于配置虚拟机的内存管理单元（MMU），以便在 Hypervisor 中管理 Guest 虚拟机的内存地址转换。

# VTCR_EL2 的作用
VTCR_EL2 控制了 EL1 和 EL0 在虚拟化环境下的内存地址转换的行为。其主要作用包括：

配置地址转换的大小：定义第二级地址转换的虚拟地址空间的大小。
设置内存属性：配置内存类型、访问权限等属性。
控制转换的粒度：确定页面表条目的大小和对齐方式。
设置翻译表的基础地址：指定用于第二级地址转换的页表的基地址。
VTCR_EL2 的位字段
以下是 VTCR_EL2 寄存器的位字段说明：

## SL0 (Bits[5:6])：指定第二级转换表的初始级别。
00: 从第一级转换表开始
01: 从第二级转换表开始
10: 从第三级转换表开始
11: 从第四级转换表开始（如果实现）

## IRGN0 (Bits[8:9])：内部缓存属性。
00: Non-cacheable
01: Write-back, Write-allocate
10: Write-through, No Write-allocate
11: Write-back, No Write-allocate

## ORGN0 (Bits[10:11])：外部缓存属性。
00: Non-cacheable
01: Write-back, Write-allocate
10: Write-through, No Write-allocate
11: Write-back, No Write-allocate

## SH0 (Bits[12:13])：共享属性。
00: Non-shareable
01: Reserved
10: Outer Shareable
11: Inner Shareable

## TG0 (Bits[14:15])：页大小。
00: 4KB
01: 64KB
10: 16KB
11: Reserved

## PS (Bits[16:18])：物理地址大小。
000: 32 bits (4GB)
001: 36 bits (64GB)
010: 40 bits (1TB)
011: 42 bits (4TB)
100: 44 bits (16TB)
101: 48 bits (256TB)
110: 52 bits (4PB)
111: Reserved

## VS (Bits[19:20])：虚拟地址大小。
00: 48 bits
01: 40 bits
10: 36 bits
11: Reserved

## T0SZ (Bits[0:5])：指定转换表 0 的大小。该字段的值等于 64 减去转换表的虚拟地址空间大小的对数。
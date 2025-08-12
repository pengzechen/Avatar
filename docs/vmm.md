

# sctlr_el2  

M (第0位): MMU 启用
C (第2位): 数据缓存启用
I (第12位): 指令缓存启用

# HCR_EL2（Hypervisor Configuration Register）


# VTTBR_EL2（Virtualization Translation Table Base Register）。


# CNTHCTL_EL2 寄存器位分布
CNTHCTL_EL2寄存器的各个位的作用如下：

EL1PCTEN (bit 0): EL1 Physical Counter Timer Enable

1: 允许EL1和EL0访问物理计数器寄存器（CNTPCT_EL0）。
0: 禁止EL1和EL0访问物理计数器寄存器。
EL1PCEN (bit 1): EL1 Physical Timer Enable

1: 允许EL1和EL0访问物理计时器寄存器（CNTP_CTL_EL0, CNTP_CVAL_EL0, CNTP_TVAL_EL0）。
0: 禁止EL1和EL0访问物理计时器寄存器。
EVNTI (bits 4-8): Event Stream Frequency

这些位指定了虚拟机和EL0可访问的事件流频率。
EVNTDIR (bit 9): Event Stream Direction

1: 启用事件流。
0: 禁用事件流。
EVNTEN (bit 3): Event Stream Enable

1: 启用事件流。
0: 禁用事件流。
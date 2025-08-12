

## GICD_CTLR 0x000 是一个 32 位寄存器，其主要位定义如下：

Bit 0 (EnableGrp0):

启用或禁用组 0 中断。
0：禁用组 0 中断。
1：启用组 0 中断。
Bit 1 (EnableGrp1):

启用或禁用组 1 中断。
0：禁用组 1 中断。
1：启用组 1 中断。
Bit 2 (EnableGrp1A) (可选，依赖于实现):

在一些实现中，GIC 支持安全扩展 (Security Extensions)，此位用于启用组 1A 中断。
0：禁用组 1A 中断。
1：启用组 1A 中断。
Bits [31:3]:

保留，必须写为 0。


## 正常中断流程（ISR 执行很快）

📍每一步状态细节：
① 中断触发（硬件/软件）
中断变为 Pending

GIC 检查中断优先级和目标 CPU，如果合适就发中断信号（IRQ）

② CPU 响应中断（读 GICC_IAR）
GIC 将状态从 Pending → Active

同时也从 ISR 队列中移除，进入处理中

③ CPU 执行 ISR（中断服务例程）
没有再触发一次相同中断，所以不会进入 Active+Pending

ISR 快速执行完毕

④ CPU 写 GICC_EOIR（End of Interrupt Register）
Active → Inactive

系统准备好接收下一次该中断

## 关于 EOImode 

// EOImodeNS, bit [9] Controls the behavior of Non-secure accesses to GICC_EOIR GICC_AEOIR, and GICC_DIR
// 写 EOI 只清除 pending，需要写 DIR 手动清除 active


# CPSR (Current Program Status Register):

CPSR 是 ARM32 架构中的程序状态寄存器，用于存储当前运行状态的相关信息。在 ARM64 架构中，类似的寄存器是 SPSR（Saved Program Status Register，保存的程序状态寄存器）。
CPSR 包含条件标志、控制位、状态位等，用于控制指令执行和处理器状态的管理。
SPSR (Saved Program Status Register):

# SPSR 是 ARM64 架构中的保存的程序状态寄存器，主要用于在异常处理和处理器模式切换时保存当前的程序状态。
SPSR 允许处理器在处理异常返回时恢复之前的程序状态，例如从异常模式返回到用户模式或操作系统内核模式。
PSR (Program Status Register):

PSR 是一般指 ARM 架构中的程序状态寄存器的统称，包括 CPSR 和 SPSR 在内。它们用于管理处理器的执行状态、中断状态、条件标志等重要信息。
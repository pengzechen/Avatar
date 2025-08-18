
# 加载自定义 GDB 配置
# source .vscode/gdbinit

file build/kernel.elf
target remote localhost:1234
add-symbol-file build/kernel.elf 0x40080000
add-symbol-file app/add/build/app.elf
add-symbol-file app/sub/build/app.elf
add-symbol-file app/testapp/build/app.elf

add-symbol-file guest/testos/build/kernel.elf
add-symbol-file guest/linux/vmlinux
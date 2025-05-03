rm -f build/*.o
rm -f build/*.bin
rm -f build/*.txt
rm -f build/*.elf
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 main.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/main.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 smp.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/smp.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 main_hyper.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/main_hyper.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 boot/boot.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/boot.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 guest/guest.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/guest.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 exception/exception.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/exception.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 exception/exception.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/exception.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 io/io.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/io.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 io/uart_pl011.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/uart_pl011.o  
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 io/uart_pl011_early.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/uart_pl011_early.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 io/printf.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/printf.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 mem/mmu.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/mmu.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 mem/page.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/page.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 mem/ept.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/ept.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 mem/string.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/string.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 exception/exception_el3.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/exception_el3.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 exception/exception_el3.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/exception_el3.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 exception/exception_el2.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/exception_el2.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 exception/exception_el2.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/exception_el2.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 exception/gic/gic.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/gic.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 timer/timer.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/timer.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 schedule/task.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/task.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 schedule/context.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/context.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 spinlock/spinlock.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/spinlock.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 hyper/vcpu.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/vcpu.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 hyper/hyper_ctx.S -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/hyper_ctx.s.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 hyper/vgic.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/vgic.o
aarch64-linux-musl-gcc -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=1 hyper/vm.c -I ./include -I /home/ajax/rust/usb/first_state/aarch64-linux-musl-cross/aarch64-linux-musl/include -o build/vm.o
aarch64-linux-musl-ld -T link.lds -o build/kernel.elf \
build/boot.s.o                  \
build/guest.s.o          \
build/main.o                    \
build/smp.o              \
build/main_hyper.o              \
build/exception.s.o             \
build/exception_el3.s.o  \
build/exception.o               \
build/exception_el3.o   \
build/exception_el2.o    \
build/exception_el2.s.o  \
build/gic.o                             \
build/io.o                              \
build/uart_pl011.o       \
build/uart_pl011_early.o \
build/printf.o                  \
build/mmu.s.o                   \
build/page.o                    \
build/ept.o              \
build/string.o                  \
build/timer.o                   \
build/task.o                    \
build/context.s.o               \
build/spinlock.s.o       \
build/vcpu.o             \
build/vgic.o             \
build/vm.o               \
build/hyper_ctx.s.o
qemu-system-aarch64 -m 4G -smp 1 -cpu cortex-a72 -nographic  -M virt,gic_version=2 -M virtualization=on -kernel build/kernel.elf
set enable: reg: 0, mask: 0x1
set enable: reg: 0, mask: 0x2
set enable: reg: 0, mask: 0x4
set enable: reg: 0, mask: 0x8
set enable: reg: 0, mask: 0x10
set enable: reg: 0, mask: 0x20
set enable: reg: 0, mask: 0x40
set enable: reg: 0, mask: 0x80
set enable: reg: 0, mask: 0x100
set enable: reg: 0, mask: 0x200
set enable: reg: 0, mask: 0x400
set enable: reg: 0, mask: 0x800
set enable: reg: 0, mask: 0x1000
set enable: reg: 0, mask: 0x2000
set enable: reg: 0, mask: 0x4000
set enable: reg: 0, mask: 0x8000
set enable: reg: 0, mask: 0x10000
set enable: reg: 0, mask: 0x20000
set enable: reg: 0, mask: 0x40000
set enable: reg: 0, mask: 0x80000
set enable: reg: 0, mask: 0x100000
set enable: reg: 0, mask: 0x200000
set enable: reg: 0, mask: 0x400000
set enable: reg: 0, mask: 0x800000
set enable: reg: 0, mask: 0x1000000
set enable: reg: 0, mask: 0x2000000
set enable: reg: 0, mask: 0x4000000
set enable: reg: 0, mask: 0x8000000
set enable: reg: 0, mask: 0x10000000
set enable: reg: 0, mask: 0x20000000
set enable: reg: 0, mask: 0x40000000
set enable: reg: 0, mask: 0x80000000
    gicd enable ok
    gicc enable ok
    irq numbers: 288
    cpu num: 1
    gich enable ok
timer frq: 62500000
set enable: reg: 0, mask: 0x40000000
get enable: reg: 0, mask: 40000000, value: ffffffff
timer enabled successfully ...
io, gic, timer, init ok...

    Initialize vtcr...
vtcr val: 0x1355c
Initialize EPT...
EPT root address : 0x4265c000
ept_L2_root : 0x4265d000
ept_L3_root : 0x42665000
LPAE_L1_SIZE : 8
LPAE_L2_SIZE : 4096
LPAE_L3_SIZE : 2097152
    Initialize trap...
HCR : 0x8028403b
Copy guest dtb from 42567800 to 70000000 (7651 bytes): 0xedfe0dd0 / 0x38000000
Copy end : 0xedfe0dd0 / 0x38000000
Copy guest kernel image from 4008a400 to 70200000 (38654464 bytes): 0xfa405a4d / 0x0
Copy end : 0xfa405a4d / 0x0
Copy guest fs from 42569800 to 78000000 (736355 bytes): 0x88b1f / 0x9cbc0302
Copy end : 0x88b1f / 0x9cbc0302

Hello Hyper:
there's some hyper tests: 
scrlr_el2: 0x1
hcr_el2: 0x8028403b
read_vttbr_el2: 0x4265c000
cacheline_bytes: 4

core 0 current task 0
id: 0, elr: 0x40080178
id: 1, elr: 0x70200000

            This is smc call handler
            This is smc call handler
            This is smc call handler
            This is smc call handler
            This is smc call handler
            This is smc call handler
            This is smc call handler
            This is smc call handler
            This is smc call handler
            This is smc call handler
      >>> gicd emu read GICD_TYPER
      <<< gicd emu write GICD_CTLR
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ITARGETSR(i)
      <<< gicd emu write GICD_ICFGR(i)
      <<< gicd emu write GICD_ICFGR(i)
      <<< gicd emu write GICD_ICFGR(i)
      <<< gicd emu write GICD_ICFGR(i)
      <<< gicd emu write GICD_ICFGR(i)
      <<< gicd emu write GICD_ICFGR(i)
      <<< gicd emu write GICD_ICFGR(i)
      <<< gicd emu write GICD_ICFGR(i)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_IPRIORITYR(spi)
      <<< gicd emu write GICD_ICPENDER(i)
      <<< gicd emu write GICD_ICENABLER(i)
      <<< gicd emu write GICD_ICPENDER(i)
      <<< gicd emu write GICD_ICENABLER(i)
      <<< gicd emu write GICD_ICPENDER(i)
      <<< gicd emu write GICD_ICENABLER(i)
      <<< gicd emu write GICD_ICPENDER(i)
      <<< gicd emu write GICD_ICENABLER(i)
      <<< gicd emu write GICD_CTLR
      <<< gicd emu write GICD_ICPENDER(i)
      <<< gicd emu write GICD_ICENABLER(0)
      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)
      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)
      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)
      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)
      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)
      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)
      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)
      <<< gicd emu write GICD_IPRIORITYR(sgi-ppi)
# unsupported access
gpa: 0x8010000
gpa: 8000100, r: 1, len: 4, int id: 0
set enable: reg: 0, mask: 0x1
      <<< gicd emu write GICD_ISENABLER(0)
gpa: 8000100, r: 2, len: 4, int id: 1
set enable: reg: 0, mask: 0x2
      <<< gicd emu write GICD_ISENABLER(0)
gpa: 8000100, r: 4, len: 4, int id: 2
set enable: reg: 0, mask: 0x4
      <<< gicd emu write GICD_ISENABLER(0)
gpa: 8000100, r: 8, len: 4, int id: 3
set enable: reg: 0, mask: 0x8
      <<< gicd emu write GICD_ISENABLER(0)
gpa: 8000100, r: 10, len: 4, int id: 4
set enable: reg: 0, mask: 0x10
      <<< gicd emu write GICD_ISENABLER(0)
gpa: 8000100, r: 20, len: 4, int id: 5
set enable: reg: 0, mask: 0x20
      <<< gicd emu write GICD_ISENABLER(0)
gpa: 8000100, r: 40, len: 4, int id: 6
set enable: reg: 0, mask: 0x40
      <<< gicd emu write GICD_ISENABLER(0)
gpa: 8000100, r: 8000000, len: 4, int id: 27
set enable: reg: 0, mask: 0x8000000
      <<< gicd emu write GICD_ISENABLER(0)
            This is smc call handler
            This is smc call handler
            This is smc call handler
[    0.000000] Booting Linux on physical CPU 0x0000000000 [0x410fd083]
[    0.000000] Linux version 6.2.15 (ajax@ajax-X99) (aarch64-linux-musl-gcc (GCC) 11.2.1 20211120, GNU ld (GNU Binutils) 2.37) #6 SMP PREEMPT Fri Aug 30 22:06:21 CST 2024
[    0.000000] random: crng init done
[    0.000000] Machine model: linux,dummy-virt
[    0.000000] efi: UEFI not found.
[    0.000000] NUMA: No NUMA configuration found
[    0.000000] NUMA: Faking a node at [mem 0x0000000070000000-0x000000007bffffff]
[    0.000000] NUMA: NODE_DATA [mem 0x7bfe3a00-0x7bfe5fff]
[    0.000000] Zone ranges:
[    0.000000]   DMA      [mem 0x0000000070000000-0x000000007bffffff]
[    0.000000]   DMA32    empty
[    0.000000]   Normal   empty
[    0.000000] Movable zone start for each node
[    0.000000] Early memory node ranges
[    0.000000]   node   0: [mem 0x0000000070000000-0x000000007bffffff]
[    0.000000] Initmem setup node 0 [mem 0x0000000070000000-0x000000007bffffff]
[    0.000000] On node 0, zone DMA: 16384 pages in unavailable ranges
[    0.000000] cma: Reserved 32 MiB at 0x0000000079a00000
[    0.000000] psci: probing for conduit method from DT.
[    0.000000] psci: PSCIv33792.0 detected in firmware.
[    0.000000] psci: Using standard PSCI v0.2 function IDs
[    0.000000] psci: MIGRATE_INFO_TYPE returned unknown type (-2080374778)
[    0.000000] psci: SMC Calling Convention v32768.0
[    0.000000] percpu: Embedded 21 pages/cpu s45672 r8192 d32152 u86016
[    0.000000] Detected PIPT I-cache on CPU0
[    0.000000] CPU features: detected: Spectre-v2
[    0.000000] CPU features: detected: Spectre-v4
[    0.000000] CPU features: detected: Spectre-BHB
[    0.000000] CPU features: detected: ARM erratum 1742098
[    0.000000] CPU features: detected: ARM errata 1165522, 1319367, or 1530923
[    0.000000] alternatives: applying boot alternatives
[    0.000000] Fallback order for Node 0: 0 
[    0.000000] Built 1 zonelists, mobility grouping on.  Total pages: 48384
[    0.000000] Policy zone: DMA
[    0.000000] Kernel command line: root=/dev/vda rootfstype=ext4 rw console=ttyAMA0 rodata=off
[    0.000000] Dentry cache hash table entries: 32768 (order: 6, 262144 bytes, linear)
[    0.000000] Inode-cache hash table entries: 16384 (order: 5, 131072 bytes, linear)
[    0.000000] mem auto-init: stack:off, heap alloc:off, heap free:off
[    0.000000] Memory: 113360K/196608K available (16256K kernel code, 3892K rwdata, 9532K rodata, 7936K init, 608K bss, 50480K reserved, 32768K cma-reserved)
[    0.000000] SLUB: HWalign=64, Order=0-3, MinObjects=0, CPUs=1, Nodes=1
[    0.000000] rcu: Preemptible hierarchical RCU implementation.
[    0.000000] rcu:     RCU event tracing is enabled.
[    0.000000] rcu:     RCU restricting CPUs from NR_CPUS=256 to nr_cpu_ids=1.
[    0.000000]  Trampoline variant of Tasks RCU enabled.
[    0.000000]  Tracing variant of Tasks RCU enabled.
[    0.000000] rcu: RCU calculated value of scheduler-enlistment delay is 25 jiffies.
[    0.000000] rcu: Adjusting geometry for rcu_fanout_leaf=16, nr_cpu_ids=1
[    0.000000] NR_IRQS: 64, nr_irqs: 64, preallocated irqs: 0
[    0.000000] Root IRQ handler: gic_handle_irq
[    0.000000] GICv2m: range[mem 0x08020000-0x08020fff], SPI[80:143]
[    0.000000] rcu: srcu_init: Setting srcu_struct sizes based on contention.
[    0.000000] arch_timer: cp15 timer(s) running at 62.50MHz (virt).
[    0.000000] clocksource: arch_sys_counter: mask: 0x1ffffffffffffff max_cycles: 0x1cd42e208c, max_idle_ns: 881590405314 ns
[    0.000094] sched_clock: 57 bits at 63MHz, resolution 16ns, wraps every 4398046511096ns
[    0.011582] Console: colour dummy device 80x25
[    0.014571] Calibrating delay loop (skipped), value calculated using timer frequency.. 125.00 BogoMIPS (lpj=250000)
[    0.014823] pid_max: default: 32768 minimum: 301
[    0.016134] LSM: initializing lsm=capability,integrity
[    0.019585] Mount-cache hash table entries: 512 (order: 0, 4096 bytes, linear)
[    0.019653] Mountpoint-cache hash table entries: 512 (order: 0, 4096 bytes, linear)
[    0.126685] cacheinfo: Unable to detect cache hierarchy for CPU 0
[    0.136720] cblist_init_generic: Setting adjustable number of callback queues.
[    0.136884] cblist_init_generic: Setting shift to 0 and lim to 1.
[    0.137967] cblist_init_generic: Setting shift to 0 and lim to 1.
[    0.141063] rcu: Hierarchical SRCU implementation.
[    0.141125] rcu:     Max phase no-delay instances is 1000.
[    0.148445] EFI services will not be available.
[    0.150157] smp: Bringing up secondary CPUs ...
[    0.150288] smp: Brought up 1 node, 1 CPU
[    0.150328] SMP: Total of 1 processors activated.
[    0.150435] CPU features: detected: 32-bit EL0 Support
[    0.151703] CPU features: detected: 32-bit EL1 Support
[    0.151797] CPU features: detected: CRC32 instructions
[    0.154614] CPU: All CPU(s) started at EL1
[    0.155099] alternatives: applying system-wide alternatives
[    0.207890] devtmpfs: initialized
[    0.226151] clocksource: jiffies: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 7645041785100000 ns
[    0.261606] futex hash table entries: 256 (order: 2, 16384 bytes, linear)
[    0.267508] pinctrl core: initialized pinctrl subsystem
[    0.282308] DMI not present or invalid.
[    0.292801] NET: Registered PF_NETLINK/PF_ROUTE protocol family
[    0.341249] DMA: preallocated 128 KiB GFP_KERNEL pool for atomic allocations
[    0.342763] DMA: preallocated 128 KiB GFP_KERNEL|GFP_DMA pool for atomic allocations
[    0.343081] DMA: preallocated 128 KiB GFP_KERNEL|GFP_DMA32 pool for atomic allocations
[    0.343464] audit: initializing netlink subsys (disabled)
[    0.350984] audit: type=2000 audit(0.160:1): state=initialized audit_enabled=0 res=1
[    0.355036] thermal_sys: Registered thermal governor 'step_wise'
[    0.355095] thermal_sys: Registered thermal governor 'power_allocator'
[    0.355763] cpuidle: using governor menu
[    0.357606] hw-breakpoint: found 6 breakpoint and 4 watchpoint registers.
[    0.358274] ASID allocator initialised with 65536 entries
[    0.402873] Serial: AMBA PL011 UART driver
[    0.537758] 9000000.pl011: ttyAMA0 at MMIO 0x9000000 (irq = 13, base_baud = 0) is a PL011 rev1
[    0.539127] printk: console [ttyAMA0] enabled
[    0.618282] HugeTLB: registered 1.00 GiB page size, pre-allocated 0 pages
[    0.618622] HugeTLB: 0 KiB vmemmap can be freed for a 1.00 GiB page
[    0.618881] HugeTLB: registered 32.0 MiB page size, pre-allocated 0 pages
[    0.619610] HugeTLB: 0 KiB vmemmap can be freed for a 32.0 MiB page
[    0.619880] HugeTLB: registered 2.00 MiB page size, pre-allocated 0 pages
[    0.620105] HugeTLB: 0 KiB vmemmap can be freed for a 2.00 MiB page
[    0.620343] HugeTLB: registered 64.0 KiB page size, pre-allocated 0 pages
[    0.620581] HugeTLB: 0 KiB vmemmap can be freed for a 64.0 KiB page
[    0.634882] ACPI: Interpreter disabled.
[    0.679381] iommu: Default domain type: Translated 
[    0.679673] iommu: DMA domain TLB invalidation policy: strict mode 
[    0.681808] SCSI subsystem initialized
[    0.685553] usbcore: registered new interface driver usbfs
[    0.686158] usbcore: registered new interface driver hub
[    0.686604] usbcore: registered new device driver usb
[    0.690954] pps_core: LinuxPPS API ver. 1 registered
[    0.691215] pps_core: Software ver. 5.3.6 - Copyright 2005-2007 Rodolfo Giometti <giometti@linux.it>
[    0.692058] PTP clock support registered
[    0.693387] EDAC MC: Ver: 3.0.0
[    0.703610] FPGA manager framework
[    0.739449] Advanced Linux Sound Architecture Driver Initialized.
[    0.753103] vgaarb: loaded
[    0.757848] clocksource: Switched to clocksource arch_sys_counter
[    0.759908] VFS: Disk quotas dquot_6.6.0
[    0.762602] VFS: Dquot-cache hash table entries: 512 (order 0, 4096 bytes)
[    0.764909] pnp: PnP ACPI: disabled
[    0.831710] NET: Registered PF_INET protocol family
[    0.834347] IP idents hash table entries: 4096 (order: 3, 32768 bytes, linear)
[    0.876696] tcp_listen_portaddr_hash hash table entries: 256 (order: 0, 4096 bytes, linear)
[    0.877430] Table-perturb hash table entries: 65536 (order: 6, 262144 bytes, linear)
[    0.878289] TCP established hash table entries: 2048 (order: 2, 16384 bytes, linear)
[    0.878833] TCP bind hash table entries: 2048 (order: 4, 65536 bytes, linear)
[    0.879293] TCP: Hash tables configured (established 2048 bind 2048)
[    0.881026] UDP hash table entries: 256 (order: 1, 8192 bytes, linear)
[    0.882174] UDP-Lite hash table entries: 256 (order: 1, 8192 bytes, linear)
[    0.884129] NET: Registered PF_UNIX/PF_LOCAL protocol family
[    0.888997] RPC: Registered named UNIX socket transport module.
[    0.889327] RPC: Registered udp transport module.
[    0.889512] RPC: Registered tcp transport module.
[    0.890630] RPC: Registered tcp NFSv4.1 backchannel transport module.
[    0.891069] PCI: CLS 0 bytes, default 64
[    0.897547] Unpacking initramfs...
gpa: 8000100, r: 800000, len: 4, int id: 23
set enable: reg: 0, mask: 0x800000
      <<< gicd emu write GICD_ISENABLER(0)
[    0.907586] hw perfevents: enabled with armv8_pmuv3 PMU driver, 5 counters available
[    0.908657] kvm [1]: HYP mode not available
[    1.017288] Initialise system trusted keyrings
[    1.020672] workingset: timestamp_bits=42 max_order=16 bucket_order=0
[    1.024398] squashfs: version 4.0 (2009/01/31) Phillip Lougher
[    1.028059] NFS: Registering the id_resolver key type
[    1.028850] Key type id_resolver registered
[    1.029083] Key type id_legacy registered
[    1.030777] nfs4filelayout_init: NFSv4 File Layout Driver Registering...
[    1.031232] nfs4flexfilelayout_init: NFSv4 Flexfile Layout Driver Registering...
[    1.032761] 9p: Installing v9fs 9p2000 file system support
[    1.170602] Key type asymmetric registered
[    1.170869] Asymmetric key parser 'x509' registered
[    1.171699] Block layer SCSI generic (bsg) driver version 0.4 loaded (major 245)
[    1.172107] io scheduler mq-deadline registered
[    1.172366] io scheduler kyber registered
      >>> gicd emu read GICD_ICFGR(i)
gpa: 8000104, r: 80, len: 4, int id: 39
set enable: reg: 1, mask: 0x80
      <<< gicd emu write GICD_ISENABLER(i)
      <<< gicd emu write GICD_ITARGETSR(i)
[    1.427451] pl061_gpio 9030000.pl061: PL061 GPIO chip registered
[    1.439927] pci-host-generic 4010000000.pcie: host bridge /pcie@10000000 ranges:
[    1.441292] pci-host-generic 4010000000.pcie:       IO 0x003eff0000..0x003effffff -> 0x0000000000
[    1.442804] pci-host-generic 4010000000.pcie:      MEM 0x0010000000..0x003efeffff -> 0x0010000000
[    1.443278] pci-host-generic 4010000000.pcie:      MEM 0x8000000000..0xffffffffff -> 0x8000000000
[    1.444391] pci-host-generic 4010000000.pcie: Memory resource size exceeds max for 32 bits
[    1.445460] pci-host-generic 4010000000.pcie: ECAM at [mem 0x4010000000-0x401fffffff] for [bus 00-ff]
[    1.448314] pci-host-generic 4010000000.pcie: PCI host bridge to bus 0000:00
[    1.449020] pci_bus 0000:00: root bus resource [bus 00-ff]
[    1.449413] pci_bus 0000:00: root bus resource [io  0x0000-0xffff]
[    1.450129] pci_bus 0000:00: root bus resource [mem 0x10000000-0x3efeffff]
[    1.450620] pci_bus 0000:00: root bus resource [mem 0x8000000000-0xffffffffff]
[    1.453245] pci 0000:00:01.0: [8000:0000] type 0e class 0x000080
[    1.556362] pci 0000:00:01.0: unknown header type 0e, ignoring device
[    1.557170] pci 0000:00:02.0: [0000:0001] type 0e class 0x000100
[    1.558078] pci 0000:00:02.0: unknown header type 0e, ignoring device
[    1.558786] pci 0000:00:03.0: [8000:0001] type 0e class 0x000180
[    1.559311] pci 0000:00:03.0: unknown header type 0e, ignoring device
[    1.559965] pci 0000:00:04.0: [0000:0002] type 0e class 0x000200
[    1.560413] pci 0000:00:04.0: unknown header type 0e, ignoring device
[    1.561121] pci 0000:00:05.0: [8000:0002] type 0e class 0x000280
[    1.561565] pci 0000:00:05.0: unknown header type 0e, ignoring device
[    1.562741] pci 0000:00:06.0: [0000:0003] type 0e class 0x000300
[    1.563186] pci 0000:00:06.0: unknown header type 0e, ignoring device
[    1.563843] pci 0000:00:07.0: [8000:0003] type 0e class 0x000380
[    1.564342] pci 0000:00:07.0: unknown header type 0e, ignoring device
[    1.564991] pci 0000:00:08.0: [0000:0004] type 0e class 0x000400
[    1.565440] pci 0000:00:08.0: unknown header type 0e, ignoring device
[    1.566695] pci 0000:00:09.0: [8000:0004] type 0e class 0x000480
[    1.567110] pci 0000:00:09.0: unknown header type 0e, ignoring device
[    1.567868] pci 0000:00:0a.0: [0000:0005] type 0e class 0x000500
[    1.568337] pci 0000:00:0a.0: unknown header type 0e, ignoring device
[    1.569000] pci 0000:00:0b.0: [8000:0005] type 0e class 0x000580
[    1.569575] pci 0000:00:0b.0: unknown header type 0e, ignoring device
[    1.570646] pci 0000:00:0c.0: [0000:0006] type 0e class 0x000600
[    1.571119] pci 0000:00:0c.0: unknown header type 0e, ignoring device
[    1.571812] pci 0000:00:0d.0: [8000:0006] type 0e class 0x000680
[    1.572260] pci 0000:00:0d.0: unknown header type 0e, ignoring device
[    1.572954] pci 0000:00:0e.0: [0000:0007] type 0e class 0x000700
[    1.573411] pci 0000:00:0e.0: unknown header type 0e, ignoring device
[    1.574602] pci 0000:00:0f.0: [8000:0007] type 0e class 0x000780
[    1.575043] pci 0000:00:0f.0: unknown header type 0e, ignoring device
[    1.575721] pci 0000:00:10.0: [0000:0008] type 0e class 0x000800
[    1.576234] pci 0000:00:10.0: unknown header type 0e, ignoring device
[    1.576880] pci 0000:00:11.0: [8000:0008] type 0e class 0x000880
[    1.577329] pci 0000:00:11.0: unknown header type 0e, ignoring device
[    1.578600] pci 0000:00:12.0: [0000:0009] type 0e class 0x000900
[    1.579018] pci 0000:00:12.0: unknown header type 0e, ignoring device
[    1.579776] pci 0000:00:13.0: [8000:0009] type 0e class 0x000980
[    1.580220] pci 0000:00:13.0: unknown header type 0e, ignoring device
[    1.580872] pci 0000:00:14.0: [0000:000a] type 0e class 0x000a00
[    1.581372] pci 0000:00:14.0: unknown header type 0e, ignoring device
[    1.582394] pci 0000:00:15.0: [8000:000a] type 0e class 0x000a80
[    1.582834] pci 0000:00:15.0: unknown header type 0e, ignoring device
[    1.583624] pci 0000:00:16.0: [0000:000b] type 0e class 0x000b00
[    1.584057] pci 0000:00:16.0: unknown header type 0e, ignoring device
[    1.584718] pci 0000:00:17.0: [8000:000b] type 0e class 0x000b80
[    1.585276] pci 0000:00:17.0: unknown header type 0e, ignoring device
[    1.586297] pci 0000:00:18.0: [0000:000c] type 0e class 0x000c00
[    1.586835] pci 0000:00:18.0: unknown header type 0e, ignoring device
[    1.587515] pci 0000:00:19.0: [8000:000c] type 0e class 0x000c80
[    1.587963] pci 0000:00:19.0: unknown header type 0e, ignoring device
[    1.588762] pci 0000:00:1a.0: [0000:000d] type 0e class 0x000d00
[    1.589183] pci 0000:00:1a.0: unknown header type 0e, ignoring device
[    1.696860] Freeing initrd memory: 7384K
[    1.697969] pci 0000:00:1b.0: [8000:000d] type 0e class 0x000d80
[    1.698418] pci 0000:00:1b.0: unknown header type 0e, ignoring device
[    1.699189] pci 0000:00:1c.0: [0000:000e] type 0e class 0x000e00
[    1.699761] pci 0000:00:1c.0: unknown header type 0e, ignoring device
[    1.700533] pci 0000:00:1d.0: [8000:000e] type 0e class 0x000e80
[    1.701053] pci 0000:00:1d.0: unknown header type 0e, ignoring device
[    1.702138] pci 0000:00:1e.0: [0000:000f] type 0e class 0x000f00
[    1.702787] pci 0000:00:1e.0: unknown header type 0e, ignoring device
[    1.703558] pci 0000:00:1f.0: [8000:000f] type 0e class 0x000f80
[    1.704106] pci 0000:00:1f.0: unknown header type 0e, ignoring device
[    1.716788] EINJ: ACPI disabled.
[    1.979069] Serial: 8250/16550 driver, 4 ports, IRQ sharing enabled
[    1.992362] SuperH (H)SCI(F) driver initialized
[    1.995910] msm_serial: driver initialized
[    2.038843] cacheinfo: Unable to detect cache hierarchy for CPU 0
[    2.101877] loop: module loaded
[    2.107027] megasas: 07.719.03.00-rc1
[    2.118267] physmap-flash 0.flash: physmap platform flash device: [mem 0x00000000-0x03ffffff]
[    2.120427] 0.flash: Found 2 x16 devices at 0x0 in 32-bit bank. Manufacturer ID 0x000000 Chip ID 0x000000
[    2.121459] Intel/Sharp Extended Query Table at 0x0031
[    2.123065] Using buffer write method
[    2.124153] physmap-flash 0.flash: physmap platform flash device: [mem 0x04000000-0x07ffffff]
[    2.125248] 0.flash: Found 2 x16 devices at 0x0 in 32-bit bank. Manufacturer ID 0x000000 Chip ID 0x000000
[    2.126159] Intel/Sharp Extended Query Table at 0x0031
[    2.127300] Using buffer write method
[    2.127639] Concatenating MTD devices:
[    2.127821] (0): "0.flash"
[    2.127948] (1): "0.flash"
[    2.128068] into device "0.flash"
[    2.329191] tun: Universal TUN/TAP device driver, 1.6
[    2.334416] thunder_xcv, ver 1.0
[    2.335017] thunder_bgx, ver 1.0
[    2.335331] nicpf, ver 1.0
[    2.376425] hns3: Hisilicon Ethernet Network Driver for Hip08 Family - version
[    2.376795] hns3: Copyright (c) 2017 Huawei Corporation.
[    2.377678] hclge is initializing
[    2.378466] e1000: Intel(R) PRO/1000 Network Driver
[    2.378706] e1000: Copyright (c) 1999-2006 Intel Corporation.
[    2.379397] e1000e: Intel(R) PRO/1000 Network Driver
[    2.379653] e1000e: Copyright(c) 1999 - 2015 Intel Corporation.
[    2.380043] igb: Intel(R) Gigabit Ethernet Network Driver
[    2.380290] igb: Copyright (c) 2007-2014 Intel Corporation.
[    2.380898] igbvf: Intel(R) Gigabit Virtual Function Network Driver
[    2.381172] igbvf: Copyright (c) 2009 - 2012 Intel Corporation.
[    2.383614] sky2: driver version 1.30
[    2.391487] VFIO - User Level meta-driver version: 0.3
[    2.407692] usbcore: registered new interface driver usb-storage
[    2.461253] rtc-pl031 9010000.pl031: registered as rtc0
[    2.462653] rtc-pl031 9010000.pl031: setting system clock to 2024-08-31T16:03:09 UTC (1725120189)
      >>> gicd emu read GICD_ICFGR(i)
gpa: 8000104, r: 4, len: 4, int id: 34
set enable: reg: 1, mask: 0x4
      <<< gicd emu write GICD_ISENABLER(i)
      <<< gicd emu write GICD_ITARGETSR(i)
[    2.468530] i2c_dev: i2c /dev entries driver
[    2.531468] ghes_edac: GHES probing device list is empty
[    2.539953] sdhci: Secure Digital Host Controller Interface driver
[    2.540491] sdhci: Copyright(c) Pierre Ossman
[    2.544840] Synopsys Designware Multimedia Card Interface Driver
[    2.584381] sdhci-pltfm: SDHCI platform and OF driver helper
[    2.594631] ledtrig-cpu: registered to indicate activity on CPUs
            This is smc call handler
[    2.601313] SMCCC: SOC_ID: ARCH_FEATURES(ARCH_SOC_ID) returned error: 80000001
[    2.607290] usbcore: registered new interface driver usbhid
[    2.607609] usbhid: USB HID core driver
[    2.678813] NET: Registered PF_PACKET protocol family
[    2.680258] 9pnet: Installing 9P2000 support
[    2.681048] Key type dns_resolver registered
[    2.749479] registered taskstats version 1
[    2.785342] Loading compiled-in X.509 certificates
[    2.816576] input: gpio-keys as /devices/platform/gpio-keys/input/input0
[    2.859671] ALSA device list:
[    2.859949]   No soundcards found.
      >>> gicd emu read GICD_ICFGR(i)
gpa: 8000104, r: 2, len: 4, int id: 33
set enable: reg: 1, mask: 0x2
      <<< gicd emu write GICD_ISENABLER(i)
      <<< gicd emu write GICD_ITARGETSR(i)
[    2.867014] uart-pl011 9000000.pl011: no DMA platform data
[    2.999521] Freeing unused kernel memory: 7936K
[    3.000046] Kernel memory protection disabled.
[    3.000661] Run /init as init process
Loading, please wait...
/bin/sh: can't access tty; job control turned off
~ # ls
bin      init     mnt      root     sys
dev      linuxrc  proc     sbin     tmp
~ #

APP_NAME ?=

# TOOL_PREFIX=aarch64-linux-musl-
TOOL_PREFIX=aarch64-none-elf-

BUILD_DIR=build

INCLUDE = -I $(realpath ./include)

n = -nostdlib -nostdinc -fno-stack-protector

SMP = 1
HV  = n

CFLAGS = -g -c -O0 -fno-pie  -mgeneral-regs-only -fno-builtin-getc -fno-builtin-putc -fexec-charset=GBK\
	 -fno-builtin-vsnprintf -fno-builtin-snprintf -fno-builtin-printf -DSMP_NUM=$(SMP)

QEMU_ARGS = -m 4G -smp $(SMP) -cpu cortex-a72 -nographic 

QEMU_ARGS += -M virt

QEMU_ARGS += -M gic_version=2

# QEMU_ARGS += -M secure=on

ifeq ($(HV),y)
QEMU_ARGS += -M virtualization=on
LD = link_hyper.lds
else
LD = link.lds
endif



$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

#  kernel main
$(BUILD_DIR)/main.o: main.c
	$(TOOL_PREFIX)gcc $(CFLAGS) main.c $(INCLUDE) -o $(BUILD_DIR)/main.o

#  kernel smp
$(BUILD_DIR)/smp.o: smp.c
	$(TOOL_PREFIX)gcc $(CFLAGS) smp.c $(INCLUDE) -o $(BUILD_DIR)/smp.o

#  kernel main hyper
$(BUILD_DIR)/main_hyper.o: main_hyper.c
	$(TOOL_PREFIX)gcc $(CFLAGS) main_hyper.c $(INCLUDE) -o $(BUILD_DIR)/main_hyper.o

#  boot 
$(BUILD_DIR)/boot.s.o: boot/boot.S
	$(TOOL_PREFIX)gcc $(CFLAGS) boot/boot.S $(INCLUDE) -o $(BUILD_DIR)/boot.s.o

#  exception
$(BUILD_DIR)/exception.o: exception/exception.c
	$(TOOL_PREFIX)gcc $(CFLAGS) exception/exception.c $(INCLUDE) -o $(BUILD_DIR)/exception.o

$(BUILD_DIR)/exception_el3.o: exception/exception_el3.c
	$(TOOL_PREFIX)gcc $(CFLAGS) exception/exception_el3.c $(INCLUDE) -o $(BUILD_DIR)/exception_el3.o

$(BUILD_DIR)/exception_el2.o: exception/exception_el2.c
	$(TOOL_PREFIX)gcc $(CFLAGS) exception/exception_el2.c $(INCLUDE) -o $(BUILD_DIR)/exception_el2.o

$(BUILD_DIR)/exception.s.o: exception/exception.S
	$(TOOL_PREFIX)gcc $(CFLAGS) exception/exception.S $(INCLUDE) -o $(BUILD_DIR)/exception.s.o

$(BUILD_DIR)/exception_el3.s.o: exception/exception_el3.S
	$(TOOL_PREFIX)gcc $(CFLAGS) exception/exception_el3.S $(INCLUDE) -o $(BUILD_DIR)/exception_el3.s.o

$(BUILD_DIR)/exception_el2.s.o: exception/exception_el2.S
	$(TOOL_PREFIX)gcc $(CFLAGS) exception/exception_el2.S $(INCLUDE) -o $(BUILD_DIR)/exception_el2.s.o

$(BUILD_DIR)/gic.o: exception/gic/gic.c
	$(TOOL_PREFIX)gcc $(CFLAGS) exception/gic/gic.c $(INCLUDE) -o $(BUILD_DIR)/gic.o


#  syscall
$(BUILD_DIR)/syscall.o: syscall/syscall.c
	$(TOOL_PREFIX)gcc $(CFLAGS) syscall/syscall.c $(INCLUDE) -o $(BUILD_DIR)/syscall.o


#  io
$(BUILD_DIR)/io.o: io/io.c
	$(TOOL_PREFIX)gcc $(CFLAGS) io/io.c $(INCLUDE) -o $(BUILD_DIR)/io.o
$(BUILD_DIR)/printf.o: io/printf.c
	$(TOOL_PREFIX)gcc $(CFLAGS) io/printf.c $(INCLUDE) -o $(BUILD_DIR)/printf.o

$(BUILD_DIR)/uart_pl011.o: io/uart_pl011.c
	$(TOOL_PREFIX)gcc $(CFLAGS) io/uart_pl011.c $(INCLUDE) -o $(BUILD_DIR)/uart_pl011.o  
$(BUILD_DIR)/uart_pl011_early.o: io/uart_pl011_early.c
	$(TOOL_PREFIX)gcc $(CFLAGS) io/uart_pl011_early.c $(INCLUDE) -o $(BUILD_DIR)/uart_pl011_early.o
#  mem
$(BUILD_DIR)/mmu.s.o: mem/mmu.S
	$(TOOL_PREFIX)gcc $(CFLAGS) mem/mmu.S $(INCLUDE) -o $(BUILD_DIR)/mmu.s.o
$(BUILD_DIR)/page.o: mem/page.c
	$(TOOL_PREFIX)gcc $(CFLAGS) mem/page.c $(INCLUDE) -o $(BUILD_DIR)/page.o
$(BUILD_DIR)/string.o: mem/string.c
	$(TOOL_PREFIX)gcc $(CFLAGS) mem/string.c $(INCLUDE) -o $(BUILD_DIR)/string.o
$(BUILD_DIR)/ept.o: mem/ept.c
	$(TOOL_PREFIX)gcc $(CFLAGS) mem/ept.c $(INCLUDE) -o $(BUILD_DIR)/ept.o
$(BUILD_DIR)/bitmap.o: mem/bitmap.c
	$(TOOL_PREFIX)gcc $(CFLAGS) mem/bitmap.c $(INCLUDE) -o $(BUILD_DIR)/bitmap.o
$(BUILD_DIR)/mem.o: mem/mem.c
	$(TOOL_PREFIX)gcc $(CFLAGS) mem/mem.c $(INCLUDE) -o $(BUILD_DIR)/mem.o

#  timer
$(BUILD_DIR)/timer.o: timer/timer.c
	$(TOOL_PREFIX)gcc $(CFLAGS) timer/timer.c $(INCLUDE) -o $(BUILD_DIR)/timer.o

#  task
$(BUILD_DIR)/task.o: task/task.c
	$(TOOL_PREFIX)gcc $(CFLAGS) task/task.c $(INCLUDE) -o $(BUILD_DIR)/task.o
$(BUILD_DIR)/context.s.o: task/context.S
	$(TOOL_PREFIX)gcc $(CFLAGS) task/context.S $(INCLUDE) -o $(BUILD_DIR)/context.s.o
$(BUILD_DIR)/mutex.o: task/mutex.c
	$(TOOL_PREFIX)gcc $(CFLAGS) task/mutex.c $(INCLUDE) -o $(BUILD_DIR)/mutex.o


#  process
$(BUILD_DIR)/process.o: process/pro.c
	$(TOOL_PREFIX)gcc $(CFLAGS) process/pro.c $(INCLUDE) -o $(BUILD_DIR)/process.o

#  spinlock 
$(BUILD_DIR)/spinlock.s.o: spinlock/spinlock.S
	$(TOOL_PREFIX)gcc $(CFLAGS) spinlock/spinlock.S $(INCLUDE) -o $(BUILD_DIR)/spinlock.s.o


#  hyper 
$(BUILD_DIR)/vcpu.o: hyper/vcpu.c
	$(TOOL_PREFIX)gcc $(CFLAGS) hyper/vcpu.c $(INCLUDE) -o $(BUILD_DIR)/vcpu.o
$(BUILD_DIR)/hyper_ctx.s.o: hyper/hyper_ctx.S
	$(TOOL_PREFIX)gcc $(CFLAGS) hyper/hyper_ctx.S $(INCLUDE) -o $(BUILD_DIR)/hyper_ctx.s.o
$(BUILD_DIR)/vgic.o: hyper/vgic.c
	$(TOOL_PREFIX)gcc $(CFLAGS) hyper/vgic.c $(INCLUDE) -o $(BUILD_DIR)/vgic.o
$(BUILD_DIR)/vm.o: hyper/vm.c
	$(TOOL_PREFIX)gcc $(CFLAGS) hyper/vm.c $(INCLUDE) -o $(BUILD_DIR)/vm.o


#  lib/list
$(BUILD_DIR)/list.o: lib/list.c
	$(TOOL_PREFIX)gcc $(CFLAGS) lib/list.c $(INCLUDE) -o $(BUILD_DIR)/list.o


#  guest
$(BUILD_DIR)/guest.s.o: guest/guest.S
	$(TOOL_PREFIX)gcc $(CFLAGS) guest/guest.S $(INCLUDE) -o $(BUILD_DIR)/guest.s.o

#  app
$(BUILD_DIR)/app.s.o: app/app.S
	$(TOOL_PREFIX)gcc $(CFLAGS) app/app.S $(INCLUDE) -o $(BUILD_DIR)/app.s.o



$(BUILD_DIR)/kernel.elf: $(BUILD_DIR) $(BUILD_DIR)/main.o $(BUILD_DIR)/smp.o $(BUILD_DIR)/main_hyper.o \
$(BUILD_DIR)/boot.s.o $(BUILD_DIR)/guest.s.o $(BUILD_DIR)/app.s.o $(BUILD_DIR)/exception.s.o $(BUILD_DIR)/exception.o \
$(BUILD_DIR)/io.o $(BUILD_DIR)/uart_pl011.o $(BUILD_DIR)/uart_pl011_early.o $(BUILD_DIR)/printf.o $(BUILD_DIR)/mmu.s.o \
$(BUILD_DIR)/page.o $(BUILD_DIR)/ept.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/string.o $(BUILD_DIR)/exception_el3.s.o \
$(BUILD_DIR)/exception_el3.o $(BUILD_DIR)/exception_el2.o $(BUILD_DIR)/exception_el2.s.o $(BUILD_DIR)/gic.o  \
$(BUILD_DIR)/syscall.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/task.o $(BUILD_DIR)/context.s.o $(BUILD_DIR)/spinlock.s.o \
$(BUILD_DIR)/vcpu.o $(BUILD_DIR)/hyper_ctx.s.o $(BUILD_DIR)/vgic.o $(BUILD_DIR)/vm.o $(BUILD_DIR)/list.o $(BUILD_DIR)/mem.o \
$(BUILD_DIR)/mutex.o $(BUILD_DIR)/process.o
	$(TOOL_PREFIX)ld -T $(LD) -o $(BUILD_DIR)/kernel.elf \
	$(BUILD_DIR)/boot.s.o 			\
	$(BUILD_DIR)/guest.s.o          \
	$(BUILD_DIR)/app.s.o            \
	$(BUILD_DIR)/main.o 			\
	$(BUILD_DIR)/smp.o              \
	$(BUILD_DIR)/main_hyper.o 		\
	$(BUILD_DIR)/exception.s.o 		\
	$(BUILD_DIR)/exception_el3.s.o  \
	$(BUILD_DIR)/exception.o 		\
	$(BUILD_DIR)/exception_el3.o 	\
	$(BUILD_DIR)/exception_el2.o    \
	$(BUILD_DIR)/exception_el2.s.o  \
	$(BUILD_DIR)/syscall.o          \
	$(BUILD_DIR)/gic.o 				\
	$(BUILD_DIR)/io.o 				\
	$(BUILD_DIR)/uart_pl011.o       \
	$(BUILD_DIR)/uart_pl011_early.o \
	$(BUILD_DIR)/printf.o 			\
	$(BUILD_DIR)/mmu.s.o 			\
	$(BUILD_DIR)/page.o 			\
	$(BUILD_DIR)/bitmap.o           \
	$(BUILD_DIR)/mem.o              \
	$(BUILD_DIR)/ept.o              \
	$(BUILD_DIR)/string.o 			\
	$(BUILD_DIR)/timer.o  			\
	$(BUILD_DIR)/task.o 			\
	$(BUILD_DIR)/process.o          \
	$(BUILD_DIR)/context.s.o 		\
	$(BUILD_DIR)/mutex.o            \
	$(BUILD_DIR)/spinlock.s.o       \
	$(BUILD_DIR)/vcpu.o             \
	$(BUILD_DIR)/vgic.o             \
	$(BUILD_DIR)/vm.o               \
	$(BUILD_DIR)/hyper_ctx.s.o      \
	$(BUILD_DIR)/list.o

deasm_get_bin: $(BUILD_DIR)/kernel.elf
	$(TOOL_PREFIX)objdump -x -d -S $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/dis.txt
	$(TOOL_PREFIX)readelf -a $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/elf.txt
	$(TOOL_PREFIX)objcopy -O binary $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/kernel.bin

app:
	$(MAKE) -C app APP_NAME=$(APP_NAME) CFLAGS="$(CFLAGS)" INCLUDE="$(INCLUDE)"

app_clean:
	$(MAKE) -C app clean APP_NAME=$(APP_NAME) CFLAGS="$(CFLAGS)" INCLUDE="$(INCLUDE)"

.PHONY: app app_clean



debug: deasm_get_bin
	qemu-system-aarch64 $(QEMU_ARGS) -kernel $(BUILD_DIR)/kernel.bin -s -S

run: deasm_get_bin
	qemu-system-aarch64 $(QEMU_ARGS) -kernel $(BUILD_DIR)/kernel.bin


clean:
	rm -f build/*.o
	rm -f build/*.bin
	rm -f build/*.txt
	rm -f build/*.elf

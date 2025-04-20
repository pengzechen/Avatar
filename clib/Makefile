
TOOL_PREFIX=aarch64-none-elf-
INCLUDE=aarch64\include
LIB=aarch64\lib
LD=link.lds
BUILD_DIR=.

all:
	# Compile the main.c to object file (main.o)
	$(TOOL_PREFIX)gcc -g -c main.c  -I$(INCLUDE) -L$(LIB) -lm -lc -o $(BUILD_DIR)/main.o 
	$(TOOL_PREFIX)gcc -g -c main.S  -I$(INCLUDE) -L$(LIB) -lm -lc -o $(BUILD_DIR)/main.s.o
	
	# Link object files into an ELF executable
	$(TOOL_PREFIX)ld -T $(LD) $(BUILD_DIR)/main.s.o $(BUILD_DIR)/main.o  -static -L$(LIB) -lm -lc -o $(BUILD_DIR)/kernel.elf 
	
	# Generate disassembly and ELF information
	$(TOOL_PREFIX)objdump -x -d -S $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/dis.txt
	$(TOOL_PREFIX)readelf -a $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/elf.txt
	
	# Convert ELF to binary
	$(TOOL_PREFIX)objcopy -O binary $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/kernel.bin

	qemu-system-aarch64 -m 4G -cpu cortex-a72 -M virt -nographic  -kernel kernel.elf 

clean:
	rm -rf *.txt
	rm -rf *.elf
	rm -rf *.bin
	rm -rf *.o

	
run:
	qemu-system-aarch64 -m 4G -cpu cortex-a72 -M virt -nographic  -kernel kernel.elf
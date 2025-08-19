#
# Copyright (c) 2024 Avatar Project
#
# Licensed under the MIT License.
# See LICENSE file in the project root for full license information.
#
# @file Makefile
# @brief Build configuration: Makefile
# @author Avatar Project Team
# @date 2024
#

# app/testapp/Makefile

APP_BUILD_DIR = build
APP_SOURCE_DIR = .
APP_LD = app.lds

TARGET = $(APP_BUILD_DIR)/app.bin

# 默认目标
all: $(TARGET)

$(APP_BUILD_DIR):
	mkdir -p $(APP_BUILD_DIR)

$(APP_BUILD_DIR)/syscall.s.o: ../syscall.S | $(APP_BUILD_DIR)
	$(TOOL_PREFIX)gcc $(CFLAGS) $< $(INCLUDE) -o $@

$(APP_BUILD_DIR)/main.o: $(APP_SOURCE_DIR)/main.c $(APP_LD) | $(APP_BUILD_DIR)
	$(TOOL_PREFIX)gcc $(CFLAGS) $< $(INCLUDE) -o $@

$(APP_BUILD_DIR)/app.elf: $(APP_BUILD_DIR)/main.o $(APP_BUILD_DIR)/syscall.s.o
	$(TOOL_PREFIX)ld -T $(APP_LD) -o $@ $^

$(APP_BUILD_DIR)/app.bin: $(APP_BUILD_DIR)/app.elf
	$(TOOL_PREFIX)objcopy -O binary $< $@
	$(TOOL_PREFIX)objdump -x -d -S $< > $(APP_BUILD_DIR)/dis.txt
	$(TOOL_PREFIX)readelf -a $< > $(APP_BUILD_DIR)/elf.txt

clean:
	rm -rf $(APP_BUILD_DIR)

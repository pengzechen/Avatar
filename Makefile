
# ============================================================================
# Avatar Hypervisor Makefile
# ============================================================================

# 版本信息
VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 0
VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# 构建信息
BUILD_DATE := $(shell date +"%Y-%m-%d %H:%M:%S")
BUILD_USER := $(shell whoami)
BUILD_HOST := $(shell hostname)
#如果当前 Git 仓库的状态是 dirty（即有改动未提交），就输出 -dirty
GIT_COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
#获取当前HEAD（最新提交）的哈希值（短格式）
GIT_DIRTY := $(shell git diff-index --quiet HEAD -- 2>/dev/null || echo "-dirty")

# 定义空格变量用于字符串替换
empty :=
space := $(empty) $(empty)

# 配置变量
APP_NAME ?=
TOOL_PREFIX ?= aarch64-linux-musl-
BUILD_DIR ?= build
CONFIG ?= debug
PLATFORM ?= qemu-virt

# 编译配置
SMP ?= 1
HV ?= 0
LOGGER ?= 1
DEBUG ?= 1
DEBUG_MODULE ?= 0
OPTIMIZATION ?= 0
VERBOSE ?= 0

# CPU亲和性配置
CPU_AFFINITY ?=
TASKSET_CMD := $(if $(CPU_AFFINITY),taskset -c $(CPU_AFFINITY),)


# 目录配置
SRC_DIRS := . boot exception io mem timer task process spinlock \
            vmm lib fs app syscall
INCLUDE_DIRS := include guest
INCLUDE := $(addprefix -I, $(INCLUDE_DIRS))

# 编译器标志
CFLAGS_BASE := -fno-pie -mgeneral-regs-only -fno-builtin -nostdinc -fno-stack-protector
CFLAGS_DEBUG := $(if $(filter 1,$(DEBUG)),-g -DDEBUG,)
CFLAGS_OPT := -O$(OPTIMIZATION)
CFLAGS_DEFINES := -DSMP_NUM=$(SMP) -DHV=$(HV) -D__LOG_LEVEL=$(LOGGER) -D__DEBUG_MODULE=$(DEBUG_MODULE)
CFLAGS_VERSION := -DVERSION_STRING=\"$(VERSION)\" -DBUILD_DATE=\"$(subst $(space),_,$(BUILD_DATE))\" \
                  -DGIT_COMMIT=\"$(GIT_COMMIT)$(GIT_DIRTY)\"
CFLAGS_EXTRA ?=
CFLAGS := $(CFLAGS_BASE) $(CFLAGS_DEBUG) $(CFLAGS_OPT) $(CFLAGS_DEFINES) \
          $(CFLAGS_VERSION) $(CFLAGS_EXTRA) $(FEATURES) -c

# App构建使用的简化CFLAGS（避免带空格的参数）
CFLAGS_APP := $(CFLAGS_BASE) $(CFLAGS_DEBUG) $(CFLAGS_OPT) $(CFLAGS_DEFINES) -c

ASFLAGS := $(CFLAGS)
LDFLAGS := -nostdlib $(LDFLAGS_EXTRA)

# 静默输出控制
ifeq ($(VERBOSE),1)
Q :=
else
Q := @
endif

# QEMU配置  --trace qemu_mutex_lock
QEMU_ARGS := -m 4G -smp $(SMP) -cpu cortex-a72 -nographic -M virt -M gic_version=2 -accel tcg,thread=multi 
ifeq ($(HV),1)
QEMU_ARGS += -M virtualization=on
LD := boot/link_vmm.lds
else
LD := boot/link.lds
endif

# 工具链
CC := $(TOOL_PREFIX)gcc
AS := $(TOOL_PREFIX)gcc
LD_TOOL := $(TOOL_PREFIX)ld
OBJDUMP := $(TOOL_PREFIX)objdump
OBJCOPY := $(TOOL_PREFIX)objcopy
READELF := $(TOOL_PREFIX)readelf


# ============================================================================
# 源文件自动发现
# ============================================================================

# 自动发现源文件（排除guest和clib目录）
# 分别处理根目录和其他目录，确保完全排除clib
ROOT_C_SOURCES := $(shell find . -maxdepth 1 -name "*.c" 2>/dev/null)
OTHER_C_SOURCES := $(shell find boot exception io mem timer task process spinlock vmm lib fs syscall -name "*.c" 2>/dev/null)
# 手动添加app目录中的非main.c文件（避免包含app子目录中的main.c）
APP_C_SOURCES := $(shell find app -maxdepth 1 -name "*.c" 2>/dev/null)
# 手动添加guest目录中需要的C文件
GUEST_C_SOURCES := guest/guests.c
C_SOURCES := $(ROOT_C_SOURCES) $(OTHER_C_SOURCES) $(APP_C_SOURCES) $(GUEST_C_SOURCES)

ROOT_S_SOURCES := $(shell find . -maxdepth 1 -name "*.S" 2>/dev/null)
OTHER_S_SOURCES := $(shell find boot exception io mem timer task process spinlock vmm lib fs syscall -name "*.S" 2>/dev/null)
# 手动添加app目录中需要的汇编文件（排除syscall.S）
APP_S_SOURCES := $(shell find app -maxdepth 1 -name "*.S" 2>/dev/null | grep -v syscall.S)
S_SOURCES := $(ROOT_S_SOURCES) $(OTHER_S_SOURCES) $(APP_S_SOURCES)

# 手动添加guest相关文件（只包含需要的两个汇编文件）
GUEST_SOURCES := guest/guests.S guest/test_guest.S

# 合并所有汇编源文件（注意app/app.S已经在S_SOURCES中了）
ALL_S_SOURCES := $(S_SOURCES) $(GUEST_SOURCES)

# 生成目标文件列表
C_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(C_SOURCES)))
S_OBJECTS := $(patsubst %.S,$(BUILD_DIR)/%.s.o,$(notdir $(ALL_S_SOURCES)))
ALL_OBJECTS := $(C_OBJECTS) $(S_OBJECTS)

# 依赖文件
DEPS := $(C_OBJECTS:.o=.d)

# ============================================================================
# 构建规则
# ============================================================================

# 默认目标
.DEFAULT_GOAL := all
all: $(BUILD_DIR)/kernel.bin

# 创建构建目录
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# 根目录C文件编译规则
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

# 特定目录的C文件编译规则（避免匹配clib目录）
$(BUILD_DIR)/%.o: boot/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: exception/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: exception/gic/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: io/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: mem/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: timer/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: task/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: process/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: vmm/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: vmm/vdev/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: vmm/vdev/virtio/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: lib/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: fs/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: syscall/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

$(BUILD_DIR)/%.o: guest/%.c | $(BUILD_DIR)
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/$*.d $< -o $@

# 根目录汇编文件编译规则
$(BUILD_DIR)/%.s.o: %.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

# 特定目录的汇编文件编译规则
$(BUILD_DIR)/%.s.o: boot/%.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

$(BUILD_DIR)/%.s.o: exception/%.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

$(BUILD_DIR)/%.s.o: mem/%.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

$(BUILD_DIR)/%.s.o: task/%.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

$(BUILD_DIR)/%.s.o: spinlock/%.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

$(BUILD_DIR)/%.s.o: vmm/%.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

$(BUILD_DIR)/%.s.o: app/%.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

$(BUILD_DIR)/%.s.o: guest/%.S | $(BUILD_DIR)
	$(Q)echo "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $(INCLUDE) $< -o $@

# 特殊处理：确保某些文件的编译顺序
$(BUILD_DIR)/main_vmm.o: main_vmm.c | $(BUILD_DIR)
	$(Q)echo "  CC      $< (vmm main)"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -MF $(BUILD_DIR)/main_vmm.d $< -o $@


# ============================================================================
# 链接和最终目标
# ============================================================================

# 确保boot.s.o在最前面（链接脚本要求）
LINK_ORDER := $(BUILD_DIR)/boot.s.o
LINK_ORDER += $(filter-out $(BUILD_DIR)/boot.s.o, $(ALL_OBJECTS))

# 链接生成ELF文件
$(BUILD_DIR)/kernel.elf: $(ALL_OBJECTS) $(LD) | $(BUILD_DIR)
	$(Q)echo "  LD      $@"
	$(Q)$(LD_TOOL) $(LDFLAGS) -T $(LD) -o $@ $(LINK_ORDER)
	$(Q)echo "  SIZE    $@"
	$(Q)$(TOOL_PREFIX)size $@

# 生成二进制文件和反汇编
$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(Q)echo "  OBJCOPY $@"
	$(Q)$(OBJCOPY) -O binary $< $@
	$(Q)echo "  OBJDUMP $(BUILD_DIR)/dis.txt"
	$(Q)$(OBJDUMP) -x -d -S $< > $(BUILD_DIR)/dis.txt
	$(Q)echo "  READELF $(BUILD_DIR)/elf.txt"
	$(Q)$(READELF) -a $< > $(BUILD_DIR)/elf.txt
	$(Q)echo ""
	$(Q)echo "Kernel built successfully:"
	$(Q)echo "  Version: $(VERSION)"
	$(Q)echo "  Config:  $(CONFIG)"
	$(Q)echo "  Size:    $$(stat -c%s $@) bytes"
	$(Q)echo "  Binary:  $@"

# ============================================================================
# 应用程序构建
# ============================================================================

app:
	@$(MAKE) -C app APP_NAME=$(APP_NAME) CFLAGS="$(CFLAGS_APP)" INCLUDE="$(INCLUDE) -I../../include"

app_clean:
	@$(MAKE) -C app clean APP_NAME=$(APP_NAME) CFLAGS="$(CFLAGS_APP)" INCLUDE="$(INCLUDE)"

# ============================================================================
# 运行和调试目标
# ============================================================================

run: $(BUILD_DIR)/kernel.bin
	@echo "Starting QEMU..."
	@$(TASKSET_CMD) qemu-system-aarch64 $(QEMU_ARGS) -kernel $<

debug: $(BUILD_DIR)/kernel.bin
	@echo "Starting QEMU in debug mode..."
	@$(TASKSET_CMD) qemu-system-aarch64 $(QEMU_ARGS) -kernel $< -s -S

# ============================================================================
# 清理和维护
# ============================================================================

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

distclean: clean app_clean
	@echo "Deep cleaning..."

# 显示配置信息
info:
	@echo "=== Avatar Build Configuration ==="
	@echo "SMP:          $(SMP)"
	@echo "HV:           $(HV)"
	@echo "LOGGER:       $(LOGGER)"
	@echo "DEBUG:        $(DEBUG)"
	@echo "DEBUG_MODULE: $(DEBUG_MODULE)"
	@echo "OPTIMIZATION: $(OPTIMIZATION)"
	@echo "TOOL_PREFIX:  $(TOOL_PREFIX)"
	@echo "BUILD_DIR:    $(BUILD_DIR)"
	@echo "LINKER:       $(LD)"
	@echo "C_SOURCES:    $(words $(C_SOURCES)) files"
	@echo "S_SOURCES:    $(words $(ALL_S_SOURCES)) files"
	@echo "OBJECTS:      $(words $(ALL_OBJECTS)) files"

# 显示帮助信息
help:
	@echo "Avatar VMM Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build kernel binary (default)"
	@echo "  run          - Build and run in QEMU"
	@echo "  debug        - Build and run in QEMU debug mode"
	@echo "  app          - Build applications"
	@echo "  clean        - Clean build files"
	@echo "  distclean    - Deep clean including apps"
	@echo "  info         - Show build configuration"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Configuration variables:"
	@echo "  SMP=<n>      - Number of CPU cores (default: 1)"
	@echo "  HV=<0|1>     - VMM mode (default: 0)"
	@echo "  LOGGER=<n>   - Log level (default: 1)"
	@echo "  DEBUG=<0|1>  - Debug build (default: 1)"
	@echo "  DEBUG_MODULE=<n> - Debug module mask (default: 0)"
	@echo "                   0=none, 2=GIC, 4=TASK, 8=VGIC, 16=VTIMER, 32=VPL011, 255=ALL"
	@echo "  OPTIMIZATION=<n> - Optimization level (default: 0)"
	@echo "  CPU_AFFINITY=<cores> - Bind QEMU to specific CPU cores (e.g., 0,1 or 2-5)"

# ============================================================================
# 特殊目标和依赖
# ============================================================================

.PHONY: all run debug clean distclean app app_clean info help

# 包含依赖文件
-include $(DEPS)

# ============================================================================
# Avatar Hypervisor Build Configuration
# ============================================================================

# 默认配置 - 可以通过环境变量或命令行覆盖
SMP ?= 1
HV ?= 0
LOGGER ?= 1
DEBUG ?= 1
OPTIMIZATION ?= 0

# 工具链配置
TOOL_PREFIX ?= aarch64-linux-musl-
# 备选工具链: aarch64-none-elf-

# 目录配置
BUILD_DIR ?= build
SRC_DIRS := . boot exception exception/gic io mem timer task process spinlock \
            hyper lib fs app guest syscall
INCLUDE_DIRS := include

# 编译器特定配置
CFLAGS_ARCH := -mgeneral-regs-only
CFLAGS_FREESTANDING := -fno-builtin -nostdinc -fno-stack-protector -fno-pie
CFLAGS_DEBUG := $(if $(filter 1,$(DEBUG)),-g -DDEBUG,-DNDEBUG)
CFLAGS_OPT := -O$(OPTIMIZATION)
CFLAGS_DEFINES := -DSMP_NUM=$(SMP) -DHV=$(HV) -D__LOG_LEVEL=$(LOGGER)

# 链接器配置
LDFLAGS_BASE := -nostdlib
ifeq ($(HV),1)
LINKER_SCRIPT := link_hyper.lds
QEMU_VIRT_ARGS := -M virtualization=on
else
LINKER_SCRIPT := link.lds
QEMU_VIRT_ARGS :=
endif

# QEMU配置
QEMU_MACHINE := virt
QEMU_CPU := cortex-a72
QEMU_MEMORY := 4G
QEMU_GIC := 2
QEMU_EXTRA_ARGS ?=

# 构建配置验证
ifeq ($(SMP),)
$(error SMP must be defined)
endif

ifneq ($(HV),0)
ifneq ($(HV),1)
$(error HV must be 0 or 1)
endif
endif

# 调试配置
ifeq ($(DEBUG),1)
CFLAGS_DEBUG += -DDEBUG_VERBOSE
endif

# 性能配置
ifeq ($(OPTIMIZATION),3)
CFLAGS_OPT += -flto
LDFLAGS_BASE += -flto
endif

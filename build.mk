# ============================================================================
# Avatar Hypervisor Advanced Build Configuration
# ============================================================================

# 构建配置预设
CONFIGS := debug release profile

# Debug配置
ifeq ($(CONFIG),debug)
DEBUG := 1
OPTIMIZATION := 0
CFLAGS_EXTRA := -DDEBUG_VERBOSE -fsanitize-address-use-after-scope
endif

# Release配置
ifeq ($(CONFIG),release)
DEBUG := 0
OPTIMIZATION := 2
CFLAGS_EXTRA := -DNDEBUG -fomit-frame-pointer
endif

# Profile配置
ifeq ($(CONFIG),profile)
DEBUG := 1
OPTIMIZATION := 2
CFLAGS_EXTRA := -DPROFILE -pg
LDFLAGS_EXTRA := -pg
endif

# 平台特定配置
PLATFORMS := qemu-virt qemu-raspi3 hardware

ifeq ($(PLATFORM),qemu-virt)
QEMU_MACHINE := virt
QEMU_CPU := cortex-a72
MEMORY_BASE := 0x40000000
endif

ifeq ($(PLATFORM),qemu-raspi3)
QEMU_MACHINE := raspi3b
QEMU_CPU := cortex-a53
MEMORY_BASE := 0x00000000
endif

# 特性开关
FEATURES := 
ifeq ($(ENABLE_SMP),1)
FEATURES += -DENABLE_SMP
endif

ifeq ($(ENABLE_VGIC),1)
FEATURES += -DENABLE_VGIC
endif

ifeq ($(ENABLE_VTIMER),1)
FEATURES += -DENABLE_VTIMER
endif

# 安全编译选项（注意：某些选项可能与裸机环境不兼容）
SECURITY_FLAGS := -fstack-protector-strong -D_FORTIFY_SOURCE=2
ifeq ($(ENABLE_SECURITY),1)
CFLAGS_EXTRA += $(SECURITY_FLAGS)
endif

# 注意：避免在CFLAGS_EXTRA中添加带空格的参数，这会影响app构建

# 代码覆盖率
ifeq ($(ENABLE_COVERAGE),1)
CFLAGS_EXTRA += --coverage
LDFLAGS_EXTRA += --coverage
endif

# 静态分析
STATIC_ANALYSIS_TOOLS := cppcheck clang-tidy

.PHONY: analyze
analyze:
	@echo "Running static analysis..."
	@cppcheck --enable=all --inconclusive --std=c11 $(C_SOURCES)
	@clang-tidy $(C_SOURCES) -- $(CFLAGS) $(INCLUDE)

# 代码格式化
.PHONY: format
format:
	@echo "Formatting code..."
	@clang-format -i $(C_SOURCES) $(shell find include guest -name "*.h" | grep -v -E "(guest/linux/|guest/nimbos/|guest/testos/|clib/)")

# 文档生成
.PHONY: docs
docs:
	@echo "Generating documentation..."
	@doxygen Doxyfile

# 测试目标
.PHONY: test
test: $(BUILD_DIR)/kernel.bin
	@echo "Running tests..."
	@python3 scripts/test_runner.py

# 性能分析
.PHONY: perf
perf: $(BUILD_DIR)/kernel.bin
	@echo "Running performance analysis..."
	@qemu-system-aarch64 $(QEMU_ARGS) -kernel $< -monitor stdio

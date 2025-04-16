

# 第二层 Makefile：扫描子目录或构建指定 app

TOOL_PREFIX ?= aarch64-none-elf-
CFLAGS ?= -c -nostdlib -nostartfiles -ffreestanding -O2
INCLUDE ?=
APP_NAME ?=

# 所有子目录（含 Makefile）
APP_SUBDIRS := $(shell find . -mindepth 1 -maxdepth 1 -type d -exec test -f "{}/Makefile" \; -print | sed 's|^\./||')


.PHONY: all clean $(APP_SUBDIRS)

# 默认构建所有 app 或指定 app
all: $(if $(APP_NAME),$(APP_NAME),$(APP_SUBDIRS)) app.S info.lds app.h

$(APP_SUBDIRS):
	$(MAKE) -C $@ TOOL_PREFIX=$(TOOL_PREFIX) CFLAGS="$(CFLAGS)" INCLUDE="$(INCLUDE)"

app.S: $(foreach dir,$(APP_SUBDIRS),$(dir)/build/app.bin)
	@echo "create app.S..."
	@echo "/* auto created */" > app.S
	@$(foreach dir,$(APP_SUBDIRS), \
		echo '.section .data.$(dir)_bin' >> app.S; \
		echo '    .incbin "../app/$(dir)/build/app.elf"' >> app.S; \
		echo '' >> app.S;)

# 生成 lds 文件
info.lds: $(foreach app, $(APP_SUBDIRS), $(app)/build/app.bin)
	@echo "/* Automatically generated lds file */" > info.lds
	@for app in $(APP_SUBDIRS); do \
		section=".data.$${app}_bin"; \
		echo "__$${app}_bin_start = .;" >> info.lds; \
		echo "*($$section)" >> info.lds; \
		echo "__$${app}_bin_end = .;" >> info.lds; \
		echo ". = ALIGN((1 << 10));" >> info.lds; \
		echo "" >> info.lds; \
	done


app.h:
	@echo "/* Automatically generated app.h */" > app.h
	@echo "#ifndef __APP_H__" >> app.h
	@echo "#define __APP_H__" >> app.h
	@echo "" >> app.h
	@echo "#ifdef __cplusplus" >> app.h
	@echo "extern \"C\" {" >> app.h
	@echo "#endif" >> app.h
	@for app in $(APP_SUBDIRS); do \
		echo "extern void __$${app}_bin_start();" >> app.h; \
		echo "extern void __$${app}_bin_end();" >> app.h; \
	done
	@echo "" >> app.h
	@echo "#ifdef __cplusplus" >> app.h
	@echo "}" >> app.h
	@echo "#endif" >> app.h
	@echo "" >> app.h
	@echo "#endif // __APP_H__" >> app.h

clean:
	@if [ -n "$(APP_NAME)" ]; then \
		$(MAKE) -C $(APP_NAME) clean; \
	else \
		for dir in $(APP_SUBDIRS); do \
			$(MAKE) -C $$dir clean; \
		done \
	fi
	rm -rf app.S
	rm -rf info.lds
	rm -rf app.h
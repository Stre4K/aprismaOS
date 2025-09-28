HOST?=$(DEFAULT_HOST)
HOSTARCH?=i386

CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=
# -------------------------
# Paths
# -------------------------
SRC_DIR ?= $(ROOTDIR)/src/kernel
OBJ_DIR ?= $(OBJ_DIR)/kernel
ARCHDIR ?= $(SRC_DIR)/arch/$(HOSTARCH)
SYSROOT ?= $(SYSROOT)

CFLAGS   := $(CFLAGS) -ffreestanding -Wall -Wextra -nostdlib -nostdinc -Werror
CPPFLAGS := $(CPPFLAGS) -D__is_kernel -Iinclude -I$(SYSROOT)/include

include $(ARCHDIR)/make.config

CFLAGS   += $(KERNEL_ARCH_CFLAGS)
CPPFLAGS += $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS  += $(KERNEL_ARCH_LDFLAGS)
LIBS     += $(KERNEL_ARCH_LIBS)

# Kernel sources and objects
KERNEL_SRCS := $(shell find $(SRC_DIR)/kernel -name '*.c') \
               $(shell find $(SRC_DIR)/arch/$(HOSTARCH) -name '*.c' -o -name '*.S')
KERNEL_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(KERNEL_SRCS))
KERNEL_OBJS := $(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/%.o,$(KERNEL_OBJS))

BIN := aprisma.kernel
LIBC_A := $(SYSROOT)/lib/libc.a

.PHONY: all clean install install-headers install-kernel

all: $(BIN)

# Colors
BLUE  := \033[0;34m
GREEN := \033[0;32m
GREEN_BOLD:= \033[1;32m
YELLOW:= \033[0;33m
RESET := \033[0m

# -------------------------
# Build kernel
# -------------------------
$(BIN): $(KERNEL_OBJS) $(ARCHDIR)/linker.ld $(LIBC_A)
	@mkdir -p $(dir $@)
	@$(CC) -nostdlib -ffreestanding \
		-T $(ARCHDIR)/linker.ld \
		-o $@ \
		$(KERNEL_OBJS) \
		$(LIBC_A) \
		-lgcc \
		$(LIBS)
	@echo "$(GREEN_BOLD)[kernel] Kernel done compiling!$(RESET)"

# -------------------------
# Compile .c files
# -------------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@REL_SRC=$(patsubst $(SRC_DIR)/%,%,$<) ; \
	echo "$(BLUE)[kernel] Compiling $$REL_SRC$(RESET)"
	@$(CC) -MD -MP -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

# -------------------------
# Compile .S files
# -------------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	@REL_SRC=$(patsubst $(SRC_DIR)/%,%,$<) ; \
	echo "$(BLUE)[kernel] Compiling $$REL_SRC$(RESET)"
	@$(CC) -MD -MP -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# -------------------------
# Clean
# -------------------------
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(BIN)

# -------------------------
# Install
# -------------------------
install: install-headers install-kernel

#install-headers:
#	@mkdir -p $(SYSROOT)/include
#	@cp -R -p $(SRC_DIR)/include/. $(SYSROOT)/include/
install-headers:
	@if [ ! -d $(SYSROOT)/include/kernel ]; then \
		echo "$(YELLOW)[kernel] Copying headers$(RESET)"; \
		mkdir -p $(SYSROOT)/include >/dev/null;\
		cp -R -p $(SRC_DIR)/include/. $(SYSROOT)/include/ >/dev/null; \
	fi

install-kernel: $(BIN)
	@mkdir -p $(SYSROOT)/boot
	@cp $(BIN) $(SYSROOT)/boot

-include $(KERNEL_OBJS:.o=.d)


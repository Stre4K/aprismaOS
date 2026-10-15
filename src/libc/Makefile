# =========================
# AprismaOS libc Makefile
# =========================

HOST?=$(DEFAULT_HOST)
HOSTARCH?=i386

CFLAGS?=-O2 -g
CPPFLAGS?=
DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
INCLUDEDIR?=$(PREFIX)/include
LIBDIR?=$(EXEC_PREFIX)/lib

CFLAGS   := $(CFLAGS) -ffreestanding -Wall -Wextra -nostdlib -nostdinc -Werror
CPPFLAGS := $(CPPFLAGS) -D__is_libc -I $(SYSROOT)/include
LIBK_CFLAGS := $(CFLAGS)
LIBK_CPPFLAGS := $(CPPFLAGS) -D__is_libk

# -------------------------
# Paths
# -------------------------
# Absolute path to this Makefile
SRC_DIR := $(ROOTDIR)/src/libc
OBJ_DIR := $(OBJ_DIR)/libc
SYSROOT := $(SYSROOT)
ARCHDIR := $(SRC_DIR)/arch/$(HOSTARCH)

# Include architecture-specific flags
include $(ARCHDIR)/make.config

CFLAGS       += $(ARCH_CFLAGS)
CPPFLAGS     += $(ARCH_CPPFLAGS)
LIBK_CFLAGS  += $(KERNEL_ARCH_CFLAGS)
LIBK_CPPFLAGS += $(KERNEL_ARCH_CPPFLAGS)

# -------------------------
# Source files
# -------------------------
SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
# An arch can leave files out of libk with LIBK_EXCLUDE (paths relative to src/libc)
LIBK_SRCS := $(filter-out $(addprefix $(SRC_DIR)/,$(LIBK_EXCLUDE)),$(SRCS))
LIBK_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.libk.o,$(LIBK_SRCS))

BINARIES := $(OBJ_DIR)/libc.a $(OBJ_DIR)/libk.a

# -------------------------
# Targets
# -------------------------
.PHONY: all clean install install-headers install-libs

all: $(BINARIES)

# -------------------------
# Colors
# -------------------------
GREEN   := \033[0;32m
CYAN 	:= \033[0;36m
MAGENTA := \033[0;35m
BLUE    := \033[0;34m
YELLOW  := \033[0;33m
RESET   := \033[0m

# -------------------------
# Build libraries
# -------------------------
$(OBJ_DIR)/libc.a: $(OBJS)
	@echo  "$(GREEN)[libc] Archiving libc.a$(RESET)"
	@mkdir -p $(dir $@)
	@$(AR) rcs $@ $(OBJS)

$(OBJ_DIR)/libk.a: $(LIBK_OBJS)
	@echo "$(GREEN)[kernel] Archiving libk.a$(RESET)"
	@mkdir -p $(dir $@)
	@$(AR) rcs $@ $(LIBK_OBJS)

# -------------------------
# Compile rules
# -------------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "$(MAGENTA)[libc] Compiling $(notdir $<)$(RESET)"
	@$(CC) -MD -MP -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

$(OBJ_DIR)/%.libk.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "$(BLUE)[kernel] Compiling $(notdir $<)$(RESET)"
	@$(CC) -MD -MP -c $< -o $@ -std=gnu11 $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

# -------------------------
# Clean
# -------------------------
clean:
	@echo "$(YELLOW)[build] Cleaning object files and binaries...$(RESET)"
	rm -rf $(OBJ_DIR)
	rm -f $(BINARIES)

# -------------------------
# Installation
# -------------------------
install: install-headers install-libs

#install-headers:
#	@echo -e "$(YELLOW)[install] Copying headers...$(RESET)"
#	mkdir -p $(SYSROOT)/include
#	cp -R -p $(SRC_DIR)/include/. $(SYSROOT)/include/
# Copy on every build so header edits reach the sysroot (cp -p keeps the
# timestamps, so unchanged headers do not trigger rebuilds)
install-headers:
	@echo "$(YELLOW)[libc] Copying headers$(RESET)"
	@mkdir -p $(SYSROOT)/include
	@cp -R -p $(SRC_DIR)/include/. $(SYSROOT)/include/

install-libs: $(BINARIES)
	@echo "$(YELLOW)[install] Copying libraries$(RESET)"
	@mkdir -p $(SYSROOT)/lib
	@cp $(BINARIES) $(SYSROOT)/lib

# -------------------------
# Include dependency files
# -------------------------
-include $(OBJS:.o=.d)
-include $(LIBK_OBJS:.libk.o=.d)


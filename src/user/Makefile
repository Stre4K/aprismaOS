# =========================
# AprismaOS user programs
# =========================
#
# Every programs/<name>.c becomes a statically linked ELF executable. They
# go into the ramdisk (initrd.tar) as /bin/<name>, together with everything
# in src/initrd/; the bootloader loads it next to the kernel and
# `run <name>` starts a program from it. Only x86_64 has user mode so far;
# elsewhere the ramdisk holds just the files from src/initrd/.

HOSTARCH ?= i386

SRC_DIR := $(ROOTDIR)/src/user
OBJ_DIR := $(OBJ_DIR)/user
SYSROOT := $(SYSROOT)

YELLOW := \033[0;33m
GREEN  := \033[0;32m
CYAN   := \033[0;36m
RESET  := \033[0m

.PHONY: all install install-headers clean

ifeq ($(HOSTARCH),x86_64)

PROGRAMS := $(sort $(basename $(notdir $(wildcard $(SRC_DIR)/programs/*.c))))

# Programs may use x87/SSE: the kernel saves those registers per process.
# No errno in the runtime, so math builtins (e.g. sqrt) compile to plain
# instructions instead of library calls. The runtime's memset and memcpy
# are loops that gcc must not turn back into calls to themselves.
CFLAGS  := -O2 -g -std=gnu11 -ffreestanding -fno-pie -fno-math-errno -fno-tree-loop-distribute-patterns \
           -Wall -Wextra -Werror -I$(SRC_DIR)/lib -I$(ROOTDIR)/src/kernel/include
LDFLAGS := -nostdlib -static -no-pie -z max-page-size=0x1000 -T $(SRC_DIR)/user.ld

LIB_OBJS := $(OBJ_DIR)/lib/start.o $(OBJ_DIR)/lib/ulib.o $(OBJ_DIR)/lib/malloc.o

ELFS     := $(patsubst %,$(OBJ_DIR)/bin/%.elf,$(PROGRAMS))

# Objects also depend on this Makefile, so changing CFLAGS rebuilds them
$(OBJ_DIR)/lib/%.o: $(SRC_DIR)/lib/%.c $(SRC_DIR)/Makefile
	@mkdir -p $(dir $@)
	@echo "$(CYAN)[user] Compiling $(notdir $<)$(RESET)"
	@$(CC) $(CFLAGS) -MD -MP -c $< -o $@

$(OBJ_DIR)/lib/%.o: $(SRC_DIR)/lib/%.S $(SRC_DIR)/Makefile
	@mkdir -p $(dir $@)
	@echo "$(CYAN)[user] Compiling $(notdir $<)$(RESET)"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/programs/%.o: $(SRC_DIR)/programs/%.c $(SRC_DIR)/Makefile
	@mkdir -p $(dir $@)
	@echo "$(CYAN)[user] Compiling $(notdir $<)$(RESET)"
	@$(CC) $(CFLAGS) -MD -MP -c $< -o $@

$(OBJ_DIR)/bin/%.elf: $(OBJ_DIR)/programs/%.o $(LIB_OBJS) $(SRC_DIR)/user.ld
	@mkdir -p $(dir $@)
	@echo "$(CYAN)[user] Linking $(notdir $@)$(RESET)"
	@$(CC) $(LDFLAGS) -o $@ $(LIB_OBJS) $< -lgcc

-include $(wildcard $(OBJ_DIR)/lib/*.d $(OBJ_DIR)/programs/*.d)

else

PROGRAMS :=
ELFS     :=

endif

# ---- the ramdisk ----
INITRD_SRC   := $(ROOTDIR)/src/initrd
INITRD_DIR   := $(OBJ_DIR)/initrd
INITRD       := $(OBJ_DIR)/initrd.tar
STATIC_FILES := $(shell find $(INITRD_SRC) -type f)

all: $(INITRD)

# ustar is the format the kernel reads; --format is understood by GNU tar
# and bsdtar (macOS) alike
$(INITRD): $(ELFS) $(STATIC_FILES) $(SRC_DIR)/Makefile
	@echo "$(GREEN)[user] Building ramdisk ($(words $(PROGRAMS)) programs)$(RESET)"
	@rm -rf $(INITRD_DIR)
	@mkdir -p $(INITRD_DIR)/bin
	@cp -R $(INITRD_SRC)/. $(INITRD_DIR)/
	@for p in $(PROGRAMS); do cp $(OBJ_DIR)/bin/$$p.elf $(INITRD_DIR)/bin/$$p; done
	@tar --format=ustar -cf $@ -C $(INITRD_DIR) .

install: $(INITRD)
	@mkdir -p $(SYSROOT)/boot
	@cp $(INITRD) $(SYSROOT)/boot/initrd.tar

install-headers:

clean:
	rm -rf $(OBJ_DIR)

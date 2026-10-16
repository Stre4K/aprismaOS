# Host-side unit tests for AprismaOS
#
#   make -C tests/unit            build and run all tests
#   make -C tests/unit run T=pow  run tests whose name contains "pow"
#
# The code under test is compiled from the real sources with the host gcc as
# freestanding code against AprismaOS's own headers (the libk flavour the
# kernel links), then every symbol gets an "aprisma_" prefix so it can live
# next to the host C library in one test binary.

ROOTDIR   := $(abspath ../..)
BUILD_DIR ?= $(ROOTDIR)/build/unit-tests

CC      ?= gcc
OBJCOPY ?= objcopy

# Code under test: libc (without putchar, which capture.c provides, and the
# busy-wait sleep), pure kernel helpers and the user-space allocator
LIBC_DIR    := $(ROOTDIR)/src/libc
LIBC_SRCS   := $(filter-out $(LIBC_DIR)/stdio/putchar.c $(LIBC_DIR)/time/sleep.c, \
                 $(shell find $(LIBC_DIR) -name '*.c'))
KERNEL_SRCS := $(ROOTDIR)/src/kernel/core/bitmap.c $(ROOTDIR)/src/kernel/fs/tar.c
# The user programs' allocator (its sbrk, print and exit come from test_malloc.c)
USER_SRCS   := $(ROOTDIR)/src/user/lib/malloc.c
UUT_SRCS    := $(LIBC_SRCS) $(KERNEL_SRCS) $(USER_SRCS)
UUT_OBJS    := $(patsubst $(ROOTDIR)/%.c,$(BUILD_DIR)/uut/%.o,$(UUT_SRCS))

# -fno-tree-loop-distribute-patterns keeps gcc from turning loops inside
# memset/memcpy into calls to themselves; no stack protector because the
# prefixed __stack_chk_fail would not resolve
UUT_CFLAGS := -std=gnu11 -O2 -g -ffreestanding -fno-builtin -nostdinc \
              -fno-stack-protector -fno-tree-loop-distribute-patterns \
              -D__is_libk -I$(LIBC_DIR)/include -I$(ROOTDIR)/src/kernel/include -Wall -Wextra -Werror

TEST_SRCS  := $(wildcard test_*.c) runner.c capture.c
TEST_OBJS  := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))
TEST_CFLAGS := -std=gnu11 -O1 -g -Wall -Wextra -Werror -I$(ROOTDIR)/src/kernel/include

BIN := $(BUILD_DIR)/unit-tests

.PHONY: all run clean

all: run

run: $(BIN)
	@$(BIN) $(T)

$(BIN): $(TEST_OBJS) $(UUT_OBJS)
	@$(CC) -o $@ $^ -lm

# Compile the code under test, then prefix all of its symbols
$(BUILD_DIR)/uut/%.o: $(ROOTDIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(UUT_CFLAGS) -c $< -o $@.tmp
	@$(OBJCOPY) --prefix-symbols=aprisma_ $@.tmp $@
	@rm -f $@.tmp

$(BUILD_DIR)/%.o: %.c test.h aprisma.h
	@mkdir -p $(dir $@)
	@$(CC) $(TEST_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

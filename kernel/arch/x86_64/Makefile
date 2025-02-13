CC=x86_64-elf-gcc
SHARED_FLAGS = -fno-builtin -O2 -nostdinc -nostdlib -ffreestanding -g -Wall -Wextra \
               -Werror -I. -MMD -mno-red-zone -mcmodel=kernel -fno-pie
CFLAGS = $(SHARED_FLAGS)
ASFLAGS = $(SHARED_FLAGS) -Wa,--divide

OBJS := boot.o
OBJS += kernel.o

DFILES = $(patsubst %.o,%.d,$(OBJS))

all: kernel iso

kernel: $(OBJS) kernel.ld Makefile
	$(CC) -z max-page-size=0x1000 $(CFLAGS) -no-pie -Wl,--build-id=none -T kernel.ld -o $@ $(OBJS)

iso:
	mkisofs -o kernel.iso -b kernel -no-emul-boot -boot-load-size 4 -boot-info-table -J -R kernel

clean:
	find . -name "*~" -delete
	rm -rf $(OBJS) $(DFILES) kernel kernel.iso

$(OBJS): Makefile
-include $(DFILES)

run:
	qemu-system-x86_64 -cdrom kernel.iso

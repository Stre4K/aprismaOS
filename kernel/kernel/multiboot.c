#include <kernel/multiboot.h>
#include <kernel/printk.h>  // needed for printk

void print_multiboot_info(multiboot_info_t *mbi) {
    printk("=== Multiboot Info ===\n");

    if (mbi->flags & 0x1) {
        printk("Lower memory: %u KB\n", mbi->mem_lower);
        printk("Upper memory: %u KB\n", mbi->mem_upper);
    }

    if (mbi->flags & 0x2) {
        printk("Boot device: %x\n", mbi->boot_device);
    }

    if (mbi->flags & 0x4 && mbi->cmdline) {
        printk("Command line: %s\n", (char *)mbi->cmdline);
    }

    if (mbi->flags & 0x8 && mbi->mods_count > 0) {
        multiboot_mod_t *mods = (multiboot_mod_t *)mbi->mods_addr;
        printk("Modules loaded: %u\n", mbi->mods_count);
        for (uint32_t i = 0; i < mbi->mods_count; i++) {
            printk("Module %u: %x - %x\n", i, mods[i].mod_start,
                   mods[i].mod_end);
            if (mods[i].cmdline) {
                printk("Module cmdline: %s\n", (char *)mods[i].cmdline);
            }
        }
    }

    if (mbi->flags & 0x40 && mbi->mmap_length > 0) {
        multiboot_mmap_entry_t *mmap = (multiboot_mmap_entry_t *)mbi->mmap_addr;
        uint32_t end = mbi->mmap_addr + mbi->mmap_length;
        printk("Memory map:\n");

        while ((uint32_t)mmap < end) {
            printk("%llx - %llx, type=%u\n", mmap->addr, mmap->addr + mmap->len,
                   mmap->type);
            mmap = (multiboot_mmap_entry_t *)((uint8_t *)mmap + mmap->size + 4);
        }
    }

    // Bootloader name
    if (mbi->flags & (1 << 9) && mbi->boot_loader_name) {
        printk("Bootloader: %s\n", (char *)mbi->boot_loader_name);
    }

    // Drives
    if (mbi->flags & (1 << 7) && mbi->drives_addr) {
        uint8_t *drive_ptr = (uint8_t *)mbi->drives_addr;
        uint32_t drives_end = mbi->drives_addr + mbi->drives_length;
        printk("Drives info:\n");
        while ((uint32_t)drive_ptr < drives_end) {
            multiboot_drive_t *drv = (multiboot_drive_t *)drive_ptr;
            printk(
                "Drive 0x%x: mode=0x%x, cylinders=%u, heads=%u, sectors=%u\n",
                drv->drive_number, drv->drive_mode, drv->drive_cylinders,
                drv->drive_heads, drv->drive_sectors);
            drive_ptr += 4 + drv->drive_ports[0] * 2;  // rough estimation
        }
    }

    // VBE/graphics info
    if (mbi->flags & (1 << 11) && mbi->vbe_control_info) {
        multiboot_vbe_info_t *vbe =
            (multiboot_vbe_info_t *)mbi->vbe_control_info;
        printk("VBE Info: %ux%u, %u bpp, framebuffer at 0x%llx\n", vbe->width,
               vbe->height, vbe->bpp, vbe->framebuffer_addr);
    }

    printk("=== End of Multiboot Info ===\n");
}

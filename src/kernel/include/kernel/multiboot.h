#ifndef _KERNEL_MULTIBOOT_H
#define _KERNEL_MULTIBOOT_H

#include <stdint.h>

/* Multiboot1 info structure */
typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];  // a.out/ELF info
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;     // flag 7
    uint32_t drives_addr;       // flag 7
    uint32_t config_table;      // flag 8
    uint32_t boot_loader_name;  // flag 9
    uint32_t apm_table;         // flag 10
    uint32_t vbe_control_info;  // flag 11
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t framebuffer_type;
    uint32_t color_info;
} __attribute__((packed)) multiboot_info_t;

/* Multiboot1 memory map entry */
typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

/* Multiboot1 module structure */
typedef struct multiboot_mod {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
} __attribute__((packed)) multiboot_mod_t;

typedef struct multiboot_drive {
    uint8_t drive_number;
    uint8_t drive_mode;
    uint16_t drive_cylinders;
    uint8_t drive_heads;
    uint8_t drive_sectors;
    uint8_t drive_ports[0];  // variable-sized
} __attribute__((packed)) multiboot_drive_t;

typedef struct multiboot_vbe_info {
    uint16_t mode;
    uint16_t interface_seg;
    uint16_t interface_off;
    uint16_t interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
} __attribute__((packed)) multiboot_vbe_info_t;

/* Function declaration */
void print_multiboot_info(multiboot_info_t *mbi);

#endif

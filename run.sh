#!/bin/sh
set -e


show_help() {
    cat << EOF
Usage: $0 [MODE]

Modes:
  kernel     Run aprisma.kernel directly in QEMU (default)
  iso        Run aprisma.iso with GRUB in QEMU
  -h, --help Show this help message
EOF
}

# Check for help before doing anything
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_help
    exit 0
fi


# Paths
KernelPATH="$PWD/sysroot/boot/aprisma.kernel"
ISO="$PWD/aprisma.iso"

# Default mode
MODE=${1:-kernel}   # Accepts "kernel" or "iso" as argument, default is "kernel"

case "$MODE" in
    kernel)
        echo "Running kernel directly in QEMU..."
        qemu-system-i386 -kernel "$KernelPATH" -monitor stdio
        ;;
    iso)
        echo "Running ISO with GRUB in QEMU..."
        qemu-system-i386 -cdrom "$ISO" -boot d -monitor stdio
        ;;
    *)
        echo "Unknown mode: $MODE"
        show_help
        exit 1
        ;;
esac


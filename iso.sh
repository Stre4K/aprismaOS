#!/bin/sh
set -e


show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
  --kernel KERNEL_PATH       Specify the kernel to include (default: sysroot/boot/aprisma.kernel)
  --output ISO_PATH          Specify the output ISO filename (default: aprisma.iso)
  --homebrew-grub            Use homebrew GRUB instead of system default
  -h, --help                 Show this help message
EOF
}

# Check for help before doing anything
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_help
    exit 0
fi

# Build kernel
. ./build.sh

# Default values
KERNEL=sysroot/boot/aprisma.kernel
ISO_OUTPUT=aprisma.iso
GRUBDIR=  # empty by default, will use system grub-mkrescue

# Parse optional arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --kernel)
            KERNEL="$2"
            shift 2
            ;;
        --output)
            ISO_OUTPUT="$2"
            shift 2
            ;;
        --homebrew-grub)
            # Change this to your homebrew path
            GRUBDIR=/opt/homebrew/Cellar/i686-elf-grub/2.12/bin
            shift
            ;;
        *)
            echo "Usage: $0 [--kernel KERNEL_PATH] [--output ISO_PATH] [--homebrew-grub]"
            exit 1
            ;;
    esac
done

# Create ISO structure
mkdir -p isodir/boot/grub
cp "$KERNEL" isodir/boot/aprisma.kernel

# Create GRUB config
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "aprisma" {
    multiboot /boot/aprisma.kernel root=/dev/null
}
EOF

# Build ISO
if [ -n "$GRUBDIR" ]; then
    # Use homebrew GRUB
    "$GRUBDIR/i686-elf-grub-mkrescue" -o "$ISO_OUTPUT" isodir
else
    # Use system default grub-mkrescue
    grub-mkrescue -o "$ISO_OUTPUT" isodir
fi

echo "ISO created: $ISO_OUTPUT"


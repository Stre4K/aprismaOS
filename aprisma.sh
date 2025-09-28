#!/bin/sh
set -e

# ============================
# AprismaOS Unified Build Script
# ============================

show_help() {
cat << EOF
Usage: $0 [OPTIONS]

Options:
  --compiler COMPILER        Select cross-compiler (i686-elf-gcc or x86_64-elf-gcc, default: i686-elf-gcc)
  --kernel KERNEL_PATH       Kernel path for ISO (default: build/AprismaOS/sysroot/boot/aprisma.kernel)
  --output ISO_PATH          Output ISO filename (default: aprisma.iso)
  --homebrew-grub            Use homebrew GRUB instead of system default
  --clean                    Clean previous build (build/, sysroot, ISO)
  --build [iso|kernel]       Build ISO (default) or only kernel
  --run [kernel|iso]         Run QEMU after building
  --run-path PATH            Path to kernel or ISO when using --run
  -h, --help                 Show this help message
EOF
}

# ============================
# Defaults
# ============================
COMPILER=i686-elf-gcc
ROOTDIR=$(pwd)                   # top-level project directory
SYSROOT=$ROOTDIR/build/AprismaOS/sysroot
OBJ_DIR=$ROOTDIR/build/objects
ISO_OUTPUT=$ROOTDIR/build/aprisma.iso
GRUBDIR=
RUN_MODE=
RUN_PATH=
BUILD_TYPE=""  # empty means no build
CLEAN=0
MAKE=${MAKE:-make}

SYSTEM_HEADER_PROJECTS="libc kernel"
PROJECTS="libc kernel"
KERNEL_PATH="$SYSROOT/boot/aprisma.kernel"

# ============================
# Show help if no options provided
# ============================
if [ $# -eq 0 ] && [ -z "$BUILD_TYPE" ] && [ -z "$RUN_MODE" ] && [ "$CLEAN" = "0" ]; then
    echo "No options provided."
    show_help
    exit 0
fi

# ============================
# Parse arguments
# ============================
while [ $# -gt 0 ]; do
    case "$1" in
        --build)
            BUILD_TYPE="${2:-iso}"
            shift 2
            ;;
        --kernel)
            KERNEL_PATH="$2"
            shift 2
            ;;
        --output)
            ISO_OUTPUT="$2"
            shift 2
            ;;
        --compiler)
            COMPILER="$2"
            shift 2
            ;;
        --run)
            RUN_MODE="$2"
            shift 2
            ;;
        --run-path)
            RUN_PATH="$2"
            shift 2
            ;;
        --homebrew-grub)
            GRUBDIR=/opt/homebrew/Cellar/i686-elf-grub/2.12/bin
            shift
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# ============================
# Environment setup
# ============================
case "$COMPILER" in
    i686-elf-gcc) HOST=i686-elf ;;
    x86_64-elf-gcc) HOST=x86_64-elf ;;
    *) echo "Invalid compiler: $COMPILER"; exit 1 ;;
esac

export CC="${HOST}-gcc"
export AS="${HOST}-as"
export AR="${HOST}-ar"
export CFLAGS='-O2 -g'
export CPPFLAGS=''

# ============================
# Colors
# ============================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

# ============================
# Clean previous build
# ============================
if [ "$CLEAN" = "1" ]; then
    echo "${YELLOW}Cleaning previous build...${NC}"
    rm -rf build build/AprismaOS build/objects "$ISO_OUTPUT"
        for PROJECT in $PROJECTS; do
            (cd "$ROOTDIR/src/$PROJECT" && \
                ROOTDIR="$ROOTDIR" \
                SYSROOT="$SYSROOT" \
                OBJ_DIR="$OBJ_DIR" \
                $MAKE clean)
        done
        exit 0
    fi

# ============================
# Build
# ============================
if [ -n "$BUILD_TYPE" ]; then
    echo "${GREEN}[status] Installing headers${NC}"
    mkdir -p "$SYSROOT"
    for PROJECT in $SYSTEM_HEADER_PROJECTS; do
    (cd "$ROOTDIR/src/$PROJECT" && \
        ROOTDIR="$ROOTDIR" \
        SYSROOT="$SYSROOT" \
        OBJ_DIR="$OBJ_DIR" \
        $MAKE install-headers)
    done

    echo "${GREEN}[status] Building projects${NC}"
    for PROJECT in $PROJECTS; do
    (cd "$ROOTDIR/src/$PROJECT" && \
        ROOTDIR="$ROOTDIR" \
        SYSROOT="$SYSROOT" \
        OBJ_DIR="$OBJ_DIR" \
        $MAKE install)
    done
fi


# ============================
# Create ISO if needed
# ============================
if [ "$BUILD_TYPE" = "iso" ]; then
    echo "${YELLOW}[status] Creating ISO${NC}"
    ISO_DIR=build/isodir
    mkdir -p "$ISO_DIR/boot/grub"
    cp "$KERNEL_PATH" "$ISO_DIR/boot/aprisma.kernel"

    cat > "$ISO_DIR/boot/grub/grub.cfg" << EOF
menuentry "aprisma" {
    multiboot /boot/aprisma.kernel root=/dev/null
}
EOF

    if [ -n "$GRUBDIR" ]; then
        "$GRUBDIR/i686-elf-grub-mkrescue" -o "$ISO_OUTPUT" "$ISO_DIR" > /dev/null
    else
        grub-mkrescue -o "$ISO_OUTPUT" "$ISO_DIR" > /dev/null
    fi
    echo "${GREEN}[status] ISO created: $ISO_DIR/aprisma.iso${NC}"
fi

# ============================
# Optionally run QEMU
# ============================
if [ -n "$RUN_MODE" ]; then
    case "$RUN_MODE" in
        kernel)
            RUN_KERNEL=${RUN_PATH:-"$KERNEL_PATH"}
            echo "Running kernel in QEMU: $RUN_KERNEL"
            qemu-system-i386 -kernel "$RUN_KERNEL" -monitor stdio
            ;;
        iso)
            RUN_ISO=${RUN_PATH:-"$ISO_OUTPUT"}
            echo "Running ISO in QEMU: $RUN_ISO"
            qemu-system-i386 -cdrom "$RUN_ISO" -boot d -monitor stdio
            ;;
        *)
            echo "Unknown run mode: $RUN_MODE"
            exit 1
            ;;
    esac
fi


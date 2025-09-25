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
  --kernel KERNEL_PATH       Kernel path for ISO (default: sysroot/boot/aprisma.kernel)
  --output ISO_PATH          Output ISO filename (default: aprisma.iso)
  --homebrew-grub            Use homebrew GRUB instead of system default
  --clean                    Clean previous build (sysroot, isodir, binaries)
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
KERNEL=sysroot/boot/aprisma.kernel
ISO_OUTPUT=aprisma.iso
GRUBDIR=
RUN_MODE=
RUN_PATH=
BUILD_TYPE=""  # empty means no build
CLEAN=0
MAKE=${MAKE:-make}

SYSTEM_HEADER_PROJECTS="libc kernel"
PROJECTS="libc kernel"
SYSROOT=$(pwd)/sysroot

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
            if [ -n "$2" ] && ! echo "$2" | grep -q '^--'; then
                case "$2" in
                    iso|kernel)
                        BUILD_TYPE="$2"
                        shift 2
                        ;;
                    *)
                        echo "Invalid argument for --build: $2 (expected 'iso' or 'kernel')"
                        exit 1
                        ;;
                esac
            else
                BUILD_TYPE="iso"
                shift
            fi
            ;;
        --kernel)
            if [ -z "$2" ] || echo "$2" | grep -q '^--'; then
                echo "Error: --kernel requires a value"
                exit 1
            fi
            KERNEL="$2"
            shift 2
            ;;
        --output)
            if [ -z "$2" ] || echo "$2" | grep -q '^--'; then
                echo "Error: --output requires a value"
                exit 1
            fi
            ISO_OUTPUT="$2"
            shift 2
            ;;
        --compiler)
            if [ -z "$2" ] || echo "$2" | grep -q '^--'; then
                echo "Error: --compiler requires a value"
                exit 1
            fi
            COMPILER="$2"
            shift 2
            ;;
        --run)
            if [ -n "$2" ] && ! echo "$2" | grep -q '^--'; then
                RUN_MODE="$2"
                shift 2
            else
                echo "Error: --run requires 'kernel' or 'iso'"
                exit 1
            fi
            ;;
        --run-path)
            if [ -n "$2" ] && ! echo "$2" | grep -q '^--'; then
                RUN_PATH="$2"
                shift 2
            else
                echo "Error: --run-path requires a value"
                exit 1
            fi
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
    *)
        echo "Invalid compiler: $COMPILER"
        exit 1
        ;;
esac

export CC="${HOST}-gcc"
export AS="${HOST}-as"
export AR="${HOST}-ar"
export CFLAGS='-O2 -g'
export CPPFLAGS=''
export PREFIX=/usr
export EXEC_PREFIX=$PREFIX
export BOOTDIR=/boot
export LIBDIR=$EXEC_PREFIX/lib
export INCLUDEDIR=$PREFIX/include

export CC="$CC --sysroot=$SYSROOT"
if echo "$HOST" | grep -Eq -- '-elf($|-)'; then
    export CC="$CC -isystem=$INCLUDEDIR"
fi

# ============================
# Clean previous build
# ============================

if [ "$CLEAN" = "1" ]; then
    echo "Cleaning previous build..."
    for PROJECT in $PROJECTS; do
        (cd "$PROJECT" && $MAKE clean)
    done
    rm -rf "$SYSROOT" isodir "$ISO_OUTPUT"
    exit 0
fi

# ============================
# Build
# ============================

if [ -n "$BUILD_TYPE" ]; then
    echo "Installing headers..."
    mkdir -p "$SYSROOT"
    for PROJECT in $SYSTEM_HEADER_PROJECTS; do
        (cd "$PROJECT" && DESTDIR="$SYSROOT" $MAKE install-headers)
    done

    echo "Building projects..."
    for PROJECT in $PROJECTS; do
        (cd "$PROJECT" && DESTDIR="$SYSROOT" $MAKE install)
    done
fi

# Only create ISO if BUILD_TYPE=iso
if [ "$BUILD_TYPE" = "iso" ]; then
    echo "Creating ISO..."
    mkdir -p isodir/boot/grub
    cp "$KERNEL" isodir/boot/aprisma.kernel

    cat > isodir/boot/grub/grub.cfg << EOF
menuentry "aprisma" {
    multiboot /boot/aprisma.kernel root=/dev/null
}
EOF

    if [ -n "$GRUBDIR" ]; then
        "$GRUBDIR/i686-elf-grub-mkrescue" -o "$ISO_OUTPUT" isodir
    else
        grub-mkrescue -o "$ISO_OUTPUT" isodir
    fi
    echo "ISO created: $ISO_OUTPUT"
fi

# ============================
# Optionally run QEMU
# ============================

if [ -n "$RUN_MODE" ]; then
    case "$RUN_MODE" in
        kernel)
            RUN_KERNEL=${RUN_PATH:-"$KERNEL"}
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


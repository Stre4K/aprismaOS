#!/bin/sh
set -e

# ============================
# AprismaOS Unified Build Script
# ============================

show_help() {
cat << EOF
Usage: $0 [OPTIONS]

Options:
  -d,  --default             Use default build
  --compiler COMPILER        Select cross-compiler (i686-elf-gcc or x86_64-elf-gcc, default: i686-elf-gcc)
  --kernel KERNEL_PATH       Kernel path for ISO (default: build/AprismaOS/sysroot/boot/aprisma.kernel)
  --output ISO_PATH          Output ISO filename (default: aprisma.iso)
  --homebrew-grub            Use homebrew GRUB instead of system default (REDACTED)
  --grub-path PATH           Path to GRUB binaries
  --clean                    Clean previous build (build/, sysroot, ISO)
  --build [iso|kernel]       Build ISO (default) or only kernel
  --gen-compile-db           Generate compile_commands.json
  -r, --run [kernel|iso]     Run QEMU after building
  -rd, --run-debug           Run QEMU in debugger mode [kernel|iso]
  --run-path PATH            Path to kernel or ISO when using --run
  --gdb                      Run gdb and connect to QEMU
  -h, --help                 Show this help message
EOF
}

# ============================
# Defaults
# ============================
DEBUGGER=i686-elf-gdb
COMPILER=i686-elf-gcc
#ROOTDIR=$(pwd)                   # top-level project directory
ROOTDIR="$(cd "$(dirname "$0")" && pwd)"
SYSROOT=$ROOTDIR/build/AprismaOS/sysroot
OBJ_DIR=$ROOTDIR/build/objects
ISO_OUTPUT=$ROOTDIR/build/aprisma.iso
GRUBDIR=
RUN_MODE=
RUN_PATH=
QEMU_GDB_DEBUG=
GDB=0
BUILD_TYPE=""  # empty means no build
CLEAN=0
MAKE=${MAKE:-make}

SYSTEM_HEADER_PROJECTS="libc kernel"
PROJECTS="libc kernel"
KERNEL_PATH="$SYSROOT/boot/aprisma.kernel"

# ============================
# Colors
# ============================
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
BLUE='\033[0;34m'
RED='\033[0;31m'
GREEN='\033[0;32m'
GREEN_BOLD='\033[1;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

# ============================
# Functions
# ============================
detect_grub() {
    CUSTOM_PATH="$1"

    # If user supplied a path
    if [ -n "$CUSTOM_PATH" ]; then
        if [ -x "$CUSTOM_PATH/i686-elf-grub-mkrescue" ]; then
            GRUB_MKRESCUE="$CUSTOM_PATH/i686-elf-grub-mkrescue"
            return
        elif [ -x "$CUSTOM_PATH/grub-mkrescue" ]; then
            GRUB_MKRESCUE="$CUSTOM_PATH/grub-mkrescue"
            return
        else
            echo "${RED}[Error] No grub-mkrescue found in $CUSTOM_PATH${NC}"
            exit 1
        fi
    fi

    # Automatic detection
    if command -v i686-elf-grub-mkrescue >/dev/null 2>&1; then
        GRUB_MKRESCUE=$(command -v i686-elf-grub-mkrescue)
    elif command -v grub-mkrescue >/dev/null 2>&1; then
        GRUB_MKRESCUE=$(command -v grub-mkrescue)
    elif command -v brew >/dev/null 2>&1; then
        GRUB_MKRESCUE="$(brew --prefix)/bin/i686-elf-grub-mkrescue"
    else
        echo "${RED}[Error] grub-mkrescue not found.${NC}"
        exit 1
    fi
    echo "${CYAN}[tool] Using GRUB: $GRUB_MKRESCUE${NC}"
}

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
        -d|--default)
            BUILD_TYPE="iso"
            GEN_COMPILE_DB=1
            shift
            ;;
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
        -r|--run)
            RUN_MODE="$2"
            shift 2
            ;;
        --run-path)
            RUN_PATH="$2"
            shift 2
            ;;
        -rd|--run-debug)
            RUN_MODE="$2"
            QEMU_GDB_DEBUG="-S -s"
            shift 2
            ;;
        --gdb)
            GDB=1
            shift
            ;;
        #--homebrew-grub)
        #    GRUBDIR=/opt/homebrew/Cellar/i686-elf-grub/2.12/bin
        #    shift
        #    ;;
        --grub-path)
            detect_grub "$2"
            shift 2
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --gen-compile-db)
            GEN_COMPILE_DB=1
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
# Clean previous build
# ============================
if [ "$CLEAN" = "1" ]; then
    echo "${YELLOW}[status] Cleaning previous build${NC}"
    rm -rf $ROOTDIR/build #build/AprismaOS build/objects "$ISO_OUTPUT"
    #    for PROJECT in $PROJECTS; do
    #        (cd "$ROOTDIR/src/$PROJECT" && \
    #            ROOTDIR="$ROOTDIR" \
    #            SYSROOT="$SYSROOT" \
    #            OBJ_DIR="$OBJ_DIR" \
    #            $MAKE clean)
    #    done
    echo "${GREEN_BOLD}[status] Clean complete${NC}"
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
    [ -z "$GRUB_MKRESCUE" ] && detect_grub
    echo "${YELLOW}[status] Creating ISO${NC}"
    ISO_DIR=$ROOTDIR/build/isodir
    mkdir -p "$ISO_DIR/boot/grub"
    cp "$KERNEL_PATH" "$ISO_DIR/boot/aprisma.kernel"

    cat > "$ISO_DIR/boot/grub/grub.cfg" << EOF
menuentry "aprisma" {
    multiboot /boot/aprisma.kernel root=/dev/null
}
EOF

    #if [ -n "$GRUBDIR" ]; then
    #    "$GRUBDIR/i686-elf-grub-mkrescue" -o "$ISO_OUTPUT" "$ISO_DIR" --quiet
    #else
    #    grub-mkrescue -o "$ISO_OUTPUT" "$ISO_DIR" --quiet
    #fi
    "$GRUB_MKRESCUE" -o "$ISO_OUTPUT" "$ISO_DIR" --quiet
    echo "${GREEN_BOLD}[status] ISO created: $ISO_DIR/aprisma.iso${NC}"
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
            qemu-system-i386 -cdrom "$RUN_ISO" -boot d -monitor stdio $QEMU_GDB_DEBUG
            ;;
        *)
            echo "Unknown run mode: $RUN_MODE"
            exit 1
            ;;
    esac
fi

# ============================
# Run GDB on qemu
# ============================
if [ "$GDB" = "1" ]; then
    echo "${YELLOW}[status] Connecting to qemu${NC}"
    $DEBUGGER $KERNEL_PATH -ex "target remote :1234" # Run gdb on qemu
    #    done
    echo "${GREEN_BOLD}[status] Connected with gdb${NC}"
        exit 0
    fi



# ============================
# Generate compile_commands.json
# ============================
if [ "$GEN_COMPILE_DB" = "1" ]; then
    # ============================
    # Manual compile_commands.json generator for AprismaOS
    # ============================

    #ROOTDIR=$(pwd)
    OBJ_DIR="$ROOTDIR/build/objects"
    SYSROOT="$ROOTDIR/build/AprismaOS/sysroot"
    CC="i686-elf-gcc"

    # Common flags
    CFLAGS="-O2 -g -ffreestanding -Wall -Wextra -nostdlib -nostdinc -Werror"
    #KERNEL_CPPFLAGS="-D__is_kernel -Isrc/kernel/include -I$SYSROOT/include"
    #LIBC_CPPFLAGS="-D__is_libc -Isrc/libc/include -I$SYSROOT/include"

    COMMON_INCLUDES="-Isrc/kernel/include -Isrc/libc/include"
    KERNEL_CPPFLAGS="-D__is_kernel $COMMON_INCLUDES"
    LIBC_CPPFLAGS="-D__is_libc $COMMON_INCLUDES"

    OUT_JSON="$ROOTDIR/compile_commands.json"

    # Clean previous
    rm -f "$OUT_JSON"
    mkdir -p "$OBJ_DIR/kernel" "$OBJ_DIR/libc"

    echo "[" > "$OUT_JSON"
    first=1

    add_entry() {
        file="$1"
        dir="$2"
        cmd="$3"

        [ $first -eq 1 ] || echo "," >> "$OUT_JSON"
        first=0
        jq -n --arg d "$dir" --arg c "$cmd" --arg f "$file" '{directory:$d,command:$c,file:$f}' >> "$OUT_JSON"
    }

    # -------------------------
    # Kernel entries
    # -------------------------
    echo "${BLUE}[kernel] Generating compile_commands.json${NC}"
    #KERNEL_SRCS=$(find src/kernel/core src/kernel/arch/i386 -type f \( -name '*.c' -o -name '*.S' \))
    KERNEL_SRCS=$(find $ROOTDIR/src/kernel/ -type f \( -name '*.c' -o -name '*.S' \))
    for f in $KERNEL_SRCS; do
        rel="${f#$ROOTDIR/src/kernel/}"
        obj="$OBJ_DIR/kernel/${rel%.*}.o"
        #obj="$OBJ_DIR/kernel/${rel%.S}.o"
        mkdir -p "$(dirname "$obj")"

        #if [[ "$f" == *.c ]]; then
        #    cmd="$CC $CFLAGS $KERNEL_CPPFLAGS -std=gnu11 -c $f -o $obj"
        #else
        #    cmd="$CC $CFLAGS $KERNEL_CPPFLAGS -c $f -o $obj"
        #fi

        case "$f" in
            *.c)
                cmd="$CC $CFLAGS $KERNEL_CPPFLAGS -std=gnu11 -c $f -o $obj"
                ;;
            *)
                cmd="$CC $CFLAGS $KERNEL_CPPFLAGS -c $f -o $obj"
                ;;
        esac
        add_entry "$f" "$ROOTDIR" "$cmd"
    done

    # -------------------------
    # Libc entries
    # -------------------------
    echo "${MAGENTA}[libc] Generating compile_commands.json${NC}"
    LIBC_SRCS=$(find $ROOTDIR/src/libc -type f -name '*.c')
    for f in $LIBC_SRCS; do
        rel="${f#$ROOTDIR/src/libc/}"
        obj="$OBJ_DIR/libc/${rel%.c}.o"
        mkdir -p "$(dirname "$obj")"
        cmd="$CC $CFLAGS $LIBC_CPPFLAGS -std=gnu11 -c $f -o $obj"
        add_entry "$f" "$ROOTDIR" "$cmd"
    done

    echo "]" >> "$OUT_JSON"
    echo "${GREEN_BOLD}[status] compile_commands.json generated at $OUT_JSON"

    exit 0
fi



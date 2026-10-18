#!/bin/sh
# Build the disk image QEMU attaches as the first IDE drive: a FAT32
# filesystem (no partition table) with the files of src/disk/, a
# numbers.txt spread over many clusters, and, given the ramdisk, a copy of
# its /bin/hello so a program can be started from the disk.
#
#   scripts/make_disk.sh OUT.img [initrd.tar]
#
# Needs mkfs.fat (dosfstools) and mtools.
set -e

OUT=$1
INITRD=${2:-}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SIZE_MB=64  # FAT32 needs at least about 33 MiB; the file is sparse

if [ -z "$OUT" ]; then
    echo "usage: $0 OUT.img [initrd.tar]" >&2
    exit 1
fi
for tool in mkfs.fat mcopy mmd; do
    if ! command -v "$tool" >/dev/null 2>&1 && ! [ -x "/usr/sbin/$tool" ] && ! [ -x "/sbin/$tool" ]; then
        echo "$0: $tool not found (install dosfstools and mtools)" >&2
        exit 2
    fi
done
PATH="$PATH:/usr/sbin:/sbin"

# FAT stores local times; this OS reads them as UTC, so write them as UTC
export TZ=UTC MTOOLS_SKIP_CHECK=1

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

rm -f "$OUT"
dd if=/dev/zero of="$OUT" bs=1048576 count=0 seek=$SIZE_MB 2>/dev/null
mkfs.fat -F 32 -n APRISMA "$OUT" >/dev/null

mcopy -s -m -i "$OUT" "$ROOT"/src/disk/* ::/
seq 1 5000 > "$TMP/numbers.txt"
mcopy -m -i "$OUT" "$TMP/numbers.txt" ::/

if [ -n "$INITRD" ] && tar -xf "$INITRD" -C "$TMP" ./bin/hello 2>/dev/null; then
    mmd -i "$OUT" ::/bin
    mcopy -m -i "$OUT" "$TMP/bin/hello" ::/bin/hello
fi

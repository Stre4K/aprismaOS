#!/bin/sh
# FAT images for test_fat.c: OUT_DIR/fat16.img and fat32.img with the same
# files, which are also left in OUT_DIR/src to compare against. Besides
# plain files there are long names, lowercase and mixed-case names, an
# empty file, a big file over many clusters, nested directories, a known
# date, a deleted entry and a fragmented file.
set -e
OUT=$1
PATH="$PATH:/usr/sbin:/sbin"
export TZ=UTC MTOOLS_SKIP_CHECK=1

rm -rf "$OUT"
mkdir -p "$OUT/src/sub/deeper" "$OUT/extra"
S=$OUT/src
printf 'hello\n' > "$S/hello.txt"
touch -d "2001-02-03 04:05:06" "$S/hello.txt"
printf 'lower\n' > "$S/lower.txt"
printf 'upper\n' > "$S/UPPER.TXT"
printf 'mixed\n' > "$S/MixedCase.Txt"
printf 'long\n' > "$S/A long file name with many characters.txt"
: > "$S/empty.txt"
seq 1 40000 > "$S/big.txt"
printf 'deep\n' > "$S/sub/deeper/note.txt"
# For fragmentation: a, then b, remove a, then frag fills a's hole and more
seq 1 1200 > "$OUT/extra/a.txt"
printf 'b\n' > "$OUT/extra/b.txt"
seq 1 5000 > "$OUT/extra/frag.txt"

make_image() {  # image size_mb mkfs-options
    dd if=/dev/zero of="$1" bs=1048576 count=0 seek="$2" 2>/dev/null
    shift 2
    mkfs.fat "$@" -n TESTDISK "$IMG" >/dev/null
}

for bits in 16 32; do
    IMG=$OUT/fat$bits.img
    if [ $bits = 16 ]; then make_image "$IMG" 16 -F 16 -s 4; else make_image "$IMG" 40 -F 32 -s 1; fi
    if [ $bits = 32 ]; then
        # Fill the first 34 MiB, so the files land above cluster 65535 and
        # need the high half of FAT32's cluster numbers
        dd if=/dev/zero of="$OUT/extra/filler" bs=1048576 count=34 2>/dev/null
        mcopy -i "$IMG" "$OUT/extra/filler" ::/
    fi
    mcopy -s -m -i "$IMG" "$S"/* ::/
    mcopy -i "$IMG" "$OUT/extra/a.txt" "$OUT/extra/b.txt" ::/
    mdel -i "$IMG" ::/a.txt
    mcopy -i "$IMG" "$OUT/extra/frag.txt" ::/
    # Removed last, so its entries stay behind marked deleted
    mcopy -i "$IMG" "$OUT/extra/b.txt" "::/Removed with a long name.txt"
    mdel -i "$IMG" "::/Removed with a long name.txt"
    if [ $bits = 32 ]; then mdel -i "$IMG" ::/filler; fi
done
rm -f "$OUT/extra/filler"
cp "$OUT/extra/frag.txt" "$S/frag.txt"
cp "$OUT/extra/b.txt" "$S/b.txt"

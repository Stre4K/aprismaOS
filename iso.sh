#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/aprisma.kernel isodir/boot/aprisma.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "aprisma" {
	multiboot /boot/aprisma.kernel
}
EOF
grub-mkrescue -o aprisma.iso isodir

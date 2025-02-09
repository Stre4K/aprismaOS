#!/bin/sh

KernelPATH=$PWD/sysroot/boot/

qemu-system-i386 -kernel $KernelPATH/aprisma.kernel

#!/bin/bash

mkdir -p build

# Build the standard library
nasm stdlib/loader.asm -f elf -o build/stdlib-loader.o
gcc -m32 -ffreestanding -c stdlib/printf.c -o build/stdlib-printf.o
ar rcs build/libstd-tom.a build/stdlib-printf.o

# Build sample application
gcc -m32 -ffreestanding -I. -c sample/main.c -o build/sample-main.o
ld -o build/sample.elf -m elf_i386 build/stdlib-loader.o build/sample-main.o build/libstd-tom.a

# Build bootstrap kernel
gcc -m32 -ffreestanding -c bootstrap-kernel/kernel.c -o build/bootstrap-kernel.o

# Assemble final kernel
nasm bootstrap-kernel/kernel-entry.asm -f elf -o build/kernel-entry.o
ld -o build/bootstrap-kernel.bin -m elf_i386 -Ttext 0x8000 --oformat binary build/kernel-entry.o build/bootstrap-kernel.o

nasm bootsector/boot.asm -f bin -o build/bootsector.bin

cat build/bootsector.bin build/bootstrap-kernel.bin > build/image.bin
# Pad to 16384 bytes (32 sectors)
truncate -s 16384 build/image.bin
cat build/sample.elf >> build/image.bin
# Pad another 4096 bytes (8 sectors)
truncate -s 20480 build/image.bin

rm -f boot.vhd
VBoxManage convertfromraw ./build/image.bin boot.vhd --format VHD --uuid="{8241d56f-3c7e-4907-9db2-0d05ac2430ce}"

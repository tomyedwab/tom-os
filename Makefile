KERNEL_FILES=gdt.c ports.c screen.c kprintf.c heap.c pic.c interrupt.c keyboard.c ata.c filesystem.c vmm.c process.c elf.c syscall.c memcpy.c stream.c kernel.c
KERNEL_OBJECTS=$(patsubst %.c, build/bootstrap-kernel/%.o, $(KERNEL_FILES))

all: vm

build/%.o: %.c
	mkdir -p `dirname $@`
	gcc -g -Os -m32 -I. -I./include -ffreestanding -c $< -o $@

# Bootloader stage 1
output/bootloader-stage1.bin: bootloader/stage1.asm
	mkdir -p output
	nasm bootloader/stage1.asm -f bin -o $@

# Bootloader stage 2
output/bootloader-stage2.bin: bootloader/stage2-entry.asm build/bootstrap-kernel/screen.o build/bootstrap-kernel/ports.o build/bootstrap-kernel/ata.o build/tomfs/tomfs.o build/bootloader/stage2.o
	mkdir -p output
	nasm bootloader/stage2-entry.asm -f elf -o build/bootloader/stage2-entry.o
	ld -o $@ -m elf_i386 -Ttext 0x8000 --oformat binary build/bootloader/stage2-entry.o build/bootstrap-kernel/screen.o build/bootstrap-kernel/ports.o build/bootstrap-kernel/ata.o build/tomfs/tomfs.o build/bootloader/stage2.o

# Stream library test suite
output/streamlib_test: streamlib/streams.c streamlib/test.c
	mkdir -p output
	gcc -I./include -o $@ $+

# Bootstrap kernel
output/bootstrap-kernel.bin: $(KERNEL_OBJECTS) bootstrap-kernel/kernel-entry.asm build/streamlib/streams.o build/tomfs/tomfs.o
	mkdir -p output
	nasm bootstrap-kernel/kernel-entry.asm -f elf -o build/bootstrap-kernel/kernel-entry.o
	ld -o output/bootstrap-kernel.elf -m elf_i386 -Ttext 0x10000 build/bootstrap-kernel/kernel-entry.o $(KERNEL_OBJECTS) build/streamlib/streams.o build/tomfs/tomfs.o
	ld --entry=main -o $@ -m elf_i386 -Ttext 0x10000 --oformat binary build/bootstrap-kernel/kernel-entry.o $(KERNEL_OBJECTS) build/streamlib/streams.o build/tomfs/tomfs.o

# Standard library
output/libstd-tom.a: build/stdlib/init.o build/stdlib/printf.o build/stdlib/random.o build/stdlib/memcpy.o build/stdlib/process.o
	mkdir -p output
	ar rcs $@ build/stdlib/init.o build/stdlib/printf.o build/stdlib/random.o build/stdlib/memcpy.o build/stdlib/process.o

build/stdlib/loader.o: stdlib/loader.asm
	nasm stdlib/loader.asm -f elf -o build/stdlib/loader.o

# Init application
output/init.elf: build/sample/init.o output/libstd-tom.a build/streamlib/streams.o build/stdlib/loader.o
	ld --entry=__init -o $@ -m elf_i386 build/stdlib/loader.o build/sample/init.o output/libstd-tom.a build/streamlib/streams.o

# Sample application
output/snake.elf: build/sample/snake.o output/libstd-tom.a build/streamlib/streams.o build/stdlib/loader.o
	ld --entry=__init -o $@ -m elf_i386 build/stdlib/loader.o build/sample/snake.o output/libstd-tom.a build/streamlib/streams.o

# TomFS test suite
output/tomfs_test: tomfs/tomfs.c tomfs/tomfs_test.c
	gcc -I./include -o $@ $+

# TomFS make_fs utility
output/tomfs_make_fs: tomfs/tomfs.c tomfs/make_fs.c
	gcc -I./include -o $@ $+

# TomFS FUSE driver
output/tomfs_fuse: tomfs/tomfs.c tomfs/fuse.c
	mkdir -p output
	gcc -g -I./include -D_FILE_OFFSET_BITS=64 -o $@ $+ -lfuse

# Filesystem
output/filesystem.img: output/tomfs_make_fs output/tomfs_fuse output/init.elf output/snake.elf output/bootstrap-kernel.bin
	mkdir -p mnt
	rm -f output/filesystem.img.tmp
	output/tomfs_make_fs output/filesystem.img.tmp
	output/tomfs_fuse -o file=output/filesystem.img.tmp mnt
	sleep 1
	cp output/bootstrap-kernel.bin mnt/kernel
	sleep 1
	mkdir -p mnt/bin
	sleep 1
	cp output/init.elf mnt/bin/init.elf
	cp output/snake.elf mnt/bin/snake.elf
	sleep 1
	fusermount -z -u mnt
	mv output/filesystem.img.tmp output/filesystem.img

# Complete image
image: output/bootloader-stage1.bin output/bootloader-stage2.bin output/filesystem.img output/libstd-tom.a output/snake.elf output/init.elf
	cat output/bootloader-stage1.bin output/bootloader-stage2.bin > output/image.bin
	# Pad to 17408 bytes (34 sectors)
	truncate -s 17408 output/image.bin
	cat output/filesystem.img >> output/image.bin
	# Pad to 10813440 bytes (66 cylinders x 10 heads x 32 sectors)
	truncate -s 10813440 output/image.bin

# Disassembly
disasm: output/bootstrap-kernel.bin output/snake.elf
	ndisasm -b 32 -o 0x10000 output/bootstrap-kernel.bin > output/bootstrap-kernel.bin.as
	ndisasm -b 32 -e 160 -o 0x080480A0 output/snake.elf > output/snake.elf.as
	readelf -s output/bootstrap-kernel.elf > output/bootstrap-kernel.syms
	readelf -s output/snake.elf > output/snake.syms

vm: image disasm
	rm -f boot.vhd
	VBoxManage convertfromraw ./output/image.bin boot.vhd --format VHD --uuid="{8241d56f-3c7e-4907-9db2-0d05ac2430ce}"

# Unit tests
test: output/tomfs_test output/streamlib_test
	output/tomfs_test
	output/streamlib_test

clean:
	rm -rf build output boot.vhd

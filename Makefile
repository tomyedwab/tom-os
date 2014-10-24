KERNEL_FILES=gdt.c ports.c screen.c heap.c pic.c interrupt.c keyboard.c ata.c vmm.c process.c elf.c syscall.c memcpy.c stream.c kernel.c
KERNEL_OBJECTS=$(patsubst %.c, build/bootstrap-kernel/%.o, $(KERNEL_FILES))

build/%.o: %.c
	mkdir -p `dirname $@`
	gcc -m32 -I. -I./include -ffreestanding -c $< -o $@

# Bootloader
output/bootsector.bin: bootsector/boot.asm
	mkdir -p output
	nasm bootsector/boot.asm -f bin -o output/bootsector.bin

# Stream library
output/libstream.a: build/streamlib/write.o build/streamlib/read.o
	mkdir -p output
	ar rcs $@ build/streamlib/write.o build/streamlib/read.o

# Bootstrap kernel
output/bootstrap-kernel.bin: $(KERNEL_OBJECTS) bootstrap-kernel/kernel-entry.asm output/libstream.a
	mkdir -p output
	nasm bootstrap-kernel/kernel-entry.asm -f elf -o build/bootstrap-kernel/kernel-entry.o
	ld -o $@ -m elf_i386 -Ttext 0x8000 --oformat binary build/bootstrap-kernel/kernel-entry.o $(KERNEL_OBJECTS) output/libstream.a

# Standard library
output/libstd-tom.a: build/stdlib/init.o build/stdlib/printf.o build/stdlib/memcpy.o
	mkdir -p output
	ar rcs $@ build/stdlib/init.o build/stdlib/printf.o build/stdlib/memcpy.o

build/stdlib/loader.o: stdlib/loader.asm
	nasm stdlib/loader.asm -f elf -o build/stdlib/loader.o

# Sample application
output/sample.elf: build/sample/main.o output/libstd-tom.a output/libstream.a build/stdlib/loader.o
	ld -o $@ -m elf_i386 build/stdlib/loader.o build/sample/main.o output/libstd-tom.a output/libstream.a

# Complete image
image: output/bootsector.bin output/bootstrap-kernel.bin output/libstd-tom.a output/sample.elf
	cat output/bootsector.bin output/bootstrap-kernel.bin > output/image.bin
	# Pad to 17408 bytes (34 sectors)
	truncate -s 17408 output/image.bin
	cat output/sample.elf >> output/image.bin
	# Pad another 10240 bytes (20 sectors) for kernel
	truncate -s 27648 output/image.bin

# Disassembly
disasm: output/bootstrap-kernel.bin output/sample.elf
	ndisasm -b 32 -o 0x8000 output/bootstrap-kernel.bin > output/bootstrap-kernel.bin.as
	ndisasm -b 32 -e 128 -o 0x08048080 output/sample.elf > output/sample.elf.as

vm: image disasm
	rm -f boot.vhd
	VBoxManage convertfromraw ./output/image.bin boot.vhd --format VHD --uuid="{8241d56f-3c7e-4907-9db2-0d05ac2430ce}"

clean:
	rm -rf build output boot.vhd
